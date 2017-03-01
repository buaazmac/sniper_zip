#include "vault_remap.h"

VaultRemappingEntry::VaultRemappingEntry(UInt32 idx)
{
	m_bank_num = 8;

	m_valid = true;
	m_orig = true;
	m_changed = false;
	m_idx = idx;
	m_ridx = idx; // point to itself

	clearStats();

	n_access = 0;

	m_bremap_table = new BankRemappingStructure(m_bank_num);
}

VaultRemappingEntry::~VaultRemappingEntry()
{
	delete m_bremap_table;
}

void
VaultRemappingEntry::clearStats()
{
	stats.tACT = stats.tPRE = stats.tRD = stats.tWR = SubsecondTime::Zero();
	stats.reads = stats.writes = stats.row_hits = 0;
}

void
VaultRemappingEntry::clearAccess()
{
	n_access = 0;
	m_bremap_table->clearAllAccess();
}

void
VaultRemappingEntry::remapTo(UInt32 idx)
{
	m_ridx = idx;
	//reset the access count
	clearStats();
	if (idx != m_idx)
		m_orig = false;
	else
		m_orig = true;
	//m_valid = false;
	setValidBit(false);
	//setChangedBit(true);
}

UInt32
VaultRemappingEntry::getAccess()
{
	return n_access;
}

UInt32
VaultRemappingEntry::getIdx()
{
	return m_ridx;
}

SubsecondTime
VaultRemappingEntry::accessOnce(UInt32 bank_idx, DramCntlrInterface::access_t access_type, SubsecondTime pkt_time)
{
	SubsecondTime latency = SubsecondTime::Zero();
	n_access ++;
	if (access_type == DramCntlrInterface::WRITE) {
		stats.writes++;
	} else {
		stats.reads++;
	}

	m_bremap_table->accessOnce(bank_idx, access_type, pkt_time);

	return latency;
}

void
VaultRemappingEntry::setValidBit(bool valid)
{
	m_valid = valid;
	for (UInt32 i = 0; i < m_bank_num; i++) {
		if (valid) {
			m_bremap_table->setValid(i);
		} else {
			m_bremap_table->setInvalid(i);
		}
	}
}

bool
VaultRemappingEntry::isTooHot(UInt32 phy_vault_i, UInt32 bank_idx)
{
	return m_bremap_table->isTooHot(phy_vault_i, bank_idx);
}

void
VaultRemappingEntry::balanceBanks(UInt32 phy_vault_i, UInt32 bank_idx)
{
	m_bremap_table->balanceBanks(phy_vault_i, bank_idx);
}

int
VaultRemappingEntry::getBankValid()
{
	int res = (1 << m_bank_num);
	for (UInt32 i = 0; i < m_bank_num; i++) {
		int tmp = 1 << i;
		if (m_bremap_table->isValid(i)) {
			res |= tmp;
		}
	}
	return res;
}

VaultRemappingStructure::VaultRemappingStructure(UInt32 vault_num)
{
	n_clock = 0;
	n_vaults = vault_num;
	m_vremap_arr = new VaultRemappingEntry*[n_vaults];
	for (UInt32 i = 0; i < n_vaults; i++) {
		m_vremap_arr[i] = new VaultRemappingEntry(i);
	}
	copy_times = 0;
}

VaultRemappingStructure::~VaultRemappingStructure()
{
	delete [] m_vremap_arr;
}

SubsecondTime
VaultRemappingStructure::swap(UInt32 src, UInt32 des)
{
	//printf(" now begin swapping %d: %d\n", src, des);
	m_vremap_arr[src]->setChangedBit(true);
	m_vremap_arr[src]->clearAccess();
	m_vremap_arr[des]->setChangedBit(true);
	m_vremap_arr[des]->clearAccess();

	bool s_orig = m_vremap_arr[src]->m_orig, d_orig = m_vremap_arr[des]->m_orig;
	UInt32 src_des = m_vremap_arr[src]->getIdx();
	UInt32 des_des = m_vremap_arr[des]->getIdx();

	//printf(" src_des: %d, des_des: %d\n", src_des, des_des);
	// remap src to des
	m_vremap_arr[src]->remapTo(des);
	// reset src's original destination
	if (!s_orig)
		m_vremap_arr[src_des]->remapTo(src_des);
	// remap des to src
	if (!d_orig) {
		m_vremap_arr[des_des]->remapTo(des_des);
	}
	m_vremap_arr[des]->remapTo(src);

	int data_mov = 0;
	if ((s_orig && d_orig) || (src == des)) {
		data_mov = 3;
	} else if ((s_orig && !d_orig) || (!s_orig && d_orig)) {
		data_mov = 4;
	} else if (!s_orig && !d_orig) {
		data_mov = 5;
	} else {
		printf("\tInteresting! src: %d, des: %d, src_des: %d, des_des: %d\n",
				src, des, src_des, des_des);
	}
	copy_times += data_mov;
	return SubsecondTime::Zero();
}

UInt32
VaultRemappingStructure::getVaultIdx(UInt32 idx, bool *valid)
{
	UInt32 res = m_vremap_arr[idx]->getIdx();
	*valid = m_vremap_arr[idx]->m_valid;
	return res;
}

UInt32
VaultRemappingStructure::getBankIdx(UInt32 vault_idx, UInt32 bank_idx, bool *valid)
{
	VaultRemappingEntry *vault = m_vremap_arr[vault_idx];
	UInt32 bank_ridx = vault->m_bremap_table->getBankIdx(bank_idx, valid);
	return bank_ridx;
}

