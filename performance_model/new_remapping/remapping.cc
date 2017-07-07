#include "remapping.h"

BankStat::BankStat(UInt32 id) 
	: _bank_id(id), _logical_id(id), _physical_id(id)
{
}

BankStat::~BankStat()
{
}

void
BankStat::accessRow(UInt32 row_id)
{
	/* Here we use MEA method to select hottest rows*/
	if (mea_map.find(row_id) != mea_map.end()) {
		mea_map[row_id]++;
	} else {
		if (mea_map.size() < _n_migrate_row) {
			mea_map[row_id] = 1;
		} else {
			std::vector<UInt32> deleted_keys;
			for (auto i = mea_map.begin(); i != mea_map.end(); i++) {
				i->second --;
				if (i->second == 0) {
					deleted_keys.push_back(i->first);
				}
			}
			for (auto i : deleted_keys)
				mea_map.erase(i);
		}
	}	
}

void
BankStat::remapTo(UInt32 phy_bank_id, bool disabled)
{
	_physical_id = phy_bank_id;
	_valid = false;
	_disabled = disabled;

	invalidate_times ++;
	remap_times ++;
	if (disabled) {
		disabled_times ++;
	}
}

void
BankStat::combineWith(BankStat* target_bank)
{
	_logical_id = target_bank->_logical_id;
	_physical_id = target_bank->_physical_id;
	_valid = false;
	_disabled = false;
	
	invalidate_times ++;
}

void
BankStat::migrateRow(UInt32 row_id)
{
	if (valid_rows.find(row_id) != valid_rows.end()) {
		std::cout << "[Warning] Migrate a chosen row!\n";
	}
	valid_rows.insert(row_id);
}

void
BankStat::setId(UInt32 log_id, UInt32 phy_id)
{
	_logical_id = log_id;
	_physical_id = phy_id;
}

void
BankStat::finishRemapping()
{
	valid_rows.clear();
}

//---------------------------RemappingManager-------------------------------

RemappingManager::RemappingManager(StackedDramPerfUnison* dram_perf_cntlr)
	: _m_dram_perf_cntlr (dram_perf_cntlr)
{
	_n_vaults = dram_perf_cntlr->n_vaults;
	_n_banks = dram_perf_cntlr->n_banks;
	_n_rows = dram_perf_cntlr->n_rows;
	
	for (UInt32 i = 0; i < _n_banks; i++) {
		struct PhyBank phy_bank;
		phy_bank._logical_bank = i;
		phy_bank._temperature = 0;
		phy_bank._valid = true;
		_phy_banks.push_back(phy_bank);

		BankStat* bank_stat = new BankStat(i);
		_bank_stat.push_back(bank_stat);
	}
}

RemappingManager::~RemappingManager()
{
	for (UInt32 i = 0; i < _n_banks; i++) {
		delete _bank_stat[i];
	}
}

void 
RemappingManager::setRemapConfig(UInt32 n_remap, bool inter_vault, UInt32 high_thres, UInt32 dangerous_thres, UInt32 remap_thres, UInt32 init_temp)
{
	_n_remap = n_remap;
	_inter_vault = inter_vault;
	_high_thres = high_thres;
	_dangerous_thres = dangerous_thres;
	_remap_thres = remap_thres;
	_init_temp = init_temp;
}

void
RemappingManager::updateTemperature(UInt32 v, UInt32 b, double temp)
{
	UInt32 bank_id = getBankId(v, b);
	_phy_banks[bank_id]._temperature = temp;
}

void
RemappingManager::resetBank(UInt32 bank_id)
{
	BankStat* bank = _bank_stat[bank_id];

	UInt32 phy_bank = bank->_physical_id;

	_phy_banks[phy_bank]._valid = true;

	if (_n_remap == 0) {
		bank->setDisabled(false);
	} else if (_n_remap == 1) {
		bank->setId(bank_id, bank_id);
	} else if (_n_remap == 2) {
		bank->setDisabled(false);
	} else {
		std::cout << "[Error] unrecognized remap policy!\n";
	}
}

void
RemappingManager::resetStats(bool reset)
{
	for (UInt32 bank_id = 0; bank_id < _n_vaults * _n_banks; bank_id++) {
		BankStat* bank = _bank_stat[bank_id];
		UInt32 phy_id = bank->_physical_id;
		
		bank->setValid(true);
		bank->finishRemapping();
		/* Here we reset all cool banks */
		if (reset && _phy_banks[phy_id]._temperature < _high_thres) {
			resetBank(bank_id);
		} 
	}
}

