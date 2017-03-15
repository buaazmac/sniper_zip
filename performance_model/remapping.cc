#include "remapping.h"

RemappingTable::RemappingTable(UInt32 vaults, UInt32 banks) :
	n_vaults(vaults),
	n_banks(banks)
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
RemappingTable::remapBankTo(UInt32 src, UInt32 des, bool invalid)
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

void
RemappingTable::remapVaultTo(UInt32 src, UInt32 des, bool invalid)
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

StatStoreUnit::StatStoreUnit(UInt32 vaults, UInt32 banks) :
	n_vaults(vaults),
	n_banks(banks)
{
	n_entries = n_vaults * n_banks;
	m_table = new struct stats_entry[n_entries];
	for (UInt32 i = 0; i < n_entries; i++) {
			m_table[i].idx = i;
			m_table[i].access_count =m_table[i].data_cov = 0;
			m_table[i].valid = true;
			m_table[i].too_hot = false;
			m_table[i].just_remapped = false;
	}
}

StatStoreUnit::~StatStoreUnit()
{
	delete [] m_table;
}

void
StatStoreUnit::clear(UInt32 idx)
{
	m_table[idx].access_count = 0;
	m_table[idx].data_cov = 0;
	m_table[idx].valid = true;
	m_table[idx].too_hot = false;
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
StatStoreUnit::setTemp(UInt32 idx, UInt32 x)
{
	m_table[idx].temperature = x;
	m_table[idx].too_hot = (x > temperature_threshold);
}

void
StatStoreUnit::setControllerTemp(UInt32 idx, UInt32 x)
{
	m_table[idx].cntlr_temp = x;
}

UInt32
StatStoreUnit::getLogIdx(UInt32 idx)
{
	return m_table[idx].idx;
}

UInt32
StatStoreUnit::getAccess(UInt32 idx)
{
	return m_table[idx].access_count;
}

UInt32 
StatStoreUnit::getDataCov(UInt32 idx) 
{
	return m_table[idx].data_cov;
}

UInt32
StatStoreUnit::getTemp(UInt32 idx)
{
	return m_table[idx].temperature;
}

bool
StatStoreUnit::isTooHot(UInt32 idx)
{
	return m_table[idx].too_hot;
}

bool
StatStoreUnit::isTooFreq(UInt32 idx)
{
	return (m_table[idx].access_count > bank_access_threshold);
}

bool
StatStoreUnit::isJustRemapped(UInt32 idx)
{
	return m_table[idx].just_remapped;
}

UInt32
StatStoreUnit::getVaultAccess(UInt32 vault_i)
{
	UInt32 rst = 0;
	UInt32 base_idx = vault_i * n_banks;
	for (UInt32 i = 0; i < n_banks; i++) {
		rst += getAccess(base_idx + i);
	}
	return rst;
}

UInt32
StatStoreUnit::getVaultDataCov(UInt32 vault_i)
{
	UInt32 rst = 0;
	UInt32 base_idx = vault_i * n_banks;
	for (UInt32 i = 0; i < n_banks; i++) {
		rst += getDataCov(base_idx + i);
	}
	return rst;
}

UInt32 StatStoreUnit::getVaultTemp(UInt32 vault_i)
{
	UInt32 base_idx = vault_i * n_banks;
	return m_table[base_idx].cntlr_temp;
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

RemappingManager::RemappingManager(StackedDramPerfUnison* dram_perf_cntlr)
	: m_dram_perf_cntlr(dram_perf_cntlr)
{
	n_vaults = dram_perf_cntlr->n_vaults;
	n_banks = dram_perf_cntlr->n_banks;
	m_remap_table = new RemappingTable(n_vaults, n_banks);
	m_stat_unit = new StatStoreUnit(n_vaults, n_banks);
}

RemappingManager::~RemappingManager()
{
	delete m_remap_table;
	delete m_stat_unit;
}

bool
RemappingManager::getPhysicalIndex(UInt32* vault_i, UInt32* bank_i)
{
	UInt32 global_idx = *vault_i * n_banks + *bank_i;
	UInt32 phy_idx = m_remap_table->getPhyIdx(global_idx);
	bool valid = m_remap_table->getValid(global_idx);
	*vault_i = phy_idx / n_banks;
	*bank_i = phy_idx % n_banks;
	return valid;
}

void
RemappingManager::accessRow(UInt32 vault_i, UInt32 bank_i, UInt32 row_i, UInt32 req_times)
{
	UInt32 idx = vault_i * n_banks + bank_i;
	UInt32 access = m_stat_unit->getAccess(idx);
	UInt32 data_cov = m_stat_unit->getDataCov(idx);
	m_stat_unit->setAccess(idx, access + req_times);
	UInt32 part_id = (row_i >> 8) % 32;
	data_cov |= ((1 << part_id) - 1);
	m_stat_unit->setDataCov(idx, data_cov);
}

UInt32
RemappingManager::findBankTarget(UInt32 idx)
{
	UInt32 v = idx / n_banks;
	UInt32 base = v * n_banks;
	UInt32 target = INVALID_TARGET;
	if (policy == 1 || policy == 3) {
		UInt32 max_access = 0;
		for (UInt32 i = 0; i < n_banks; i++) {
			UInt32 bank_i = base + i;
			UInt32 ac = m_stat_unit->getAccess(bank_i);
			if (m_remap_table->getLogIdx(bank_i) != idx && ac > max_access && 
				m_stat_unit->isJustRemapped(bank_i) == false) {
				max_access = ac;
				target = bank_i;
			}
		}		
	} else {
		UInt32 max_access = 0;
		for (UInt32 i = 0; i < n_vaults * n_banks; i++) {
			UInt32 ac = m_stat_unit->getAccess(i);
			if (m_remap_table->getLogIdx(i) != idx && ac > max_access 
				&& m_stat_unit->isJustRemapped(i) == false) {
				max_access = ac;
				target = i;
			}
		}
	}
	return target;
}

UInt32
RemappingManager::findVaultTarget(UInt32 vault_i)
{
	UInt32 target = INVALID_TARGET;
	UInt32 max_access = 0;
	for (UInt32 i = 0; i < n_vaults; i++) {
		UInt32 base_idx = i * n_banks;
		UInt32 log_v = m_remap_table->getLogIdx(base_idx) / n_banks;
		UInt32 ac = m_stat_unit->getVaultAccess(i);

		if (log_v != vault_i && ac > max_access 
			&& m_stat_unit->isJustRemapped(base_idx) == false) {
			max_access = ac;
			target = i;
		}
	}
	return target;
}

/* Change idx in remapping table*/
/* Change logical idx in stat store unit*/
void 
RemappingManager::issueBankRemap(UInt32 src, UInt32 des)
{
	bool invalid = false;
	if (policy == 1 || policy == 2)
		invalid = true;
	m_remap_table->remapBankTo(src, des, invalid);
	m_stat_unit->remapBank(src, des);
}

void 
RemappingManager::issueVaultRemap(UInt32 src, UInt32 des)
{
	bool invalid = false;
	if (policy == 1 || policy == 2)
		invalid = true;
	m_remap_table->remapVaultTo(src, des, invalid);
	m_stat_unit->remapVault(src, des);
}

bool 
RemappingManager::checkBankStat(UInt32 v, UInt32 b)
{
	UInt32 idx = v * n_banks + b;
	UInt32 bank_access = m_stat_unit->getAccess(idx),
		bank_data_cov = m_stat_unit->getDataCov(idx);

	UInt32 unit_temp = m_stat_unit->getTemp(idx);

	bool bank_hot = m_stat_unit->isTooHot(idx),
		 bank_freq = m_stat_unit->isTooFreq(idx),
		 just_remapped = m_stat_unit->isJustRemapped(idx);
	/* We should not remap a part of DRAM was just remapped */
	if (just_remapped) return false;

	/* Here is our trigger function for bank remapping*/
	if (bank_freq) {
		return true;
	}
	return false;
}

bool
RemappingManager::checkVaultStat(UInt32 v, UInt32 b)
{
	UInt32 idx = v * n_banks + b;
	UInt32 vault_access = m_stat_unit->getVaultAccess(v),
		vault_data_cov = m_stat_unit->getVaultDataCov(v);

	UInt32 cntlr_temp = m_stat_unit->getVaultTemp(v);

	bool vault_hot = m_stat_unit->isVaultTooHot(v),
		 vault_freq = m_stat_unit->isVaultTooFreq(v),
		 just_remapped = m_stat_unit->isJustRemapped(idx);
	/* We should not remap a part of DRAM was just remapped */
	if (just_remapped) return false;

	/* Here is our trigger function for vault remapping*/
	if (vault_hot | vault_freq) {
		return true;
	}
	return false;
}

bool
RemappingManager::checkStat(UInt32 v, UInt32 b, bool remap)
{
	/*TODO: 1. Here we get the statistics
	 *		2. Decide whether to remap based on algorithm
	 *		3. Issue Remap based on policy
	 */
	UInt32 idx = v * n_banks + b;
	bool vault_hot = checkVaultStat(v, b),
		 bank_hot = checkBankStat(v, b);

	bool remap_result = false;

	if (policy == 1) 
	{
		if (vault_hot) {
			UInt32 phy_vault_target = findVaultTarget(v);
			issueVaultRemap(v, phy_vault_target);
#define REMAP_LOG
#ifdef REMAP_LOG
			std::cout << "[REMAP_MAN] Here we remap vault " << v << std::endl
					 << "===Target is: " << phy_vault_target << std::endl;
#endif
		} else if (bank_hot) {
			UInt32 phy_bank_target = findBankTarget(idx);
			issueBankRemap(idx, phy_bank_target);
#define REMAP_LOG
#ifdef REMAP_LOG
			std::cout << "[REMAP_MAN] Here we remap bank " << idx << std::endl
					 << "===Target is: " << phy_bank_target << std::endl;
#endif
		}
	}
	else if (policy == 2) 
	{
		if (bank_hot) {
			UInt32 phy_bank_target = findBankTarget(idx);
			issueBankRemap(idx, phy_bank_target);
		}
	}
	else if (policy == 3)
	{
		if (vault_hot) {
			UInt32 phy_vault_target = findVaultTarget(v);
			issueVaultRemap(v, phy_vault_target);
		} else if (bank_hot) {
			UInt32 phy_bank_target = findBankTarget(idx);
			issueBankRemap(idx, phy_bank_target);
		}
	}
	else
	{
		if (bank_hot) {
			UInt32 phy_bank_target = findBankTarget(idx);
			issueBankRemap(idx, phy_bank_target);
		}
	}
	return false;
}

void
RemappingManager::handleRequest(UInt32 v, UInt32 b)
{
}

void
RemappingManager::updateTemperature(UInt32 v, UInt32 b, UInt32 temperature, UInt32 v_temp)
{
	UInt32 idx = v * n_banks + b;
	m_stat_unit->setTemp(idx, temperature);
	m_stat_unit->setControllerTemp(idx, v_temp);
}

void
RemappingManager::reset(UInt32 v, UInt32 b)
{
	UInt32 idx = v * n_banks + b;
	m_remap_table->setValid(idx, true);
	m_remap_table->setMigrated(idx, false);
	m_stat_unit->clear(idx);
}

void
RemappingManager::finishRemapping()
{
	for (UInt32 i = 0; i < n_vaults; i++) {
		for (UInt32 j = 0; j < n_banks; j++) {
			reset(i, j);
		}
	}
}


bool
RemappingManager::checkMigrated(UInt32 v, UInt32 b)
{
	UInt32 idx = v * n_banks + b;
	return m_remap_table->getMigrated(idx);
}
