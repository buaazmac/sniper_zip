#include "dram_sim.h"

DramModel::DramModel()
{
}

DramModel::DramModel(const std::string& fname)
{
	configs = new RamConfig(fname);
	hbm = new HBM(configs->getPara("org"), configs->getPara("speed"));
	int C = configs->get_channels(), R = configs->get_ranks();
	for (int c = 0; c < C; c++) {
		DRAM* channel = new DRAM(hbm, Level::Channel);
		channel->id = c;
		channel->regStats("");
		Controller* ctlr = new Controller(*configs, channel);
		ctlrs.push_back(ctlr);
	}
	memory = new Memory(*configs, ctlrs);
}

DramModel::~DramModel()
{
}
