#include "remapping.h"

RemappingTable::RemappingTable(UInt32 vaults, UInt32 banks, UInt32 rows) :
	n_vaults(vaults),
	n_banks(banks),
	n_rows(rows)
{
	n_entries = n_vaults * n_banks * n_rows;
	m_table = new struct remapping_entry[n_entries];
	for (UInt32 i = 0; i < n_entries; i++) {
		m_table[i].phy = m_table[i].log = i;
		m_table[i].valid = true;
		m_table[i].migrated = false;
	}
}

RemappingTable::~RemappingTable()
{
	delete[] m_table;
}

void
RemappingTable::remapRow(UInt32 src, UInt32 des, bool invalid)
{
	UInt32 src_phy = m_table[src].phy, des_log = m_table[des].log;
	m_table[src].phy = des;
	m_table[des].log = src;
	m_table[src_phy].log = des_log;
	m_table[des_log].phy = src_phy;

	if (invalid) {
		m_table[src].valid = false;
		m_table[des_log].valid = false;
	} else {
		m_table[src].migrated = true;
		m_table[des_log].migrated = true;
	}
}

/* We may not need this*/
void
RemappingTable::remapBank(UInt32 src, UInt32 des, bool invalid)
{
}

/* We may not need this*/
void
RemappingTable::remapVault(UInt32 src, UInt32 des, bool invalid)
{
/*
	UInt32 src_bank_0 = src * n_banks, des_bank_0 = des * n_banks;
	UInt32 src_phy = m_table[src_bank_0].phy / n_banks,
		des_log = m_table[des_bank_0].log / n_banks;
	for (UInt32 i = 0; i < n_banks; i++) {
		UInt32 src_bid = src * n_banks + i,
			remap_b = m_table[src_bid].phy % n_banks;
		UInt32 des_bid = des * n_banks + remap_b;
		// phy[src] . des
		m_table[src_bid].phy = des_bid;
		// log[des] . src
		m_table[des_bid].log = src_bid;

		if (invalid) {
			m_table[src_bid].valid = false;
		} else {
			m_table[src_bid].migrated = true;
		}
	}
	for (UInt32 i = 0; i < n_banks; i++) {
		UInt32 des_bid = des_log * n_banks + i,
			remap_b = m_table[des_bid].phy % n_banks;
		UInt32 src_bid = src_phy * n_banks + remap_b;
		// phy[des_log] . src_phy
		m_table[des_bid].phy = src_bid;
		// log[src_phy] . des_log
		m_table[src_bid].log = des_bid;

		if (invalid) {
			m_table[des_bid].valid = false;
		} else {
			m_table[des_bid].migrated = true;
		}
	}
*/
}

UInt32
RemappingTable::getPhyIdx(UInt32 idx)
{
	return m_table[idx].phy;
}

UInt32
RemappingTable::getLogIdx(UInt32 idx)
{
	return m_table[idx].log;
}

UInt32 
RemappingTable::getPhyBank(UInt32 idx)
{
	UInt32 r_phy = getPhyIdx(idx);
	return (r_phy / n_rows) % n_banks;
}

UInt32 RemappingTable::getLogBank(UInt32 idx)
{
	UInt32 r_log = getLogIdx(idx);
	return (r_log / n_rows) % n_banks;
}

UInt32 
RemappingTable::getPhyVault(UInt32 idx)
{
	UInt32 r_phy = getPhyIdx(idx);
	return r_phy / n_banks / n_rows;
}

UInt32 RemappingTable::getLogVault(UInt32 idx)
{
	UInt32 r_log = getLogIdx(idx);
	return r_log / n_banks / n_rows;
}

bool
RemappingTable::getValid(UInt32 idx)
{
	return m_table[idx].valid;
}

bool
RemappingTable::getMigrated(UInt32 idx)
{
	return m_table[idx].migrated;
}
//-----------------------------------------------------

