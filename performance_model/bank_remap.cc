#include "bank_remap.h"

BankRemappingEntry::BankRemappingEntry(UInt32 idx)
{
	m_valid = true;
	m_orig = true;
	m_level = idx / 2;
	n_count = 0;
	m_idx = idx;
	m_ridx = idx;
}

BankRemappingEntry::~BankRemappingEntry()
{
}

void 
BankRemappingEntry::remapTo(UInt32 idx)
{
	m_ridx = idx;
	n_count = 0;
	if (idx != m_idx)
		m_orig = false;
	else
		m_orig = true;
	//m_valid = false;
	setValidBit(false);
}

UInt32 
BankRemappingEntry::getCount()
{
	return n_count;
}

UInt32 
BankRemappingEntry::getIdx()
{
	return m_ridx;
}

SubsecondTime
BankRemappingEntry::accessOnce()
{
	SubsecondTime latency = SubsecondTime::Zero();
	n_count++;
	return latency;
}

void
BankRemappingEntry::setValidBit(bool valid)
{
	m_valid = valid;
}

BankRemappingStructure::BankRemappingStructure(UInt32 bank_num)
{
	n_clock = 0;
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
	printf(" now begin swapping banks %d: %d\n", src, des);

	bool s_orig = m_bremap_arr[src]->m_orig, d_orig = m_bremap_arr[des]->m_orig;
	UInt32 src_des = m_bremap_arr[src]->getIdx();
	UInt32 des_des = m_bremap_arr[des]->getIdx();

	printf(" src_des: %d, des_des: %d\n", src_des, des_des);

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
BankRemappingStructure::isTooHot(UInt32 idx)
{
	UInt32 cnt = m_bremap_arr[idx]->getCount();

	//printf("bank_%d is too hot? %d\n", idx, cnt);

	if (cnt > 100) {
		return true;
	}
	return false;
}

void
BankRemappingStructure::balanceBanks(UInt32 idx)
{
	UInt32 target = idx;
	UInt32 min_c = 99999;

	for (UInt32 i = 0; i < n_banks; i++) {
		BankRemappingEntry* bank = m_bremap_arr[i];
		UInt32 cnt = bank->getCount();
		UInt32 remap_i = bank->m_ridx;
		if (remap_i != idx && cnt < min_c) {
			min_c = cnt;
			target = remap_i;
		}
	}

	printf(" balance bank %d, balance target: %d\n", idx, target);
	swap(idx, target);
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

SubsecondTime
BankRemappingStructure::accessOnce(UInt32 idx)
{
	SubsecondTime latency = m_bremap_arr[idx]->accessOnce();
	return latency;
}
