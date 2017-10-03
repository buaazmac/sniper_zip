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
#include <vector>

/* Standards */
#include "HBM.h"

using namespace std;
using namespace ramulator;

class DramModel {
public:
	struct Request {
		int type;
		int vault, bank, row, col;
	};
	vector<Request> req_queue;
	int recent_read_latency, recent_write_latency;
	uint64_t last_req_time = 0;
	int n_updates = 0;
	double avg_req_num = 0;
	double avg_update_time;
	double avg_mem_latency = 0;

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
	map<int, int> overhead;
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

	int getReadLatency(int vault, int bank, int row, int col, uint64_t pkt_time);
	int getWriteLatency(int vault, int bank, int row, int col, uint64_t pkt_time);
	void updateLatency(uint64_t pkt_time);
	int getPrevLatency();

	uint64_t getTotTime();

	double getVaultQueLenAvg(int vault);
	double getVaultQueLenSum(int vault);

	uint64_t getVaultRdReq(int vault);
	
	uint64_t getVaultWrReq(int vault);

	uint64_t getServingRdReq(int vault);
	uint64_t getServingWrReq(int vault);

	uint64_t getVaultRowHits(int vault);

	uint64_t getBankRowHits(int vault, int bank);
	uint64_t getBankRowConflicts(int vault, int bank);
	uint64_t getBankRowMisses(int vault, int bank);

	uint64_t getBankActTime(int vault, int bank);

	uint64_t getBankRdTime(int vault, int bank);

	uint64_t getBankWrTime(int vault, int bank);

	uint32_t getBankReads(int vault, int bank);
	uint32_t getBankWrites(int vault, int bank);

};

#endif

