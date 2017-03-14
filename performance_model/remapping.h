#ifndef __REMAPPING_H__
#define __REMAPPING_H__

#include "utils.h"
#include "stats.h"
#include "core.h"
#include "simulator.h"
#include "config.h"
#include "dram_cntlr_interface.h"

#include "dram_vault.h"

#include "bank_remap.h"

#include "fixed_types.h"

#include "stacked_dram_cntlr.h"

#include <iostream>
#include <fstream>

class StackedDramPerfUnison;

class RemappingTable {
	/*
	n_vaults: number of vaults in 3D DRAM
	n_banks: number of banks in vault
	m_table: table of size n_vaults * n_banks
	entry: v, b, phy_v, phy_b, valid

	operations: remap(x, y) remap x to y
	*/
public:
	struct remapping_entry {
		int phy, log;
		bool valid, migrated;
	};
	int n_vaults, n_banks;
	int n_entries;
	struct remapping_entry* m_table;
	RemappingTable(int vaults, int banks);
	~RemappingTable();

	void remapBankTo(int src, int des, bool invalid);
	void remapVaultTo(int src, int des, bool invalid);

	int setValid(int idx, bool valid) {m_table[idx].valid = valid;}
	int setMigrated(int idx, bool migrated) {m_table[idx].migrated = migrated;}

	int getPhyIdx(int idx);
	int getLogIdx(int idx);
	bool getValid(int idx);
};

class StatStoreUnit {
	/*
	n_vaults: number of vaults in 3D DRAM
	n_banks: number of banks in vault
	m_table: table of size n_vaults * n_banks
	entry: v, b, access_count, data_cov, temperature

	operations: swap(x,y), clear(x), setAccess(x), setDataCov(x), setTemp(x)
	getAccess(x), getDataCov(x), getTemp(x)
	*/
public:
	struct stats_entry {
		int idx, access_count, data_cov, temperature, cntlr_temp;
		bool too_hot, valid, just_swapped;
	};
	int n_vaults, n_banks;
	const int temperature_threshold = 90;
	const int bank_access_threshold = 2000;
	const int vault_access_threshold = 3000;
	int n_entries;
	struct stats_entry* m_table;
	StatStoreUnit(int vaults, int banks);
	~StatStoreUnit();

	void clear(int idx);
	void swap(int x, int y);
	void setAccess(int idx, int x) {m_table[idx].access_count = x;}
	void setDataCov(int idx, int x) {m_table[idx].data_cov = x;}
	void setTemp(int idx, int x);
	void setControllerTemp(int v, int);
	int getAccess(int idx);
	int getDataCov(int idx);
	int getTemp(int idx);
	bool isTooHot(int idx);
	bool isTooFreq(int idx);
	bool isJustRemapped(int idx);

	int getVaultAccess(int vault_i);
	int getVaultDataCov(int vault_i);
	int getVaultTemp(int vault_i);
	bool isVaultTooHot(int vault_i);
	bool isVaultTooFreq(int vault_i);

};

class RemappingManager {
	/*
	m_remap_table, m_stat_unit;
	int findTarget();
	void issueRemap(int src, int des);
	bool checkStat(int v, int b, bool remap);
	void getPhysicalIndex(int* v_i, int* b_i); // return a physical global bank index, (useful for both remapping table structure)
	*/
public:
	/* remapping policy: 
						1. invalidation vault remapping
						2. invalidation bank remapping
						3. migration vault remapping
						4. migration bank remapping
	 */
	int policy = 1;
	int n_vaults, n_banks;
	RemappingTable *m_remap_table;
	StatStoreUnit *m_stat_unit;
	StackedDramPerfUnison* m_dram_perf_cntlr;

	RemappingManager(StackedDramPerfUnison* dram_perf_cntlr);
	~RemappingManager();

	bool getPhysicalIndex(int* vault_i, int* bank_i);
	void accessRow(int vault_i, int bank_i, int row_i, int req_times);
	int findBankTarget();
	int findVaultTarget();
	void issueBankRemap(int src, int des);
	void issueVaultRemap(int src, int des);
	bool checkBankStat(int v, int  b);
	bool checkStat(int v, int b, bool remap);
	void handleRequest(int v, int b);
	void updateTemperature(int v, int b, int temperature, int v_temp);

	void reset(int v, int b);

	bool checkMigrated(int v, int b);
};

#endif
