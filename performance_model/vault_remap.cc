#include "vault_remap.h"

VaultRemappingEntry::VaultRemappingEntry(UInt32 idx)
{
	m_bank_num = 8;

	m_valid = true;
	m_orig = true;
	n_count = 0;
	m_idx = idx;
	m_ridx = idx; // point to itself

	m_bremap_table = new BankRemappingStructure(m_bank_num);
}

VaultRemappingEntry::~VaultRemappingEntry()
{
	delete m_bremap_table;
}

void
VaultRemappingEntry::remapTo(UInt32 idx)
{
	m_ridx = idx;
	//reset the access count
	n_count = 0;
	if (idx != m_idx)
		m_orig = false;
	else
		m_orig = true;
	//m_valid = false;
	setValidBit(false);
}

UInt32
VaultRemappingEntry::getCount()
{
	return n_count;
}

UInt32
VaultRemappingEntry::getIdx()
{
	return m_ridx;
}

SubsecondTime
VaultRemappingEntry::accessOnce(UInt32 bank_idx)
{
	SubsecondTime latency = SubsecondTime::Zero();
	n_count++;

	m_bremap_table->accessOnce(bank_idx);

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
VaultRemappingEntry::isTooHot(UInt32 bank_idx)
{
	return m_bremap_table->isTooHot(bank_idx);
}

void
VaultRemappingEntry::balanceBanks(UInt32 bank_idx)
{
	m_bremap_table->balanceBanks(bank_idx);
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
	bool bankTooHot = m_vremap_arr[idx]->isTooHot(bank_i);
	if (bankTooHot) {
		printf("---vault: %d blance bank: %d", idx, bank_i);
		m_vremap_arr[idx]->balanceBanks(bank_i);
	}
	UInt32 cnt = m_vremap_arr[idx]->getCount();
	//printf("vault_%d's count is %d, bankTooHot: %d\n", idx, cnt, bankTooHot);
	if (cnt > 10000000) {
		return true;
	}
	return false;
}

void
VaultRemappingStructure::balanceVaults(UInt32 idx)
{
	UInt32 target = idx;
	UInt32 min_c = 99999;

	for (UInt32 i = 0; i < n_vaults; i++) {
		VaultRemappingEntry* vault = m_vremap_arr[i];
		UInt32 cnt = vault->getCount();
		UInt32 remap_i = vault->m_ridx;
		if (remap_i != idx && cnt < min_c) {
			min_c = cnt;
			target = remap_i;
		}
	}

	std::cout << " balance vault " << idx
			  << ", balance target: " << target << std::endl;

	swap(idx, target);
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

SubsecondTime
VaultRemappingStructure::accessOnce(UInt32 vault_idx, UInt32 bank_idx, DramCntlrInterface::access_t access_type)
{
	SubsecondTime access_time  = m_vremap_arr[vault_idx]->accessOnce(bank_idx);

	return access_time;
}
