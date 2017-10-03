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
#include <map>
#include <vector>
#include <unordered_set>

#define INVALID_TARGET 9999999

class StackedDramPerfUnison;

class BankStat {
public:
	/*
	 * bank_id: corresponding to original mapping
	 * logical_id: usually the same as bank_id, changed to another id when combined
	 * physical_id: initial same as bank_id, changed to another id when remapped
	 */
	UInt32 _bank_id;
	UInt32 _logical_id, _physical_id, _remap_id;
	/*
	 * valid: indicate whether the content of bank is still valid
	 * disabled: indicate whether the bank is disabled because of high temperature
	 * valid_rows: add some exceptional rows to optimize the cache miss rate (migrated)
	 */
	bool _valid = true, _disabled = false, _combined = false;
	std::unordered_set<UInt32> valid_rows;

	/* MEA map*/
	std::map<UInt32, UInt32> mea_map;
	UInt32 _n_migrate_row = 10;

	/* Statistics */
	long remap_times = 0, disabled_times = 0, invalidate_times = 0;

	/* Constructor */
	BankStat(UInt32 id);
	~BankStat();

	/* Methods of management */
	void accessRow(UInt32 row_id);
	void remapTo(UInt32 phy_bank_id, bool disabled);
	void combineWith(BankStat* target_bank);
	void migrateRow(UInt32 row_id);
	void setValid(bool valid) {_valid = valid;}
	void setDisabled(bool disabled) {_disabled = disabled;}
	void setId(UInt32 log_id, UInt32 phy_id);
	void finishRemapping();

};

class RemappingManager {
public:
	/* Memory information*/
	UInt32 _n_vaults, _n_banks, _n_rows;
	UInt32 _tot_banks;
	/* Experiment configurations */
	bool _inter_vault = false;
	UInt32 _n_remap = 0;
	UInt32 _high_thres, _dangerous_thres, _remap_thres;
	UInt32 _init_temp;
	/*physical bank table*/
	struct PhyBank {
		UInt32 _logical_bank;
		double _temperature;
		bool _valid;
		long _hot_access, _cool_access, _remap_access;
	};
	int remap_times = 0, disable_times = 0, double_disable_times = 0, recovery_times = 0;
	vector<PhyBank> _phy_banks;
	vector<BankStat*> _bank_stat;

	

	StackedDramPerfUnison* _m_dram_perf_cntlr;

	RemappingManager(StackedDramPerfUnison* dram_perf_cntlr);
	~RemappingManager();

	void setRemapConfig(UInt32 n_remaps, bool inter_vault, UInt32 ht, UInt32 dt, UInt32 rt, UInt32 it);
	
	void updateTemperature(UInt32 v, UInt32 b, double temp);

	void resetBank(UInt32 bank_id);
	void resetStats(bool reset);

	bool checkMigrated(UInt32 v, UInt32 b, UInt32 r);
	bool checkValid(UInt32 v, UInt32 b, UInt32 r);
	bool checkDisabled(UInt32 v, UInt32 b, UInt32 r);
	/* Access a row: update bank stats */
	void accessRow(UInt32 v, UInt32 b, UInt32 r);

	/* Do remapping-based thermal management */
	void runMechanism();
	

	/* Get the index information: bank, row...*/

	void splitId(UInt32 idx, UInt32* v, UInt32* b, UInt32* r);
	UInt32 getBankId(UInt32 v, UInt32 b);
	void getPhysicalIndex(UInt32* v, UInt32* b, UInt32* r);
	void getLogicalIndex(UInt32* v, UInt32* b, UInt32* r);
};

#endif
