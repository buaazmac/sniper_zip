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

	void printSpec();

	bool sendMemReq(RamRequest req);

	int mask(int bits);

	bool readRow(int vault, int bank, int row, int col);
	bool writeRow(int vault, int bank, int row, int col);

	void tickOnce();

	/* The unit of all time stats is "NS" */

	int getReadLatency(int vault);

	uint64_t getTotTime();

	uint64_t getVaultRdReq(int vault);
	
	uint64_t getVaultWrReq(int vault);

	uint64_t getVaultRowHits(int vault);

	uint64_t getBankActTime(int vault, int bank);

	uint64_t getBankRdTime(int vault, int bank);

	uint64_t getBankWrTime(int vault, int bank);

};

#endif

