#ifndef __BANK_REMAP_H__
#define __BANK_REMAP_H__

#include "utils.h"
#include "stats.h"
#include "core.h"
#include "simulator.h"
#include "config.h"
#include "dram_cntlr_interface.h"

#include "dram_bank.h"

#include "fixed_types.h"

#include <iostream>
#include <fstream>

struct BankStatEntry;

class BankRemappingEntry {
	public :
		UInt32 m_idx; // logical bank index
		UInt32 m_ridx; // physical bank index

		bool m_valid;
		bool m_orig;
		bool m_changed;
		int n_access;
		UInt32 m_level; // bank level
		/**/
		BankStatEntry stats;

		BankRemappingEntry(UInt32 idx);
		~BankRemappingEntry();

		void clearStats();
		void clearAccess();

		void remapTo(UInt32 idx);
		SubsecondTime accessOnce(DramCntlrInterface::access_t);
		void setValidBit(bool valid);
		void setChangedBit(bool changed) {m_changed = changed;};
		bool getChangedBit() {return m_changed;};
		UInt32 getAccess();
		UInt32 getIdx();
};

class BankRemappingStructure {
	public:
		BankRemappingEntry** m_bremap_arr;
		SubsecondTime last_time, current_time;
		UInt32 n_banks;
		BankRemappingStructure(UInt32 bank_num);
		~BankRemappingStructure();

		SubsecondTime swap(UInt32 src, UInt32 des);
		UInt32 getBankIdx(UInt32 idx, bool *valid);

		bool isValid(UInt32 idx);
		bool isTooHot(UInt32 phy_vault_i, UInt32 idx);
		void balanceBanks(UInt32 phy_vault_i, UInt32 idx);

		void setValid(UInt32 idx);
		void setInvalid(UInt32 idx);

		void clearAllAccess();

		int copy_times;

		SubsecondTime accessOnce(UInt32 idx, DramCntlrInterface::access_t, SubsecondTime pkt_time);
};

#endif
