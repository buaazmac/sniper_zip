#ifndef __STACKED_DRAM_CNTLR_H__
#define __STACKED_DRAM_CNTLR_H__

#include "utils.h"

#include "stats.h"

#include "dram_vault.h"
#include "core.h"
#include "dram_cntlr_interface.h"

#include "fixed_types.h"

#include <iostream>
#include <fstream>

class StackedDramPerfMem {
	public:
		UInt32 n_vaults;
		UInt32 m_vault_size;
		UInt32 m_bank_size;
		UInt32 m_row_size;
		VaultPerfModel** m_vaults_array;
		std::ofstream log_file;

		StackedDramPerfMem(UInt32 vaults_num, UInt32 vault_size, UInt32 bank_size, UInt32 row_size);
		~StackedDramPerfMem();

		SubsecondTime getAccessLatency(SubsecondTime pkt_time, UInt32 pkt_size, IntPtr address, DramCntlrInterface::access_t access_type);
};

class StackedDramPerfCache {
	public:
		UInt32 n_vaults;
		UInt32 m_vault_size;
		UInt32 m_bank_size;
		UInt32 m_row_size;
		VaultPerfModel** m_vaults_array;
		std::ofstream log_file;

		StackedDramPerfCache(UInt32 vaults_num, UInt32 vault_size, UInt32 bank_size, UInt32 row_size);
		~StackedDramPerfCache();

		SubsecondTime getAccessLatency(SubsecondTime pkt_time, UInt32 pkt_size, UInt32 set_i, DramCntlrInterface::access_t access_type);

		SubsecondTime getAccessLatency(SubsecondTime pkt_time, UInt32 pkt_size, IntPtr address, DramCntlrInterface::access_t access_type);
};

#endif
