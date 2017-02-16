#ifndef __DRAM_SIM_H__
#define __DRAM_SIM_H__

#include "RamConfig.h"
#include "Controller.h"
#include "Memory.h"
#include "DRAM.h"
#include "Statistics.h"
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
	RamConfig* configs;
	HBM* hbm;
	std::vector<Controller*> ctlrs;
	Memory* memory;


};

#endif

