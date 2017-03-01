#include "bank_remap.h"

BankRemappingEntry::BankRemappingEntry(UInt32 idx)
{
	m_valid = true;
	m_orig = true;
	m_changed = false;
	m_level = idx / 2;
	clearStats();
	n_access = 0;
	m_idx = idx;
	m_ridx = idx;
}

BankRemappingEntry::~BankRemappingEntry()
{
}

void
BankRemappingEntry::clearStats()
{
	stats.tACT = stats.tPRE = stats.tRD = stats.tWR = SubsecondTime::Zero();
	stats.reads = stats.writes = stats.row_hits = 0;
}

void
BankRemappingEntry::clearAccess()
{
	n_access = 0;
}

void 
BankRemappingEntry::remapTo(UInt32 idx)
{
	m_ridx = idx;
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
BankRemappingEntry::getAccess()
{
	return n_access;
}

UInt32 
BankRemappingEntry::getIdx()
{
	return m_ridx;
}

SubsecondTime
BankRemappingEntry::accessOnce(DramCntlrInterface::access_t access_type)
{
	SubsecondTime latency = SubsecondTime::Zero();
	n_access ++;
	if (access_type == DramCntlrInterface::WRITE) {
		stats.writes++;
	} else {
		stats.reads++;
	}
	return latency;
}

void
BankRemappingEntry::setValidBit(bool valid)
{
	m_valid = valid;
}

/*
void
BankRemappingEntry::setChangedBit(bool changed)
{
	m_changed = changed;
}
*/

BankRemappingStructure::BankRemappingStructure(UInt32 bank_num)
{
	last_time = current_time = SubsecondTime::Zero();
	n_banks = bank_num;
	m_bremap_arr = new BankRemappingEntry*[n_banks];
	for (UInt32 i = 0; i < n_banks; i++) {
		m_bremap_arr[i] = new BankRemappingEntry(i);
	}
	copy_times = 0;
}

BankRemappingStructure::~BankRemappingStructure()
{
	delete [] m_bremap_arr;
}

SubsecondTime
BankRemappingStructure::swap(UInt32 src, UInt32 des)
{
	//printf(" now begin swapping banks %d: %d\n", src, des);
	m_bremap_arr[src]->setChangedBit(true);
	m_bremap_arr[src]->clearAccess();
	m_bremap_arr[des]->setChangedBit(true);
	m_bremap_arr[des]->clearAccess();

	bool s_orig = m_bremap_arr[src]->m_orig, d_orig = m_bremap_arr[des]->m_orig;
	UInt32 src_des = m_bremap_arr[src]->getIdx();
	UInt32 des_des = m_bremap_arr[des]->getIdx();

	//printf(" src_des: %d, des_des: %d\n", src_des, des_des);

	m_bremap_arr[src]->remapTo(des);
	if (!s_orig)
		m_bremap_arr[src_des]->remapTo(src_des);
	if (!d_orig) {
		m_bremap_arr[des_des]->remapTo(des_des);
	}
	m_bremap_arr[des]->remapTo(src);

	int data_mov = 0;
	if ((s_orig && d_orig) || (src == des)) {
		data_mov = 3;
	} else if ((s_orig && !d_orig) || (!s_orig && d_orig)) {
		data_mov = 4;
	} else if (!s_orig && !d_orig) {
		data_mov = 5;
	} else {
		printf("\tIntereting! src: %d, des: %d, src_des: %d, des_des: %d\n", 
				src, des, src_des, des_des);
	}
	copy_times += data_mov;
	return SubsecondTime::Zero();
}

UInt32
BankRemappingStructure::getBankIdx(UInt32 idx, bool *valid)
{
	UInt32 res = m_bremap_arr[idx]->getIdx();
	*valid = m_bremap_arr[idx]->m_valid;
	return res;
}

bool
BankRemappingStructure::isValid(UInt32 idx)
{
	return m_bremap_arr[idx]->m_valid;
}

bool
BankRemappingStructure::isTooHot(UInt32 phy_vault_i, UInt32 idx)
{
	bool tooHot =false;
	UInt32 cnt = m_bremap_arr[idx]->getAccess();
	bool b_changed = m_bremap_arr[idx]->getChangedBit();
	SubsecondTime interval = current_time - last_time;
	double interval_ms = double(interval.getFS()) * 1.0e-12;

	double access_rate = double(cnt) * interval_ms;

	//printf("[Bank access rate] Interval: %.2f ms, Access: %d, AccessRate: %.2f\n", 
	//		double(interval.getFS()) / 1000000000.0, cnt, access_rate);
	
	bool valid;
	UInt32 phy_bank_i = getBankIdx(idx, &valid);
	double bank_temp = Sim()->getStatsManager()->getDramBankTemp(phy_vault_i, phy_bank_i);

	double update_time = 0.01;

	if (access_rate > 1000 && interval_ms > update_time) {
		printf("---bank_%d is ACCESSED TOO FREQUENTLY! %.5f accesses in %.5f ms\n", phy_bank_i, access_rate, interval_ms);
		if (0) {
			printf("---bank_%d is about to be swapped!\n", phy_bank_i);
			tooHot = true;
		} else {
			//printf("---bank_%d was just swapped!\n", phy_bank_i);
		}
	}
	if (interval_ms > update_time + 0.1) {
		clearAllAccess();
	}
	return tooHot;
}

void
BankRemappingStructure::balanceBanks(UInt32 phy_vault_i, UInt32 idx)
{
	SubsecondTime interval = current_time - last_time;
	// set default target
	int target = 100;
	BankRemappingEntry* orig_bank = m_bremap_arr[idx];
	UInt32 orig_remap = orig_bank->m_ridx;
	double min_temp = 999, orig_temp = Sim()->getStatsManager()->getDramBankTemp(phy_vault_i, orig_remap);

	for (UInt32 i = 0; i < n_banks; i++) {
		bool valid;
		UInt32 phy_bank_i = getBankIdx(idx, &valid);
		double bank_temp = Sim()->getStatsManager()->getDramBankTemp(phy_vault_i, phy_bank_i);
		BankRemappingEntry* bank = m_bremap_arr[i];
		/* we don't want to swap bank already swapped */
		if (bank->getChangedBit()) continue;

		UInt32 cnt = bank->getAccess();
		UInt32 remap_i = bank->m_ridx;
		/* Trade-off */
		if (bank->m_changed) {
			cnt += 1000;
		}
		//if (remap_i != idx && cnt < min_c) {
		if (min_temp > bank_temp) {
			min_temp = bank_temp;
			target = remap_i;
		}
	}

	printf(" balance bank %d(%d)? temp: %.5f\n", idx, orig_remap, orig_temp);
	if (target != orig_remap && target != 100) {
		printf("------balance bank %d, balance target: %d, temp: %.5f\n", idx, target, min_temp);
		swap(idx, target);
	} else if (target == 100) {
	}
	//clearAccess();
}

void
BankRemappingStructure::setValid(UInt32 idx)
{
	m_bremap_arr[idx]->setValidBit(true);
}

void
BankRemappingStructure::setInvalid(UInt32 idx)
{
	m_bremap_arr[idx]->setValidBit(false);
}

void
BankRemappingStructure::clearAllAccess()
{
	for (UInt32 i = 0; i < n_banks; i++) {
		m_bremap_arr[i]->clearAccess();	
		m_bremap_arr[i]->setChangedBit(false);
	}
	last_time = current_time;
}

SubsecondTime
BankRemappingStructure::accessOnce(UInt32 idx, DramCntlrInterface::access_t access_type, SubsecondTime pkt_time)
{
	/* Update current time*/
	if (pkt_time > current_time)
		current_time = pkt_time;
	SubsecondTime latency = m_bremap_arr[idx]->accessOnce(access_type);
	return latency;
}