StatStoreUnit::StatStoreUnit(UInt32 vaults, UInt32 banks, UInt32 rows) :
	n_vaults(vaults),
	n_banks(banks),
	n_rows(rows)
{
	n_entries = n_vaults * n_banks * n_rows;
	m_table = new struct stats_entry[n_entries];
	for (UInt32 i = 0; i < n_entries; i++) {
			m_table[i].idx = i;
			m_table[i].access_count = 0;
			m_table[i].valid = true;
			m_table[i].just_remapped = false;
			m_table[i].last_remapped = false;
	}
	vault_temperature = new UInt32[n_vaults];
	bank_temperature = new UInt32[n_vaults * n_banks];
	for (UInt32 i = 0; i < n_vaults; i++) {
		vault_temperature[i] = 0;
		for (UInt32 j = 0; j < n_banks; j++) {
			UInt32 idx = i * n_banks + j;
			bank_temperature[idx] = 0;
		}
	}
}

StatStoreUnit::~StatStoreUnit()
{
	delete [] m_table;
	delete [] vault_temperature;
}

void
StatStoreUnit::clear(UInt32 idx)
{
	m_table[idx].access_count = 0;
	m_table[idx].valid = true;
	// Here we first test all parts of DRAM only remap once
	//m_table[idx].last_remapped = m_table[idx].just_remapped;
	//m_table[idx].just_remapped = false;
}

void
StatStoreUnit::enableRemap(UInt32 idx)
{
	m_table[idx].last_remapped = m_table[idx].just_remapped;
	m_table[idx].just_remapped = false;
}

void
StatStoreUnit::swap(UInt32 x, UInt32 y)
{
	struct stats_entry temp = m_table[x];
	m_table[x] = m_table[y];
	m_table[y] = temp;
	m_table[x].just_remapped = true;
	m_table[y].just_remapped = true;
}

void
StatStoreUnit::remapRow(UInt32 src, UInt32 des)
{
	m_table[src].just_remapped = true;
	m_table[des].just_remapped = true;
}

void
StatStoreUnit::remapBank(UInt32 src, UInt32 des)
{
	//m_table[des].just_remapped = true;
}

void
StatStoreUnit::remapVault(UInt32 src, UInt32 des)
{
/*
	UInt32 src_base = src * n_banks, 
		   des_base = des * n_banks;
	for (UInt32 i = 0; i < n_banks; i++) {
		m_table[des_base + i].just_remapped = true;
	}
*/
}

void
StatStoreUnit::setControllerTemp(UInt32 idx, UInt32 x)
{
	vault_temperature[idx] = x;
}

void
StatStoreUnit::setBankTemp(UInt32 v, UInt32 b, UInt32 x)
{
	UInt32 idx = v * n_banks + b;
	bank_temperature[idx] = x;
}

UInt32
StatStoreUnit::getAccess(UInt32 idx)
{
	return m_table[idx].access_count;
}

bool
StatStoreUnit::isTooFreq(UInt32 idx)
{
	/* Here we can replace this by MEA algorithm*/
	return (m_table[idx].access_count > row_access_threshold);
}

bool
StatStoreUnit::isJustRemapped(UInt32 idx)
{
	return m_table[idx].just_remapped;
}

bool
StatStoreUnit::isLastRemapped(UInt32 idx)
{
	return m_table[idx].last_remapped;
}

UInt32
StatStoreUnit::getBankTemp(UInt32 bank_i)
{
	return bank_temperature[bank_i];
}

UInt32 
StatStoreUnit::getBankAccess(UInt32 bank_i)
{
	UInt32 rst = 0;
	UInt32 base_idx = bank_i * n_rows;
	for (UInt32 i = 0; i < n_rows; i++) {
		rst += getAccess(base_idx + i);
	}
	return rst;
}

bool
StatStoreUnit::isBankTooFreq(UInt32 bank_i)
{
	UInt32 access = getBankAccess(bank_i);
	return (access > bank_access_threshold);
}

UInt32
StatStoreUnit::getVaultAccess(UInt32 vault_i)
{
	UInt32 rst = 0;
	UInt32 base_idx = vault_i * n_banks * n_rows;
	for (UInt32 i = 0; i < n_banks * n_rows; i++) {
		rst += getAccess(base_idx + i);
	}
	return rst;
}

UInt32 StatStoreUnit::getVaultTemp(UInt32 vault_i)
{
	return vault_temperature[vault_i];
}