bool
RemappingManager::checkMigrated(UInt32 v, UInt32 b, UInt32 r)
{
	UInt32 bank_id = getBankId(v, b);
	BankStat* bank = _bank_stat[bank_id];
	if (bank->_valid) {
		return true;
	}
	if (bank->valid_rows.find(r) != bank->valid_rows.end()) {
		return true;
	}
	return false;
}

bool
RemappingManager::checkValid(UInt32 v, UInt32 b, UInt32 r)
{
	UInt32 bank_id = getBankId(v, b);
	BankStat* bank = _bank_stat[bank_id];
	if (bank->_valid) {
		return true;
	}
	return false;
}

bool
RemappingManager::checkDisabled(UInt32 v, UInt32 b, UInt32 r)
{
	UInt32 bank_id = getBankId(v, b);
	BankStat* bank = _bank_stat[bank_id];
	if (bank->_disabled) {
		return true;
	}
	return false;
}

void
RemappingManager::accessRow(UInt32 v, UInt32 b, UInt32 r)
{
	UInt32 bank_id = getBankId(v, b);
	BankStat* bank = _bank_stat[bank_id];
	bank->accessRow(r);
}

/* Main function of mechanism: called after temperature being updated 
 * key points:
 *   1. Manage hot bank based on policy
 *   2. Manage special rows in hot banks based on policy
 *   3. Update the state value of bank, used for cache controller to check
 */
void
RemappingManager::runMechanism()
{
	UInt32 tot_n = _n_vaults * _n_banks;
	for (UInt32 bank_id = 0; bank_id < tot_n; bank_id++) {
		BankStat* bank = _bank_stat[bank_id];
		UInt32 phy_bank = bank->_physical_id;
		double bank_temp = _phy_banks[phy_bank]._temperature;

		if (_n_remap == 0) {
			if (bank_temp < _high_thres && bank->_disabled) {
				// if the bank become cooler, then enable it
				resetBank(bank_id);
			}
			if (bank_temp >= _high_thres && !bank->_disabled) {
				// if the bank become hot, disable it
				bank->setDisabled(true);
				_phy_banks[phy_bank]._valid = false;
			}
		} else if (_n_remap == 1) {
			if (bank_temp < _high_thres && bank->_logical_id != bank_id) {
				resetBank(bank_id);
			}
			if (bank_temp >= _high_thres && bank->_logical_id == bank_id) {
				// this hot bank has not been remapped yet
				UInt32 target = bank_id, min_temp = 1000;
				UInt32 vault_id = bank_id / _n_banks;

				UInt32 begin_i = vault_id, end_i = vault_id + _n_banks;
				// check if we want a global remapping
				if (_inter_vault) {
					begin_i = 0; end_i = tot_n;
				}
				
				for (UInt32 j = begin_i; j < end_i; j++) {
					// Here we randomly choose a target to remap
					if (_phy_banks[j]._valid 
							&& _phy_banks[j]._temperature < _high_thres) {
						if (_phy_banks[j]._temperature < min_temp) {
							target = _phy_banks[j]._logical_bank;
							min_temp = _phy_banks[j]._temperature;
						}
					}
				}
				if (target == bank_id) {
					std::cout << "[Warning] cannot find a target bank!\n";
				} else {
					BankStat* target_bank = _bank_stat[target];
					bank->combineWith(target_bank);
					_phy_banks[target]._valid = false;
				}
			}
		} else if (_n_remap == 2) {
		} else {
			std::cout << "[Error] unrecognized policy!\n";
		}
	}
}

void
RemappingManager::splitId(UInt32 idx, UInt32* v, UInt32* b, UInt32* r)
{
	UInt32 tmp = idx;
	*r = tmp % _n_rows;
	tmp /= _n_rows;
	*b = tmp % _n_banks;
	*v = tmp / _n_banks;
}

UInt32
RemappingManager::getBankId(UInt32 v, UInt32 b)
{
	return v * _n_banks + b;
}

void
RemappingManager::getPhysicalIndex(UInt32* v, UInt32* b, UInt32* r)
{
	UInt32 bank_id = getBankId(*v, *b);
	BankStat* bank = _bank_stat[bank_id];
	UInt32 phy_bank = bank->_physical_id;
	splitId(phy_bank, v, b, r);
}

void
RemappingManager::getLogicalIndex(UInt32* v, UInt32* b, UInt32* r)
{
	UInt32 bank_id = getBankId(*v, *b);
	BankStat* bank = _bank_stat[bank_id];
	UInt32 log_bank = bank->_logical_id;
	splitId(log_bank, v, b, r);
}
