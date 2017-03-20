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
	UInt32 n_vaults, n_banks, n_rows;
	UInt32 n_entries;
	struct remapping_entry* m_table;
	RemappingTable(UInt32 vaults, UInt32 banks, UInt32 rows);
	~RemappingTable();

	void remapRow(UInt32 src, UInt32 des, bool invalid);
	void remapBank(UInt32 src, UInt32 des, bool invalid);
	void remapVault(UInt32 src, UInt32 des, bool invalid);

	void setValid(UInt32 idx, bool valid) {m_table[idx].valid = valid;}
	void setMigrated(UInt32 idx, bool migrated) {m_table[idx].migrated = migrated;}

	UInt32 getPhyIdx(UInt32 idx);
	UInt32 getLogIdx(UInt32 idx);
	UInt32 getPhyBank(UInt32 idx);
	UInt32 getLogBank(UInt32 idx);
	UInt32 getPhyVault(UInt32 idx);
	UInt32 getLogVault(UInt32 idx);
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
		UInt32 idx, access_count;
		bool valid, just_remapped;
	};
	UInt32 n_vaults, n_banks, n_rows;
	const UInt32 temperature_threshold = 85;
	const UInt32 row_access_threshold = 100;
	const UInt32 bank_access_threshold = 1000;
	const UInt32 vault_access_threshold = 3000;
	UInt32 n_entries;
	struct stats_entry* m_table;
	UInt32* vault_temperature;
	StatStoreUnit(UInt32 vaults, UInt32 banks, UInt32 rows);
	~StatStoreUnit();

	void clear(UInt32 idx);
	void swap(UInt32 x, UInt32 y);
	void remapRow(UInt32 src, UInt32 des);
	void remapBank(UInt32 src, UInt32 des);
	void remapVault(UInt32 src, UInt32 des);
	void setAccess(UInt32 idx, UInt32 x) {m_table[idx].access_count = x;}
	void setControllerTemp(UInt32 v, UInt32 x);
	void enableRemapping(UInt32 idx) {m_table[idx].just_remapped = false;};

	UInt32 getAccess(UInt32 idx);
	UInt32 getTemp(UInt32 idx);
	bool isTooFreq(UInt32 idx);
	bool isJustRemapped(UInt32 idx);

	UInt32 getBankAccess(UInt32 bank_i);
	UInt32 getVaultAccess(UInt32 vault_i);
	UInt32 getVaultTemp(UInt32 vault_i);
	bool isBankTooFreq(UInt32 bank_i);
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
	const int max_remap_times = 5;
	UInt32 policy;
	UInt32 n_vaults, n_banks, n_rows;
	RemappingTable *m_remap_table;
	StatStoreUnit *m_stat_unit;
	StackedDramPerfUnison* m_dram_perf_cntlr;

	RemappingManager(StackedDramPerfUnison* dram_perf_cntlr, UInt32 p);
	~RemappingManager();

	bool getPhysicalIndex(UInt32* vault_i, UInt32* bank_i, UInt32* row_i);
	void accessRow(UInt32 vault_i, UInt32 bank_i, UInt32 row_i, UInt32 req_times);
	void issueRowRemap(UInt32 src, UInt32 des);

	bool checkRowStat(UInt32 v, UInt32 b, UInt32 r);
	UInt32 findHottestRow();
	UInt32 findTargetInVault(UInt32 src_log);
	UInt32 findTargetCrossVault(UInt32 src_log);
	UInt32 tryRemapping(bool remap);

	bool checkBankStat(UInt32 v, UInt32 b) {return false;};
	bool checkVaultStat(UInt32 v, UInt32 b) {return false;};
	bool checkStat(UInt32 v, UInt32 b, bool remap) {return false;};

	void updateTemperature(UInt32 v, UInt32 v_temp);

	void reset(UInt32 v, UInt32 b, UInt32 r);
	void resetRemapping();
	void finishRemapping();

	bool checkMigrated(UInt32 v, UInt32 b, UInt32 r);
	bool checkValid(UInt32 v, UInt32 b, UInt32 r);

	void splitIdx(UInt32 idx, UInt32* v, UInt32* b, UInt32* r);
	UInt32 translateIdx(UInt32 v, UInt32 b, UInt32 r);
};

#endif
