#ifndef __STACKED_DRAM_CNTLR_H__
#define __STACKED_DRAM_CNTLR_H__

#include "utils.h"

#include "stats.h"

#include "dram_vault.h"

//#include "vault_remap.h"

//#include "bank_remap.h"

#include "fixed_types.h"

#include <iostream>
#include <fstream>

#include "ramulator/dram_sim.h"
#include "remapping.h"

//class VaultRemappingStructure;
//class BankRemappingStructure;

class RemappingManager;

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
		UInt32 n_banks;
		UInt32 n_rows;
		UInt32 m_vault_size;
		UInt32 m_bank_size;
		UInt32 m_row_size;
		UInt32 tot_reads, tot_writes, tot_misses;

		SubsecondTime last_req = SubsecondTime::Zero();

		/* structure for vault remapping*/
		//VaultRemappingStructure *m_vremap_table;

		/* Remapping Manager (REMAP_MAN)*/
		RemappingManager* m_remap_manager;
		bool remapped, enter_roi;
		UInt32 v_remap_times, b_remap_times;

		/* Some DRAM statistics*/
		UInt32 tot_dram_reads, tot_dram_writes, tot_row_hits;
		SubsecondTime tot_act_t, tot_pre_t, tot_rd_t, tot_wr_t;

		/* Here we need a remapping table */
		/* Here we need a statistics talbe */
		/* Here is functions usable for remapping */
		/*
NOTICE: these functions may be called by DRAM cache controller
	(StackedDramCacheCntlrUnison)
TODO: we need to consider vault parrellelism
		(we can enqueue all remapping requests into vault controllers (at most 4),
		 and then later memory requests to other vault controllers would not be stalled,
		 but we still need to wait on memory requests on those influenced controller)

			bool tryRemapping(); 
		    **Examine the statistics table
		    **Find if there is any need to remap
		    ****if yes, do remap, and return true
		    ****else, return false

			bool ckeckDramStatus(UInt32 v_id, UInt32 b_id);
			**Examine the status of each DRAM part
			**to help Cache Controller update cache status (set status)
			**Input: id of part of DRAM need to be checked
			**Output: valid or not
		*/
		

		//Dram Model (ramulator)
		DramModel* m_dram_model;
		bool first_req;

		VaultPerfModel** m_vaults_array;
		std::ofstream log_file;

		StackedDramPerfUnison(UInt32 vaults_num, UInt32 vault_size, UInt32 bank_size, UInt32 row_size);
		~StackedDramPerfUnison();

		void splitSetNum(UInt32 set_i, UInt32* vault_i, UInt32* bank_i, UInt32* row_i);
		UInt32 getSetNum(UInt32 vault_i, UInt32 bank_i, UInt32 row_i);
		SubsecondTime getAccessLatency(SubsecondTime pkt_time, UInt32 pkt_size, UInt32 set_i, DramCntlrInterface::access_t access_type);

		bool checkRowValid(UInt32 vault_i, UInt32 bank_i, UInt32 row_i);
		bool checkRowMigrated(UInt32 vault_i, UInt32 bank_i, UInt32 row_i);

		void checkDramValid(bool *valid_arr, UInt32 *b_valid_arr, UInt32 *b_migrated_arr);
		void checkTemperature(UInt32 idx, UInt32 bank_i);
		/* check stats and remap (REMAP_MAN)*/
		void checkStat();
		void tryRemapping();

		void clearRemappingStat();
		void updateStats();
		void clearCacheStats();
		void updateTemperature(UInt32 v, UInt32 b, UInt32 temperature, UInt32 v_temp);

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
		//VaultRemappingStructure *m_vremap_table;
		
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