bool
VaultRemappingStructure::isValid(UInt32 idx, int *bank_valid)
{
	bool vault_valid = m_vremap_arr[idx]->m_valid;

	*bank_valid = m_vremap_arr[idx]->getBankValid();

	return vault_valid;
}

int
VaultRemappingStructure::getBankValid(UInt32 vault_idx)
{
	return m_vremap_arr[vault_idx]->getBankValid();
}

bool
VaultRemappingStructure::isTooHot(UInt32 idx, UInt32 bank_i)
{
	bool valid;
	bool tooHot = false;
	UInt32 phy_vault_i = getVaultIdx(idx, &valid);
	bool bankTooHot = m_vremap_arr[idx]->isTooHot(phy_vault_i, bank_i);
	/* Get temperature data*/
	double cntlr_temp = Sim()->getStatsManager()->getDramCntlrTemp(phy_vault_i);

	if (bankTooHot) {
		printf("---vault: %d blance bank: %d", idx, bank_i);
		m_vremap_arr[idx]->balanceBanks(phy_vault_i, bank_i);
	}
	UInt32 cnt = m_vremap_arr[idx]->getAccess();
	bool v_changed = m_vremap_arr[idx]->getChangedBit();
	/* Calculate Access Rate*/
	SubsecondTime interval = current_time - last_time;
	double interval_ms = double(interval.getFS()) * 1.0e-12;

	double access_rate = double(cnt) / interval_ms;
	printf("[Vault access rate] Interval: %.2f ms, Access: %d\n", 
			interval_ms, cnt);

	double update_time = 0.01;

	if (access_rate > 80000 && interval_ms > update_time) {
		printf("---vault_%d is ACCESSED TOO FREQUENTLY! %.5f in %.5f ms\n", phy_vault_i, access_rate, interval_ms);
		// time to remap
		if (0) {
			printf("---vault_%d is about to be swapped!\n", phy_vault_i);
			tooHot = true;
		} else {
			//printf("---vault_%d was just swapped!\n", phy_vault_i);
		}
	}
	if (interval_ms > update_time * 2) {
		clearAllAccess();
	}
	return tooHot;
}

void
VaultRemappingStructure::balanceVaults(UInt32 idx)
{
	SubsecondTime interval = current_time - last_time;
	// set default target
	UInt32 target = 100;
	VaultRemappingEntry* orig_vault = m_vremap_arr[idx];
	UInt32 orig_remap = orig_vault->m_ridx;
	double min_temp = 999, orig_temp = Sim()->getStatsManager()->getDramCntlrTemp(orig_remap);
	int min_cnt = 9999999;

	for (UInt32 i = 0; i < n_vaults; i++) {
		VaultRemappingEntry* vault = m_vremap_arr[i];
		
		/* we don't want to swap vault already swapped */
		if (vault->getChangedBit()) continue;

		UInt32 cnt = vault->getAccess();
		UInt32 remap_i = vault->m_ridx;
		double cntlr_temp = Sim()->getStatsManager()->getDramCntlrTemp(remap_i);
		/* Trade-off */
		/*
		if (vault->m_changed) {
			cnt += 10000;
		}*/
		//if (remap_i != idx && cnt < min_c) {
		/*
		if (min_temp > cntlr_temp) {
			min_temp = cntlr_temp;
			target = remap_i;
		}
		*/
		if (cnt < min_cnt && cntlr_temp < orig_temp + 5) {
			min_cnt = cnt;
			min_temp = cntlr_temp;
			target = remap_i;
		}
	}

	printf(" balance vault %d(%d)? temp: %.5f\n", idx, orig_remap, orig_temp);
	if (target != orig_remap && target != 100) {
		printf("-----balance vault %d, balance target: %d, temp: %.5f\n", idx, target, min_temp);
		swap(idx, target);
	} else if (target == 100) {
		printf(" It seems that all vaults have been remapped!");
		enableAllVault();
	}
	//clearAccess();
}

void 
VaultRemappingStructure::setValid(UInt32 idx)
{
	m_vremap_arr[idx]->setValidBit(true);
}

void
VaultRemappingStructure::setInvalid(UInt32 idx)
{
	m_vremap_arr[idx]->setValidBit(false);
}

void
VaultRemappingStructure::clearAllAccess()
{
	for (UInt32 i = 0; i < n_vaults; i++) {
		m_vremap_arr[i]->clearAccess();
		//m_vremap_arr[i]->setChangedBit(false);
	}
	last_time = current_time;
}

void
VaultRemappingStructure::enableAllVault()
{
	for (UInt32 i = 0; i < n_vaults; i++) {
		m_vremap_arr[i]->setChangedBit(false);
	}
}

int
VaultRemappingStructure::getUsableVaultNum()
{
	int num = 0;
	for (UInt32 i = 0; i < n_vaults; i++) {
		if (!m_vremap_arr[i]->getChangedBit()) {
			num ++;
		}
	}
	return num;
}

SubsecondTime
VaultRemappingStructure::accessOnce(UInt32 vault_idx, UInt32 bank_idx, DramCntlrInterface::access_t access_type, SubsecondTime pkt_time)
{
	/* Update current time*/
	if (pkt_time > current_time)
		current_time = pkt_time;
	SubsecondTime access_time  = m_vremap_arr[vault_idx]->accessOnce(bank_idx, access_type, pkt_time);

	return access_time;
}
