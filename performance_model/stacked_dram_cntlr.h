#ifndef __STACKED_DRAM_CNTLR_H__
#define __STACKED_DRAM_CNTLR_H__

#include "utils.h"

#include "stats.h"

#include "dram_vault.h"

#include "vault_remap.h"

#include "bank_remap.h"

#include "core.h"
#include "dram_cntlr_interface.h"

#include "fixed_types.h"

#include <iostream>
#include <fstream>

class VaultRemappingStructure;
class BankRemappingStructure;

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
	private:
		UInt32 *bankRemap;
};

class StackedDramPerfUnison {
	public:
		UInt32 n_vaults;
		UInt32 m_vault_size;
		UInt32 m_bank_size;
		UInt32 m_row_size;

		/* structure for vault remapping*/
		VaultRemappingStructure *m_vremap_table;

		VaultPerfModel** m_vaults_array;
		std::ofstream log_file;

		StackedDramPerfUnison(UInt32 vaults_num, UInt32 vault_size, UInt32 bank_size, UInt32 row_size);
		~StackedDramPerfUnison();

		SubsecondTime getAccessLatency(SubsecondTime pkt_time, UInt32 pkt_size, UInt32 set_i, DramCntlrInterface::access_t access_type);

		void checkDramValid(bool *valid_arr, int *banks);
		void checkTemperature(UInt32 idx, UInt32 bank_i);
		void finishInvalidation();

	private:
		UInt32 *bankRemap;
};

class StackedDramPerfAlloy {
	public:
		UInt32 n_vaults;
		UInt32 m_vault_size;
		UInt32 m_bank_size;
		UInt32 m_row_size;

		/* structure for vault remapping*/
		VaultRemappingStructure *m_vremap_table;

		VaultPerfModel** m_vaults_array;
		std::ofstream log_file;

		StackedDramPerfAlloy(UInt32 vaults_num, UInt32 vault_size, UInt32 bank_size, UInt32 row_size);
		~StackedDramPerfAlloy();

		SubsecondTime getAccessLatency(SubsecondTime pkt_time, UInt32 pkt_size, UInt32 set_i, DramCntlrInterface::access_t access_type);

		void checkVaultValid(bool *valid_arr);
		void finishInvalidation();

	private:
		UInt32 *bankRemap;
};
#endif