bool
StatStoreUnit::isVaultTooHot(UInt32 vault_i)
{
	return (getVaultTemp(vault_i) > temperature_threshold);
}

bool
StatStoreUnit::isVaultTooFreq(UInt32 vault_i)
{
	UInt32 access = getVaultAccess(vault_i);
	return (access > vault_access_threshold);
}

//----------------------------------------------------

RemappingManager::RemappingManager(StackedDramPerfUnison* dram_perf_cntlr, UInt32 p)
	: m_dram_perf_cntlr(dram_perf_cntlr),
	  policy(p)
{
	n_vaults = dram_perf_cntlr->n_vaults;
	n_banks = dram_perf_cntlr->n_banks;
	n_rows = dram_perf_cntlr->n_rows;
	m_remap_table = new RemappingTable(n_vaults, n_banks, n_rows);
	m_stat_unit = new StatStoreUnit(n_vaults, n_banks, n_rows);

	tot_access = hot_access = cool_access = 0;
	tot_access_last = hot_access_last = cool_access_last = 0;
	hits_on_hot = 0;
	hits_on_cool = 0;
	tot_remaps = 0;
	cross_remaps = 0;
	n_intervals = 0;

	mea_map.clear();
	hot_access_vec.clear();
	hit_hot_vec.clear();
	hot_remap_vec.clear();
	c_hot_access = c_hit_hot = c_hot_remap = 0;
}

RemappingManager::~RemappingManager()
{
	std::cout << "\n----------[STAT_STORE_UNIT]-----------\n";
	std::cout << "\nHot Access: " << hot_access << ", Cool Access: " << cool_access << std::endl;
	std::cout << "\nHits on previous hot rows: " << hits_on_hot << std::endl;
	std::cout << "\nHits on previous cool rows: " << hits_on_cool << std::endl;
	std::cout << "\nTotal remap times: " << tot_remaps 
		  << ", Total Inter-Vault remaps: " << cross_remaps
		  << ", Total number of intervals: " << n_intervals << std::endl;
	//std::cout << "\nHot Access: \n";
	//std::cout << getAverage(hot_access_vec);
	/*
	for (auto n_hots : hot_access_vec) {
		std::cout << n_hots << ' ';
	} */
	//std::cout << "\n\nHit on Previous Hot:\n";
	//std::cout << getAverage(hit_hot_vec);
	/*
	for (auto n_hits : hit_hot_vec) {
		std::cout << n_hits << ' ';
	} */
	//std::cout << "\n\nRemap Times:\n";
	//std::cout << getAverage(hot_remap_vec);
	/*
	for (auto n_remaps : hot_remap_vec) {
		std::cout << n_remaps << ' ';
	} */
	std::cout << std::endl;
	std::cout << "\n----------[STAT_STORE_UNIT]-----------\n";
	delete m_remap_table;
	delete m_stat_unit;
}

double
RemappingManager::getAverage(std::vector<UInt32> vec)
{
	double sum = 0, num = 0;
	if (vec.size() == 0) return 0;
	num = double(vec.size());
	for (auto item : vec) {
		sum += double(item);
	}
	return sum / num;
}

bool
RemappingManager::getPhysicalIndex(UInt32* vault_i, UInt32* bank_i, UInt32* row_i)
{
	UInt32 global_idx = translateIdx(*vault_i, *bank_i, *row_i);
	UInt32 phy_idx = m_remap_table->getPhyIdx(global_idx);
	bool valid = m_remap_table->getValid(global_idx);
	splitIdx(phy_idx, vault_i, bank_i, row_i);
	return valid;
}

