#include "remapping.h"

BankStat::BankStat(UInt32 id) 
	: _bank_id(id), _logical_id(id), _physical_id(id), _remap_id(id)
{
	_valid = true;
	_disabled = false;
	_combined = false;
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
	//_physical_id = target_bank->_physical_id;
	_valid = false;
	_disabled = false;
	_combined = true;

	if (target_bank->_combined)
		std::cout << "[Error] Combine with a already combined bank!\n";
	if (target_bank->_remap_id != target_bank->_physical_id)
		std::cout << "[Error] The bank's remap_id has been set!\n";

	target_bank->_combined = true;
	target_bank->_remap_id = _physical_id;
	
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

	_tot_banks = _n_vaults * _n_banks;
	
	for (UInt32 i = 0; i < _tot_banks; i++) {
		struct PhyBank phy_bank;
		phy_bank._logical_bank = i;
		phy_bank._temperature = 0;
		phy_bank._valid = true;
		phy_bank._hot_access = phy_bank._cool_access = phy_bank._remap_access = 0;
		_phy_banks.push_back(phy_bank);

		BankStat* bank_stat = new BankStat(i);
		_bank_stat.push_back(bank_stat);
	}
}

RemappingManager::~RemappingManager()
{
	long tot_hot_access = 0, tot_cool_access = 0, tot_remap_access = 0;
	for (UInt32 i = 0; i < _tot_banks; i++) {
		tot_hot_access += _phy_banks[i]._hot_access;
		tot_cool_access += _phy_banks[i]._cool_access;
		tot_remap_access += _phy_banks[i]._remap_access;
		delete _bank_stat[i];
	}
	printf("----[REMAP OUTPUT]---\n");
	printf("*****HotAccess: %ld\n*****CoolAccess: %ld\n*****RemapAccess: %ld\n",
			tot_hot_access, tot_cool_access, tot_remap_access);
	printf("*****SingleDisableTime: %d\n*****DoubleDisableTime: %d\n*****RemapTime: %d\n*****RecoveryTime: %d\n",
			disable_times, double_disable_times, remap_times, recovery_times);
	printf("---------------------\n");
}

void 
RemappingManager::setRemapConfig(UInt32 n_remap, bool inter_vault, UInt32 high_thres, UInt32 dangerous_thres, UInt32 remap_thres, UInt32 init_temp)
{
	_n_remap = n_remap;
	_inter_vault = inter_vault;
	// high temperature threshold -> remap
	_high_thres = high_thres;
	_dangerous_thres = dangerous_thres;
	// safe temperature threshold -> deremap
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
	UInt32 log_bank = bank->_logical_id;
	UInt32 remap_bank = bank->_remap_id;

	_phy_banks[phy_bank]._valid = true;

	if (_n_remap == 0) {
		bank->setDisabled(false);
	} else if (_n_remap == 1) {
		bank->setId(bank_id, bank_id);
		bank->setDisabled(false);
		if (bank_id != log_bank) {
			BankStat* target_bank = _bank_stat[log_bank];
			bank->_combined = false;

			target_bank->_combined = false;
			if (target_bank->_remap_id != bank_id)
				std::cout << "[Error] This bank is not combined with current bank!\n";
			target_bank->_remap_id = target_bank->_physical_id;
		} else if (remap_bank != bank_id) {
			BankStat* r_bank = _bank_stat[remap_bank];
			r_bank->setDisabled(false);
		}
	} else if (_n_remap == 2) {
		bank->setDisabled(false);
	} else {
		std::cout << "[Error] unrecognized remap policy!\n";
	}
}

