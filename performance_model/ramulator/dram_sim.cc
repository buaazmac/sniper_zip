#include "dram_sim.h"

DramModel::DramModel()
{
}

DramModel::DramModel(const std::string& fname)
{
	configs = new RamConfig(fname);
	hbm = new HBM(configs->getPara("org"), configs->getPara("speed"));
	C = configs->get_channels(); R = configs->get_ranks();
	hbm->set_channel_number(C);
	hbm->set_rank_number(R);
	freq = hbm->speed_entry.freq;
	tCK = hbm->speed_entry.tCK;

	for (int c = 0; c < C; c++) {
		RamDRAM* channel = new RamDRAM(hbm, Level::Channel);
		channel->id = c;
		channel->regStats("");
		RamController* ctlr = new RamController(*configs, channel);
		ctlrs.push_back(ctlr);
	}
	memory = new RamMemory(*configs, ctlrs);

	/* Construct the initial request*/
	read_complete = [this](RamRequest& r) {this->latencies[r.depart - r.arrive]++; };
}

DramModel::~DramModel()
{
}

/*Test the functionality of our code*/
void DramModel::printSpec()
{
	printf("We are using %d controllers!!!\n", memory->ctrls.size());
}

bool DramModel::sendMemReq(RamRequest req)
{
	return memory->send(req);
}

int DramModel::mask(int bits)
{
	return (1 << bits) - 1;
}

bool DramModel::readRow(int vault, int bank, int row, int col)
{
	RamRequest::Type type = RamRequest::Type::READ;
	uint64_t addr = 0;
	int bits = 0;
	int rank = bank >> memory->addr_bits[2];
	int ba = bank & mask(memory->addr_bits[2]);
	if (memory->type == RamMemory::Type::ChRaBaRoCo) {
		addr = vault & mask(memory->addr_bits[0]);
		addr <<= memory->addr_bits[1];
		addr |= (rank & mask(memory->addr_bits[1]));
		addr <<= memory->addr_bits[2];
		addr |= (ba & mask(memory->addr_bits[2]));
		addr <<= memory->addr_bits[3];
		addr |= (row & mask(memory->addr_bits[3]));
		addr <<= memory->addr_bits[4];
		addr |= (col & mask(memory->addr_bits[4]));
		
	}
	else if (memory->type == RamMemory::Type::RoBaRaCoCh) {
		addr = row & mask(memory->addr_bits[3]);
		addr <<= memory->addr_bits[2];
		addr |= (ba & mask(memory->addr_bits[2]));
		addr <<= memory->addr_bits[1];
		addr |= (rank & mask(memory->addr_bits[1]));
		addr <<= memory->addr_bits[4];
		addr |= (col & mask(memory->addr_bits[4]));
		addr <<= memory->addr_bits[0];
		addr |= (vault & mask(memory->addr_bits[0]));
	}
	else {

	}
	//addr <<= memory->tx_bits;
	RamRequest req(addr, type, read_complete);
	return sendMemReq(req);
}

bool DramModel::writeRow(int vault, int bank, int row, int col)
{
	RamRequest::Type type = RamRequest::Type::WRITE;
	uint64_t addr = 0;
	int bits = 0;
	int rank = bank >> memory->addr_bits[2];
	int ba = bank & mask(memory->addr_bits[2]);
	if (memory->type == RamMemory::Type::ChRaBaRoCo) {
		addr = vault & mask(memory->addr_bits[0]);
		addr <<= memory->addr_bits[1];
		addr |= (rank & mask(memory->addr_bits[1]));
		addr <<= memory->addr_bits[2];
		addr |= (ba & mask(memory->addr_bits[2]));
		addr <<= memory->addr_bits[3];
		addr |= (row & mask(memory->addr_bits[3]));
		addr <<= memory->addr_bits[4];
		addr |= (col & mask(memory->addr_bits[4]));

	}
	else if (memory->type == RamMemory::Type::RoBaRaCoCh) {
		addr = row & mask(memory->addr_bits[3]);
		addr <<= memory->addr_bits[2];
		addr |= (ba & mask(memory->addr_bits[2]));
		addr <<= memory->addr_bits[1];
		addr |= (rank & mask(memory->addr_bits[1]));
		addr <<= memory->addr_bits[4];
		addr |= (col & mask(memory->addr_bits[4]));
		addr <<= memory->addr_bits[0];
		addr |= (vault & mask(memory->addr_bits[0]));
	}
	else {

	}
	//addr <<= memory->tx_bits;
	RamRequest req(addr, type, read_complete);
	return sendMemReq(req);
}

