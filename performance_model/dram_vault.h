#ifndef __DRAM_VAULT_H__
#define __DRAM_VAULT_H__

#include "core.h"
#include "dram_cntlr_interface.h"

#include "fixed_types.h"
#include "subsecond_time.h"

#include "dram_bank.h"

// Statistics
struct VaultStatEntry {
	SubsecondTime tACT, tPRE, tRD, tWR;
	UInt32 reads, writes, row_hits;
};

class VaultPerfModel {
	public:
		UInt32 m_size; // Bytes
		UInt32 m_bank_size;
		UInt32 m_row_size;
		UInt32 n_banks;
		bool auto_ref;
		BankPerfModel** m_banks_array;
		VaultStatEntry stats;
		
		VaultPerfModel(UInt32 size, UInt32 bank_size, UInt32 row_size);
		~VaultPerfModel();

		// Process Request from dram controller (cache/memory): bank index, row index
		SubsecondTime processRequest(SubsecondTime cur_time, DramCntlrInterface::access_t access_type, UInt32 bank_i, UInt32 row_i);
};


#endif