void
RemappingManager::resetStats(bool reset)
{
	for (UInt32 bank_id = 0; bank_id < _tot_banks; bank_id++) {
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
		return false;
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
	return bank->_valid;
}

bool
RemappingManager::checkDisabled(UInt32 v, UInt32 b, UInt32 r)
{
	UInt32 bank_id = getBankId(v, b);
	BankStat* bank = _bank_stat[bank_id];
	return bank->_disabled;
}

void
RemappingManager::accessRow(UInt32 v, UInt32 b, UInt32 r)
{
	UInt32 bank_id = getBankId(v, b);
	BankStat* bank = _bank_stat[bank_id];
	UInt32 phy_bank = bank->_physical_id;
	if (this->_m_dram_perf_cntlr->remapped == false) {
		if (_phy_banks[phy_bank]._temperature >= _high_thres) {
			_phy_banks[phy_bank]._hot_access ++;
			//std::cout << "lalalalalalal there is a fault!\n";
		} else { 
			_phy_banks[phy_bank]._cool_access ++;
		}
	} else {
		if (_phy_banks[phy_bank]._temperature >= _high_thres)
			_phy_banks[phy_bank]._remap_access ++;
		else
			_phy_banks[phy_bank]._cool_access ++;
	}
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
	UInt32 hot_banks = 0, cool_banks = 0, remap_banks = 0;

	for (UInt32 bank_id = 0; bank_id < _tot_banks; bank_id++) {
		BankStat* bank = _bank_stat[bank_id];
		UInt32 physical_bank = bank->_physical_id, logical_bank = bank->_logical_id;
		double bank_temp = _phy_banks[physical_bank]._temperature;

		if (_n_remap == 0) {
			if (bank_temp < _remap_thres && bank->_disabled) {
				cool_banks ++;

				recovery_times ++;

				//printf("^^Coolbank! We can enable it! ID(%d), TEMP(%.3lf)\n", bank_id, bank_temp);
				// if the bank become cooler, then enable it
				resetBank(bank_id);
			}
			if (bank_temp >= _high_thres && !bank->_disabled) {
				hot_banks ++;

				disable_times ++;
			//	printf("^^Hotbank! Disable it! ID(%d), TEMP(%.3lf)\n", bank_id, bank_temp);

				// if the bank become hot, disable it
				bank->setDisabled(true);
				bank->setValid(false);
				_phy_banks[physical_bank]._valid = false;
			}
		} else if (_n_remap == 1) {
			if (bank_temp < _remap_thres && (logical_bank != bank_id || bank->_disabled)) {

				//if ((logical_bank != bank_id) || (bank->_disabled)) {
				//[DUBUG]
				//std::cout << "Enable: " << bank_id << ", " << physical_bank << ", " << logical_bank << std::endl;
				recovery_times ++;

					cool_banks++;
					resetBank(bank_id);
				//}
			}
			bool flag = false;
			if (bank_temp >= _high_thres && bank->_combined == false && bank->_disabled == false) {
				hot_banks++;

				//printf("^^Hotbank! Disable it! ID(%d), TEMP(%.3lf)\n", bank_id, bank_temp);
				// this hot bank has not been remapped yet
				UInt32 target = bank_id;
				double min_temp = 1000.0;
				UInt32 vault_id = bank_id / _n_banks;

				UInt32 begin_i = vault_id, end_i = vault_id + _n_banks;
				// check if we want a global remapping
				if (_inter_vault) {
					begin_i = 0; end_i = _tot_banks;
				}
				
				for (UInt32 j = begin_i; j < end_i; j++) {
					// Here we randomly choose a target to remap
					UInt32 log_bank = _phy_banks[j]._logical_bank;
					if (_bank_stat[log_bank]->_combined || _bank_stat[log_bank]->_disabled) continue;

					if (_phy_banks[j]._temperature < _remap_thres) {
						if (_phy_banks[j]._temperature < min_temp) {
							target = log_bank;
							min_temp = _phy_banks[j]._temperature;
						}
					}
				}
				if (target == bank_id) {
					flag = false;
				} else {
					// [DEBUG]
					remap_times++;
					//std::cout << "Remapp " << bank->_physical_id << " to " << target << std::endl;
					remap_banks ++;
					flag = true;
					BankStat* target_bank = _bank_stat[target];
					bank->combineWith(target_bank);
					_phy_banks[target]._valid = false;
				}
			}
			/*TODO: 
			 * 1. combined == false && disable == false: just disable the bank
			 * 2. combined == true && disable == false: 
			 *		a. logical_id == physical_id: disable two banks (the bank remapped here)
			 *		b. logical_id != physical_id: pass
			 * 3. combined == false && disable == true: pass
			 * 4. combined == true && disable == true: impossible? (depends on how we handle 2.a) 
			 */
			if (bank_temp >= _high_thres && flag == false) {
				/* If we failed to remap, just disable */
				if (bank->_disabled == false) {
					if (bank->_combined == true) {
						if (logical_bank == physical_bank) {
							BankStat* remap_bank = _bank_stat[bank->_remap_id];
							// One bank with a bank remapped to it

							// [DEBUG]
							//std::cout << "Disable combined: " << bank->_physical_id << ", " << bank->_remap_id << std::endl;
							double_disable_times++;

							// set remapped bank disabled
							remap_bank->setDisabled(true);
							// set itself disabled
							bank->setDisabled(true);
							bank->setValid(false);
							_phy_banks[physical_bank]._valid = false;
						} else {
							// already remapped to another bank
							// hope that bank is cool...
						}
					} else {
						//std::cout << "[Debug] Failed to find a target!\n";
							// [DEBUG]
							//std::cout << "Disable single: " << bank->_logical_id << std::endl;
						disable_times++;

						bank->setDisabled(true);
						bank->setValid(false);
						_phy_banks[physical_bank]._valid = false;
					}
				} else {
					// already a disable bank
				}
			}
		} else if (_n_remap == 2) {
		} else {
			std::cout << "[Error] unrecognized policy!\n";
		}
	}
	//std::cout << "-----and we found " << hot_banks << " hot banks!\n";
	//std::cout << "-----and we remap " << remap_banks << " hot banks!\n";
	//std::cout << "-----and we enabled " << cool_banks << " banks!\n";
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

	UInt32 new_idx = log_bank * _n_rows + *r;

	splitId(new_idx, v, b, r);
}
