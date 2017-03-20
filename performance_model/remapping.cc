#include "remapping.h"

RemappingTable::RemappingTable(UInt32 vaults, UInt32 banks, UInt32 rows) :
	n_vaults(vaults),
	n_banks(banks),
	n_rows(rows)
{
	n_entries = n_vaults * n_banks;
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
	}
	vault_temperature = new UInt32[n_vaults];
	for (UInt32 i = 0; i < n_vaults; i++) {
		vault_temperature[i] = 0;
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
	//m_table[idx].just_remapped = false;
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
	m_table[des].just_remapped = true;
}

void
StatStoreUnit::remapBank(UInt32 src, UInt32 des)
{
	m_table[des].just_remapped = true;
}

void
StatStoreUnit::remapVault(UInt32 src, UInt32 des)
{
	UInt32 src_base = src * n_banks, 
		   des_base = des * n_banks;
	for (UInt32 i = 0; i < n_banks; i++) {
		m_table[des_base + i].just_remapped = true;
	}
}

void
StatStoreUnit::setControllerTemp(UInt32 idx, UInt32 x)
{
	vault_temperature[idx] = x;
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
}

RemappingManager::~RemappingManager()
{
	delete m_remap_table;
	delete m_stat_unit;
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
	UInt32 access = m_stat_unit->getAccess(idx);
	m_stat_unit->setAccess(idx, access + req_times);
}

/* Change idx in remapping table*/
/* Change logical idx in stat store unit*/
void
RemappingManager::issueRowRemap(UInt32 src, UInt32 des)
{
	bool invalid = true;
	m_remap_table->remapRow(src, des, invalid);
	m_stat_unit->remapRow(src, des);
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
	for (UInt32 i = 0; i < row_nums; i++) {
		UInt32 log_idx = m_remap_table->getLogIdx(i);
		bool just_remapped = m_stat_unit->isJustRemapped(i),
			 valid = m_remap_table->getValid(log_idx);
		if (!valid || just_remapped) continue;
		UInt32 access = m_stat_unit->getAccess(i);
		if (access > max_access) {
			max_access = access;
			hottest = i;
		}
	}
	UInt32 src_idx = m_remap_table->getLogIdx(hottest);
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
	while (des_phy > src_phy) {
		UInt32 des_log = m_remap_table->getLogIdx(des_phy);
		UInt32 tv = 0, tb = 0, tr = 0;
		splitIdx(des_phy, &tv, &tb, &tr);
		UInt32 d_lev = tb / 2;
		bool just_remapped = m_stat_unit->isJustRemapped(des_phy),
			 valid = m_remap_table->getValid(des_log);
		/* We also choose position higher than source and untouched */
		if (d_lev > s_lev && !just_remapped && valid) {
			target = des_phy;
		}
		if (d_lev <= s_lev) break;
		
		des_phy --;
	}
	return target;
	
}

/* Return a physical index for des*/
UInt32
RemappingManager::findTargetCrossVault(UInt32 src_log)
{
	UInt32 target = INVALID_TARGET;
	if (src_log == INVALID_TARGET) return INVALID_TARGET;
	return target;
}

UInt32
RemappingManager::tryRemapping(bool remap)
{
	if (remap == false) return 0;
	int remap_times = 0;
	while (remap_times < max_remap_times) {
		UInt32 src = findHottestRow();
		UInt32 target = findTargetInVault(src);
		if (target == INVALID_TARGET) {
			target = findTargetCrossVault(src);
		}
		if (src == INVALID_TARGET || target == INVALID_TARGET)
			break;
		std::cout << "[REMAP_DEBUG] Find a hot row and prepare remapping!\n"
				  << "---src: " << src << "---target: " << target << std::endl;
		issueRowRemap(src, target);
		remap_times ++;
	}
	return remap_times;
}

void
RemappingManager::updateTemperature(UInt32 v, UInt32 v_temp)
{
	m_stat_unit->setControllerTemp(v, v_temp);
}

void
RemappingManager::reset(UInt32 v, UInt32 b, UInt32 r)
{
	UInt32 idx = translateIdx(v, b, r);
	m_remap_table->setValid(idx, true);
	m_remap_table->setMigrated(idx, false);
	m_stat_unit->clear(idx);
}

void
RemappingManager::resetRemapping()
{
	for (UInt32 i = 0; i < n_vaults * n_banks * n_rows; i++) {
		m_stat_unit->enableRemapping(i);
	}
}

void
RemappingManager::finishRemapping()
{
	for (UInt32 i = 0; i < n_vaults; i++) {
		for (UInt32 j = 0; j < n_banks; j++) {
			for (UInt32 k = 0; k < n_rows; k++) {
				reset(i, j, k);
			}
		}
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
