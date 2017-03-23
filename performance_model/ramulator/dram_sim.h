#ifndef __DRAM_SIM_H__
#define __DRAM_SIM_H__

#include "RamConfig.h"
#include "RamController.h"
#include "RamMemory.h"
#include "RamDRAM.h"
#include "RamStatistics.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdlib.h>
#include <functional>
#include <map>

/* Standards */
#include "HBM.h"

using namespace std;
using namespace ramulator;

class DramModel {
public:
	DramModel();
	DramModel(const std::string& fname);
	~DramModel();
	int R, C;
	RamConfig* configs;
	HBM* hbm;
	// freq: MHz, tCK: ns
	double freq, tCK;
	std::vector<RamController*> ctlrs;
	RamMemory* memory;
	map<int, int> latencies;
	function<void(RamRequest&)> read_complete;
	long interval_ticks;
	long tot_ticks;

	void printSpec();

	bool sendMemReq(RamRequest req);

	int mask(int bits);

	bool readRow(int vault, int bank, int row, int col);
	bool writeRow(int vault, int bank, int row, int col);

	void tickOnce();
	void resetIntervalTick();
	void setBankRef(int vault, int bank, bool hot);

	/* The unit of all time stats is "NS" */

	int getReadLatency(int vault);

	uint64_t getTotTime();

	uint64_t getVaultRdReq(int vault);
	
	uint64_t getVaultWrReq(int vault);

	uint64_t getServingRdReq(int vault);
	uint64_t getServingWrReq(int vault);

	uint64_t getVaultRowHits(int vault);

	uint64_t getBankActTime(int vault, int bank);

	uint64_t getBankRdTime(int vault, int bank);

	uint64_t getBankWrTime(int vault, int bank);

	uint32_t getBankReads(int vault, int bank);
	uint32_t getBankWrites(int vault, int bank);

};

#endif