void
RemappingManager::accessRow(UInt32 vault_i, UInt32 bank_i, UInt32 row_i, UInt32 req_times)
{
	/* Physical Index*/
	UInt32 idx = translateIdx(vault_i, bank_i, row_i);
	UInt32 bank_idx = vault_i * n_banks + bank_i;
	UInt32 bank_temp = m_stat_unit->getBankTemp(bank_idx);

	if (m_stat_unit->isLastRemapped(idx)){
		hits_on_hot += req_times;
		c_hit_hot += req_times;
	}
	if (bank_temp > 85 && m_stat_unit->isJustRemapped(idx)) {
		hits_on_cool += req_times;
	}

	/* Check if there is a hot row */
	if (bank_temp > 85) {
		hot_access += req_times;
		c_hot_access += req_times;
		
		if (mea_map.find(idx) != mea_map.end()) {
			mea_map[idx] ++;
		} else {
			/* Add MEA mechod */
			if (mea_map.size() < max_mea_size) {
				mea_map[idx] = 1;
			} else {
				std::vector<UInt32> deleted_keys;
				for (auto i = mea_map.begin(); i != mea_map.end(); i++) {
					i->second --;
					if (i->second == 0) {
						deleted_keys.push_back(i->first);
					}
				}
				for (auto i : deleted_keys) {
					mea_map.erase(i);
				}
			}
		}

		/* Test remapping on every hot access*/
/*
		bool cross = false;
		UInt32 src = m_remap_table->getLogIdx(idx);
		UInt32 target = findTargetInVault(src);


		if (target == INVALID_TARGET) {
			target = findTargetCrossVault(src);
			cross = true;
		}
		if (target != INVALID_TARGET) {
			issueRowRemap(src, target);
		}
*/
	} else {
		if (mea_map.find(idx) != mea_map.end()) {
			mea_map.erase(idx);
		}
		cool_access += req_times;
	}
	tot_access += req_times;

	UInt32 access = m_stat_unit->getAccess(idx);
	m_stat_unit->setAccess(idx, access + req_times);
}

/* Change idx in remapping table*/
/* Change logical idx in stat store unit*/
void
RemappingManager::issueRowRemap(UInt32 src, UInt32 des)
{
	// invalid = true: invalidation after remapping
	// invalid = false: migration after remapping
	UInt32 src_phy = m_remap_table->getPhyIdx(src);
	bool invalid = false;
	m_remap_table->remapRow(src, des, invalid);
	m_stat_unit->remapRow(src_phy, des);

	c_hot_remap ++;
}

bool
RemappingManager::checkRowStat(UInt32 v, UInt32 b, UInt32 r)
{
}

/* Return a logical index for src*/
UInt32 
RemappingManager::findHottestRow()
{
	UInt32 row_nums = n_vaults * n_banks * n_rows;
	UInt32 hottest = INVALID_TARGET;
	UInt32 max_access = m_stat_unit->row_access_threshold;

	for (UInt32 v_i = 0; v_i < n_vaults; v_i++) {
		UInt32 vault_temp = m_stat_unit->getVaultTemp(v_i);
		if (vault_temp <= m_stat_unit->temperature_threshold) continue;
		for (UInt32 b_i = 0; b_i < n_banks; b_i++) {

			UInt32 phy_bank_idx = v_i * n_banks + b_i;

			UInt32 bank_temp = m_stat_unit->getBankTemp(phy_bank_idx);

			if (bank_temp < m_stat_unit->temperature_threshold) continue;

			//UInt32 max_bank_access = 0, tot_bank_access = 0;

			for (UInt32 r_i = 0; r_i < n_rows; r_i++) {
				UInt32 phy_idx = this->translateIdx(v_i, b_i, r_i);
				UInt32 log_idx = m_remap_table->getLogIdx(phy_idx);

				bool just_remapped = m_stat_unit->isJustRemapped(phy_idx),
					 valid = m_remap_table->getValid(log_idx),
					 migrated = m_remap_table->getMigrated(log_idx);
				/* If this row is unavailable, we just skip it*/
				if (!valid || just_remapped || migrated) continue;

				UInt32 access = m_stat_unit->getAccess(phy_idx);

				//if (access > max_bank_access) max_bank_access = access;
				//tot_bank_access += access;

				if (access > max_access) {
					max_access = access;
					hottest = phy_idx;
				}
			}
			/*
			if (max_bank_access > 0) {
				std::cout << "This hot bank: " << max_bank_access << ' ' << tot_bank_access << std::endl << "**" << std::endl;
			}
			*/
		}
	}
	//std::cout << "Who is hottest? ..." << hottest << "!!\n";
	
	//std::cout << "[MEA]We found hottest row" << hottest << " with " << max_access << std::endl;

	if (hottest == INVALID_TARGET)
		return INVALID_TARGET;
	UInt32 src_idx = m_remap_table->getLogIdx(hottest);
	return src_idx;
}

