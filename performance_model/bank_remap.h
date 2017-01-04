#ifndef __BANK_REMAP_H__
#define __BANK_REMAP_H__

#include "utils.h"
#include "stats.h"
#include "core.h"

#include "fixed_types.h"

#include <iostream>
#include <fstream>

class BankRemappingEntry {
	public :
		UInt32 m_idx; // physical bank index
		UInt32 m_ridx; // logical bank index

		bool m_valid;
		bool m_orig;
		UInt32 m_level; // bank level
		UInt32 n_count; // access counts

		BankRemappingEntry(UInt32 idx);
		~BankRemappingEntry();

		void remapTo(UInt32 idx);
		SubsecondTime accessOnce();
		void setValidBit(bool valid);
		UInt32 getCount();
		UInt32 getIdx();
};

class BankRemappingStructure {
	public:
		BankRemappingEntry** m_bremap_arr;
		UInt32 n_clock;
		UInt32 n_banks;
		BankRemappingStructure(UInt32 bank_num);
		~BankRemappingStructure();

		SubsecondTime swap(UInt32 src, UInt32 des);
		UInt32 getBankIdx(UInt32 idx, bool *valid);

		bool isValid(UInt32 idx);
		bool isTooHot(UInt32 idx);
		void balanceBanks(UInt32 idx);

		void setValid(UInt32 idx);
		void setInvalid(UInt32 idx);

		int copy_times;

		SubsecondTime accessOnce(UInt32 idx);
};

#endif
