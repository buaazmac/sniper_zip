#include "remapping.h"

RemappingTable::RemappingTable(int vaults, int banks) :
	n_vaults(vaults),
	n_banks(banks)
{
	n_entries = n_vaults * n_banks;
	m_table = new struct remapping_entry[n_entries];
	for (int i = 0; i < n_entries; i++) {
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
RemappingTable::remapBankTo(int src, int des, bool invalid)
{
	int src_phy = m_table[src].phy, des_log = m_table[des].log;
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
RemappingTable::remapVaultTo(int src, int des, bool invalid)
{
	int src_bank_0 = src * n_banks, des_bank_0 = des * n_banks;
	int src_phy = m_table[src_bank_0].phy / n_banks,
		des_log = m_table[des_bank_0].log / n_banks;
	for (int i = 0; i < n_banks; i++) {
		int src_bid = src * n_banks + i,
			remap_b = m_table[src_bid].phy % n_banks;
		int des_bid = des * n_banks + remap_b;
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
	for (int i = 0; i < n_banks; i++) {
		int des_bid = des_log * n_banks + i,
			remap_b = m_table[des_bid].phy % n_banks;
		int src_bid = src_phy * n_banks + remap_b;
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

int
RemappingTable::getPhyIdx(int idx)
{
	return m_table[idx].phy;
}

int
RemappingTable::getLogIdx(int idx)
{
	return m_table[idx].log;
}

bool
RemappingTable::getValid(int idx)
{
	return m_table[idx].valid;
}
//-----------------------------------------------------

StatStoreUnit::StatStoreUnit(int vaults, int banks) :
	n_vaults(vaults),
	n_banks(banks)
{
	n_entries = n_vaults * n_banks;
	m_table = new struct stats_entry[n_entries];
	for (int i = 0; i < n_entries; i++) {
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
StatStoreUnit::clear(int idx)
{
	m_table[idx].access_count = 0;
	m_table[idx].data_cov = 0;
	m_table[idx].valid = true;
	m_table[idx].too_hot = false;
	m_table[idx].just_remapped = false;
}

void
StatStoreUnit::swap(int x, int y)
{
	struct stats_entry temp = m_table[x];
	m_table[x] = m_table[y];
	m_table[y] = temp;
	m_table[x].just_remapped = true;
	m_table[y].just_remapped = true;
}

void
StatStoreUnit::setTemp(int idx, int x)
{
	m_table[idx].temperature = x;
	m_table[idx].too_hot = (x > temperature_threshold);
}

void
StatStoreUnit::setControllerTemp(int idx, int x)
{
	m_table[idx].cntlr_temp = x;
}

int
StatStoreUnit::getAccess(int idx)
{
	return m_table[idx].access_count;
}

int 
StatStoreUnit::getDataCov(int idx) 
{
	return m_table[idx].data_cov;
}

int
StatStoreUnit::getTemp(int idx)
{
	return m_table[idx].temperature;
}

bool
StatStoreUnit::isTooHot(int idx)
{
	return m_table[idx].too_hot;
}

bool
StatStoreUnit::isTooFreq(int idx)
{
	return (m_table[idx].access_count > bank_access_threshold);
}

bool
StatStoreUnit::isJustRemapped(int idx)
{
	return m_table[idx].just_remapped;
}

int
StatStoreUnit::getVaultAccess(int vault_i)
{
	int rst = 0;
	int base_idx = vault_i * n_banks;
	for (int i = 0; i < n_banks; i++) {
		rst += getAccess(base_idx + i);
	}
	return rst;
}

int
StatStoreUnit::getVaultDataCov(int vault_i)
{
	int rst = 0;
	int base_idx = vault_i * n_banks;
	for (int i = 0; i < n_banks; i++) {
		rst += getDataCov(base_idx + i);
	}
	return rst;
}

int
StatStoreUnit::getVaultTemp(int vault_i)
{
	int base_idx = vault_i * n_banks;
	return m_table[base_idx].cntlr_temp;
}

bool
StatStoreUnit::isVaultTooHot(int vault_i)
{
	return (getVaultTemp(vault_i) > temperature_threshold);
}

bool
StatStoreUnit::isVaultTooFreq(int vault_i)
{
	int access = getVaultAccess(vault);
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
RemappingManager::getPhysicalIndex(int* vault_i, int* bank_i)
{
	int global_idx = *vault_i * n_banks + *bank_i;
	int phy_idx = m_remap_table->getPhyIdx(global_idx);
	bool valid = m_remap_table->getValid(global_idx);
	*vault_i = phy_idx / n_banks;
	*bank_i = phy_idx % n_banks;
	return valid;
}

void
RemappingManager::accessRow(int vault_i, int bank_i, int row_i, int req_times)
{
	int idx = vault_i * n_banks + bank_i;
	int access = m_stat_unit->getAccess(idx);
	int data_cov = m_stat_unit->getDataCov(idx);
	m_stat_unit->setAccess(idx, access + req_times);
	int part_id = (row_i >> 8) % 32;
	data_cov |= ((1 << part_id) - 1);
	m_stat_unit->setDataCov(idx, data_cov);
}

int
RemappingManager::findBankTarget()
{
	if (policy == 1 || policy == 3) {
	} else {
	}
	return 0;
}

int
RemappingManager::findVaultTarget()
{
	return 0;
}

/* Change idx in remapping table*/
/* Change logical idx in stat store unit*/
void 
RemappingManager::issueBankRemap(int src, int des)
{
	bool invalid = false;
	if (policy == 1 || policy == 2)
		invalid = true;
}

void 
RemappingManager::issueVaultRemap(int src, int des)
{
	bool invalid = false;
	if (policy == 1 || policy == 2)
		invalid = true;
}

bool 
RemappingManager::checkBankStat(int v, int b)
{
	int idx = v * n_banks + b;
	int bank_access = m_stat_unit->getAccess(idx),
		bank_data_cov = m_stat_unit->getDataCov(idx);

	int unit_temp = m_stat_unit->getTemp(idx);

	bool bank_hot = m_stat_unit->isTooHot(idx),
		 bank_freq = m_stat_unit->isTooFreq(idx),
		 just_remapped = m_stat_unit->isJustRemapped(idx);
	/* We should not remap a part of DRAM was just remapped */
	if (just_remapped) return false;

	/* Here is our trigger function for bank remapping*/
	if (bank_freq) {
	}
}

bool
RemappingManager::checkVaultStat(int v, int b)
{
	int idx = v * n_banks + b;
	int vault_access = m_stat_unit->getVaultAccess(v),
		vault_data_cov = m_stat_unit->getVaultDataCov(v);

	int cntlr_temp = m_stat_unit->getVaultTemp(v);

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
RemappingManager::checkStat(int v, int b, bool remap)
{
	/*TODO: 1. Here we get the statistics
	 *		2. Decide whether to remap based on algorithm
	 *		3. Issue Remap based on policy
	 */
	bool vault_hot = checkVaultStat(v, b),
		 bank_hot = checkBankStat(v, b);

	bool remap_result = false;

	if (policy == 1) 
	{
		if (vault_hot) {
			int phy_vault_target = findVaultTarget();
			issueVaultRemap(v, phy_vault_target);
		} else if (bank_hot) {
			int phy_bank_target = findBankTarget();
			issueBankRemap(idx, phy_bank_target);
		}
	}
	else if (policy == 2) 
	{
		if (bank_hot) {
			int phy_bank_target = findBankTarget();
			issueBankRemap(idx, phy_bank_target);
		}
	}
	else if (policy == 3)
	{
		if (vault_hot) {
			int phy_vault_target = findVaultTarget();
			issueVaultRemap(v, phy_vault_target);
		} else if (bank_hot) {
			int phy_bank_target = findBankTarget();
			issueBankRemap(idx, phy_bank_target);
		}
	}
	else
	{
		if (bank_hot) {
			int phy_bank_target = findBankTarget();
			issueBankRemap(idx, phy_bank_target);
		}
	}
	return false;
}

void
RemappingManager::handleRequest(int v, int b)
{
}

void
RemappingManager::updateTemperature(int v, int b, int temperature, int v_temp)
{
	int idx = v * n_banks + b;
	m_stat_unit->setTemp(idx, temperature);
	m_stat_unit->setControllerTemp(idx, v_temp);
}

void
RemappingManager::reset(int v, int b)
{
	int idx = v * n_banks + b;
	m_remap_table->setValid(idx, true);
	m_remap_table->setMigrated(idx, false);
	m_stat_unit->clear(idx);
}

bool
RemappingManager::checkMigrated(int v, int b)
{
	int idx = v * n_banks + b;
	return false;
}