UInt32
RemappingManager::findHottestRowMEA()
{
	UInt32 max_idx = INVALID_TARGET;
	UInt32 max_count = m_stat_unit->row_access_threshold;

	//std::cout << "[MEA]Current we have " << mea_map.size() << " in mea map" << std::endl;

	for (auto i = mea_map.begin(); i != mea_map.end(); i++) {
		if (m_stat_unit->isJustRemapped(i->first)) continue;
		if (i->second > max_count) {
			max_idx = i->first;
			max_count = i->second;
		}
	}
	if (max_count == m_stat_unit->row_access_threshold) {
		return INVALID_TARGET;
	}
	//std::cout << "[MEA]We found hottest row with " << max_count << std::endl;
	mea_map.erase(max_idx);
	UInt32 src_idx = m_remap_table->getLogIdx(max_idx);
	return src_idx;
}

/* Return a physical index for des*/
UInt32
RemappingManager::findTargetInVault(UInt32 src_log)
{
	UInt32 target = INVALID_TARGET;
	if (src_log == INVALID_TARGET) return INVALID_TARGET;
	UInt32 src_phy = m_remap_table->getPhyIdx(src_log);
	UInt32 v = m_remap_table->getPhyVault(src_log),
		   b = m_remap_table->getPhyBank(src_log);
	UInt32 s_lev = b / 2;
	
	UInt32 des_phy = (v + 1) * n_banks * n_rows - 1;
	UInt32 src_access = m_stat_unit->getAccess(src_phy);
	UInt32 min_access = 999999;

	while (des_phy > src_phy) {
		UInt32 des_log = m_remap_table->getLogIdx(des_phy);

		des_phy --;

		UInt32 tv = 0, tb = 0, tr = 0;
		splitIdx(des_phy, &tv, &tb, &tr);
		UInt32 d_lev = tb / 2;
		UInt32 bank_temp = m_stat_unit->getBankTemp(tv * n_banks + tb);
		bool just_remapped = m_stat_unit->isJustRemapped(des_phy),
			 valid = m_remap_table->getValid(des_log),
			 migrated = m_remap_table->getMigrated(des_log);
		UInt32 des_access = m_stat_unit->getAccess(des_phy);
		/* We also choose position higher than source and untouched */
		if (just_remapped || !valid || migrated) {
			continue;
		}

		if (d_lev > s_lev && bank_temp < m_stat_unit->temperature_threshold) {
			/* Here we only choose 0 access*/
			if (des_access == 0) {
				target = des_phy;
				break;
			}
		}
	}
	return target;
	
}

/* Return a physical index for des*/
UInt32
RemappingManager::findTargetCrossVault(UInt32 src_log)
{
	//std::cout << "[CROSS] Here we start to find target in other vaults!\n";

	UInt32 target = INVALID_TARGET;
	if (src_log == INVALID_TARGET) return INVALID_TARGET;

	UInt32 src_phy = m_remap_table->getPhyIdx(src_log);
	UInt32 src_access = m_stat_unit->getAccess(src_phy);
	for (int b_i = n_banks - 1; b_i >= 0; b_i --) {
		for (int v_i = 0; v_i < n_vaults; v_i ++) {
			UInt32 phy_bank_idx = v_i * n_banks + b_i;
			UInt32 bank_temp = m_stat_unit->getBankTemp(phy_bank_idx);
			if (bank_temp >= m_stat_unit->temperature_threshold) continue;
			for (int r_i = 0; r_i < n_rows; r_i ++) {
				UInt32 des_phy = this->translateIdx(v_i, b_i, r_i);
				UInt32 des_log = m_remap_table->getLogIdx(des_phy);
				UInt32 des_access = m_stat_unit->getAccess(des_phy);

				bool just_remapped = m_stat_unit->isJustRemapped(des_phy),
					 valid = m_remap_table->getValid(des_log),
					 migrated = m_remap_table->getMigrated(des_log);

				/* We also choose position higher than source and untouched */
				/* Here we only choose 0 access*/
				if (!just_remapped && valid && !migrated && des_access == 0) {
					target = des_phy;
					return target;
				}
			}
		}
	}
	return target;
}