void DramModel::tickOnce()
{
	memory->tick();
}

int DramModel::getReadLatency(int vault)
{
	int rd_latency = memory->spec->read_latency;
	if (rd_latency < memory->ctrls[vault]->read_latency_avg.value()) {
		rd_latency = memory->ctrls[vault]->read_latency_avg.value();
	}
	return rd_latency;
}

uint64_t DramModel::getTotTime()
{
	uint64_t t_ck = (uint64_t)tCK;
	uint64_t dram_cycles = memory->num_dram_cycles.value();
	return dram_cycles * t_ck;
}

uint64_t DramModel::getVaultRdReq(int vault)
{
	uint64_t rd_row_hits = memory->ctrls[vault]->read_row_hits[0].value();
	uint64_t rd_row_misses = memory->ctrls[vault]->read_row_misses[0].value();
	uint64_t rd_row_conflicts = memory->ctrls[vault]->read_row_conflicts[0].value();
	return (rd_row_hits + rd_row_misses + rd_row_conflicts);
}

uint64_t DramModel::getVaultWrReq(int vault)
{
	uint64_t wr_row_hits = memory->ctrls[vault]->write_row_hits[0].value();
	uint64_t wr_row_misses = memory->ctrls[vault]->write_row_misses[0].value();
	uint64_t wr_row_conflicts = memory->ctrls[vault]->write_row_conflicts[0].value();
	return (wr_row_hits + wr_row_misses + wr_row_conflicts);
}

uint64_t DramModel::getVaultRowHits(int vault)
{
	uint64_t rd_row_hits = memory->ctrls[vault]->read_row_hits[0].value();
	uint64_t wr_row_hits = memory->ctrls[vault]->write_row_hits[0].value();
	return (rd_row_hits + wr_row_hits);
}

uint64_t DramModel::getBankActTime(int vault, int bank)
{
	int rank = bank >> memory->addr_bits[2];
	uint64_t t_ck = (uint64_t)tCK;
	uint64_t act_cycles = memory->ctrls[vault]->channel->children[rank]->active_cycles.value();
	return act_cycles * t_ck;
}

uint64_t DramModel::getBankRdTime(int vault, int bank)
{
	int rank = bank >> memory->addr_bits[2];
	uint64_t reads = memory->ctrls[vault]->bank_reads[bank].value();
	uint64_t writes = memory->ctrls[vault]->bank_writes[bank].value();
	uint64_t act_t = getBankActTime(vault, bank);
	double read_ratio = 0;
	if (reads + writes != 0) 
		read_ratio = double(reads) / double(reads + writes);
	uint64_t rst = act_t * read_ratio;
	//cout << "@@@RANK_READ@@@" << rank << ", reads: " << reads << ", writes: " << writes << ", rst: " << rst << endl;
	return rst;
}

uint64_t DramModel::getBankWrTime(int vault, int bank)
{
	int rank = bank >> memory->addr_bits[2];
	uint64_t reads = memory->ctrls[vault]->bank_reads[bank].value();
	uint64_t writes = memory->ctrls[vault]->bank_writes[bank].value();
	uint64_t act_t = getBankActTime(vault, bank);
	double write_ratio = 0;
	if (reads + writes != 0)
		write_ratio = float(writes) / float(reads + writes);
	uint64_t rst = act_t * write_ratio;
	//cout << "@@@RANK_WRITES@@@" << rank << ", reads: " << reads << ", writes: " << writes << ", rst: " << rst << endl;
	return rst;
}

uint32_t DramModel::getBankReads(int vault, int bank)
{
	return uint32_t(memory->ctrls[vault]->bank_reads[bank].value());
}
uint32_t DramModel::getBankWrites(int vault, int bank)
{
	return uint32_t(memory->ctrls[vault]->bank_writes[bank].value());
}
