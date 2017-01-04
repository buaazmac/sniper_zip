#ifndef __VAULT_REMAP_H__
#define __VAULT_REMAP_H__

#include "utils.h"
#include "stats.h"
#include "core.h"
#include "dram_cntlr_interface.h"

#include "bank_remap.h"

#include "fixed_types.h"

#include <iostream>
#include <fstream>
class BankRemappingStructure;

class VaultRemappingEntry {
	public:
		UInt32 m_idx; // physical vault index
		UInt32 m_ridx; // logical vault index
		UInt32 m_bank_num;

		/*Bank Remapping Management*/
		BankRemappingStructure* m_bremap_table;
		
		bool m_valid; // valid bit of content
		bool m_orig; // original bit indicating whether this bank is original
		UInt32 n_count; // access counts

		VaultRemappingEntry(UInt32 idx);
		~VaultRemappingEntry(); 

		void remapTo(UInt32 idx);
		SubsecondTime accessOnce(UInt32 bank_idx);
		void setValidBit(bool valid);
		UInt32 getCount();
		UInt32 getIdx();

		bool isTooHot(UInt32 bank_idx);
		void balanceBanks(UInt32 bank_idx);

		int getBankValid();
};

class VaultRemappingStructure {
	public:
		VaultRemappingEntry** m_vremap_arr;
		UInt64 n_clock;
		UInt32 n_vaults;
		VaultRemappingStructure(UInt32 vault_num);
		~VaultRemappingStructure();

		SubsecondTime swap(UInt32 src, UInt32 des);
		UInt32 getVaultIdx(UInt32 idx, bool *valid);
		UInt32 getBankIdx(UInt32 vault_idx, UInt32 bank_idx, bool *valid);

		bool isValid(UInt32 idx, int *bank_valid);
		int getBankValid(UInt32 vault_idx);

		bool isTooHot(UInt32 idx, UInt32 bank_i);

		void balanceVaults(UInt32 idx);

		void setValid(UInt32 idx);
		void setInvalid(UInt32 idx);

		int copy_times;

		SubsecondTime accessOnce(UInt32 vault_idx, UInt32 bank_idx, DramCntlrInterface::access_t access_type);
};

#endif