UInt32
RemappingManager::tryRemapping(bool remap)
{
	/* If we have not entered ROI, just return*/

	int max_remap_times = 5; 

	if (remap == false) return 0;
	if (hot_access_last == hot_access) return 0;
	int remap_times = 0;
	while (remap_times < max_remap_times) {
		//std::cout << "hehe\n";
		//UInt32 src = findHottestRow();
		UInt32 src = findHottestRowMEA();
		//std::cout << "haha\n";

		/* Check if there is a hot row */
		bool cross = false;
		if (src == INVALID_TARGET) break;
		UInt32 target = findTargetInVault(src);
		if (target == INVALID_TARGET) {
			target = findTargetCrossVault(src);
			cross = true;
		}
		if (cross) cross_remaps++;
		/*
		UInt32 target = findTargetCrossVault(src);
		if (target == INVALID_TARGET) {
			target = findTargetInVault(src);
		}
		*/

		if (target == INVALID_TARGET)
			break;

		issueRowRemap(src, target);

		//std::cout << "[Remapping] Here we found a remap: src(" << src
		//	  << "), target(" << target << ")\n";

		remap_times ++;
	}
	/*
	std::cout << "We find " << remap_times << " hot pages" << std::endl;
	std::cout << "There are " << tot_access - tot_access_last << " access, "
			  << hot_access - hot_access_last << " hot access, "
			  << cool_access - cool_access_last << " cool access.\n";
	*/
	hot_access_last = hot_access;
	cool_access_last = cool_access;
	tot_access_last = tot_access;

	tot_remaps += remap_times;
	n_intervals ++;

	return remap_times;
}

void
RemappingManager::updateTemperature(UInt32 v, UInt32 b, UInt32 temp, UInt32 v_temp)
{
	m_stat_unit->setControllerTemp(v, v_temp);
	m_stat_unit->setBankTemp(v, b, temp);
}

void
RemappingManager::reset(UInt32 v, UInt32 b, UInt32 r)
{
	UInt32 idx = translateIdx(v, b, r);

	m_remap_table->setValid(idx, true);
	m_remap_table->setMigrated(idx, false);
	//m_stat_unit->clear(idx);
}

void
RemappingManager::resetStats()
{
	for (UInt32 i = 0; i < m_stat_unit->n_entries; i++) {
		m_stat_unit->clear(i);
	}
	mea_map.clear();
}

void
RemappingManager::finishRemapping()
{
	//hot_access_vec.push_back(c_hot_access);
	//hit_hot_vec.push_back(c_hit_hot);
	//hot_remap_vec.push_back(c_hot_remap);
	c_hot_access = c_hit_hot = c_hot_remap = 0;
	for (UInt32 i = 0; i < n_vaults; i++) {
		for (UInt32 j = 0; j < n_banks; j++) {
			for (UInt32 k = 0; k < n_rows; k++) {
				reset(i, j, k);
			}
		}
	}
}

void
RemappingManager::enableAllRemap()
{
	for (UInt32 i = 0; i < m_stat_unit->n_entries; i++) {
		m_stat_unit->enableRemap(i);
	}
}


bool
RemappingManager::checkMigrated(UInt32 v, UInt32 b, UInt32 r)
{
	UInt32 idx = translateIdx(v, b, r);
	return m_remap_table->getMigrated(idx);
}

bool
RemappingManager::checkValid(UInt32 v, UInt32 b, UInt32 r)
{
	UInt32 idx = translateIdx(v, b, r);
	return m_remap_table->getValid(idx);
}

void
RemappingManager::splitIdx(UInt32 idx, UInt32* v, UInt32* b, UInt32* r)
{
	UInt32 tmp = idx;
	*r = tmp % n_rows;
	tmp /= n_rows;
	*b = tmp % n_banks;
	*v = tmp / n_banks;
}

UInt32 
RemappingManager::translateIdx(UInt32 v, UInt32 b, UInt32 r)
{
	return v * n_banks * n_rows + b * n_rows + r;
}
