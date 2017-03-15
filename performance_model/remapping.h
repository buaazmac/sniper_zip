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

#define INVALID_TARGET 999

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
		UInt32 phy, log;
		bool valid, migrated;
	};
	UInt32 n_vaults, n_banks;
	UInt32 n_entries;
	struct remapping_entry* m_table;
	RemappingTable(UInt32 vaults, UInt32 banks);
	~RemappingTable();

	void remapBankTo(UInt32 src, UInt32 des, bool invalid);
	void remapVaultTo(UInt32 src, UInt32 des, bool invalid);

	void setValid(UInt32 idx, bool valid) {m_table[idx].valid = valid;}
	void setMigrated(UInt32 idx, bool migrated) {m_table[idx].migrated = migrated;}

	UInt32 getPhyIdx(UInt32 idx);
	UInt32 getLogIdx(UInt32 idx);
	bool getValid(UInt32 idx);
	bool getMigrated(UInt32 idx);
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
		UInt32 idx, access_count, data_cov, temperature, cntlr_temp;
		bool too_hot, valid, just_remapped;
	};
	UInt32 n_vaults, n_banks;
	const UInt32 temperature_threshold = 90;
	const UInt32 bank_access_threshold = 2000;
	const UInt32 vault_access_threshold = 3000;
	UInt32 n_entries;
	struct stats_entry* m_table;
	StatStoreUnit(UInt32 vaults, UInt32 banks);
	~StatStoreUnit();

	void clear(UInt32 idx);
	void swap(UInt32 x, UInt32 y);
	void remapBank(UInt32 src, UInt32 des);
	void remapVault(UInt32 src, UInt32 des);
	void setAccess(UInt32 idx, UInt32 x) {m_table[idx].access_count = x;}
	void setDataCov(UInt32 idx, UInt32 x) {m_table[idx].data_cov = x;}
	void setTemp(UInt32 idx, UInt32 x);
	void setControllerTemp(UInt32 v, UInt32 x);

	UInt32 getLogIdx(UInt32 idx);
	UInt32 getAccess(UInt32 idx);
	UInt32 getDataCov(UInt32 idx);
	UInt32 getTemp(UInt32 idx);
	bool isTooHot(UInt32 idx);
	bool isTooFreq(UInt32 idx);
	bool isJustRemapped(UInt32 idx);

	UInt32 getVaultAccess(UInt32 vault_i);
	UInt32 getVaultDataCov(UInt32 vault_i);
	UInt32 getVaultTemp(UInt32 vault_i);
	bool isVaultTooHot(UInt32 vault_i);
	bool isVaultTooFreq(UInt32 vault_i);

};

class RemappingManager {
	/*
	m_remap_table, m_stat_unit;
	UInt32 findTarget();
	void issueRemap(UInt32 src, UInt32 des);
	bool checkStat(UInt32 v, UInt32 b, bool remap);
	void getPhysicalIndex(UInt32* v_i, UInt32* b_i); // return a physical global bank index, (useful for both remapping table structure)
	*/
public:
	/* remapping policy: 
						1. invalidation vault remapping
						2. invalidation bank remapping
						3. migration vault remapping
						4. migration bank remapping
	 */
	UInt32 policy = 1;
	UInt32 n_vaults, n_banks;
	RemappingTable *m_remap_table;
	StatStoreUnit *m_stat_unit;
	StackedDramPerfUnison* m_dram_perf_cntlr;

	RemappingManager(StackedDramPerfUnison* dram_perf_cntlr);
	~RemappingManager();

	bool getPhysicalIndex(UInt32* vault_i, UInt32* bank_i);
	void accessRow(UInt32 vault_i, UInt32 bank_i, UInt32 row_i, UInt32 req_times);
	UInt32 findBankTarget(UInt32 idx);
	UInt32 findVaultTarget(UInt32 vault_i);
	void issueBankRemap(UInt32 src, UInt32 des);
	void issueVaultRemap(UInt32 src, UInt32 des);
	bool checkBankStat(UInt32 v, UInt32 b);
	bool checkVaultStat(UInt32 v, UInt32 b);
	bool checkStat(UInt32 v, UInt32 b, bool remap);
	void handleRequest(UInt32 v, UInt32 b);
	void updateTemperature(UInt32 v, UInt32 b, UInt32 temperature, UInt32 v_temp);

	void reset(UInt32 v, UInt32 b);
	void finishRemapping();

	bool checkMigrated(UInt32 v, UInt32 b);
};

#endif
