#include "dram_bank.h"

BankPerfModel::BankPerfModel(UInt32 bank_size, UInt32 row_size)
	: m_bank_size(bank_size),
	  m_row_size(row_size) 
{
	// Initialize performance statistic
	stats.tACT = stats.tPRE = stats.tRD = stats.tWR = SubsecondTime::Zero();
	stats.reads = stats.writes = stats.row_hits = 0;

	// Initialize current command state: precharge
	cur_cmd = Command::PRE;

	// Initialize current and total time: 0
	cur_time = SubsecondTime::Zero();
	tot_time = SubsecondTime::Zero();

	n_rows = bank_size / row_size;
	// Initialize row state
	m_row_state = new BankPerfModel::RowState[n_rows];
	for (UInt32 i = 0; i < n_rows; i++) {
		m_row_state[i] = Closed;
	}
	// Initialize timing stats
	init_timing();

	// Set current open row to -1
	cur_open_row = -1;
}

BankPerfModel::~BankPerfModel()
{
	delete [] m_row_state;
}

BankPerfModel::RowState
BankPerfModel::getRowState(UInt32 row_i)
{
	return m_row_state[row_i];
}

SubsecondTime
BankPerfModel::getTransitionLatency(Command p_cmd, Command a_cmd)
{
	SubsecondTime latency;
	UInt32 vn = timing[(int)p_cmd].size();
	UInt32 i = 0;
	for (i = 0; i < vn; i++) {
		if (timing[(int)p_cmd][i].cmd == a_cmd) {
			uint64_t tmp = timing[p_cmd][i].val * 1000 / speed_table.freq;
			latency = SubsecondTime::NS(tmp);
			break;
		}
	}
	if (i == vn) {
		std::cout << "__ERROR__BANK_process_cmd: illegal transition" << std::endl;
		std::cout << "_________cur_cmd: " << p_cmd << "__cmd: " << a_cmd << std::endl;
		latency = SubsecondTime::Zero();
	}
	return latency;
}

SubsecondTime
BankPerfModel::processCommand(Command cmd, UInt32 row_i, SubsecondTime cmd_time)
{
	bool row_hit = false;
	SubsecondTime cmd_latency;

	if (cmd == WR && cur_open_row == row_i) {
		if (cur_cmd != ACT) {
			stats.row_hits++;
		}
		row_hit = true;
		cmd_latency = getTransitionLatency(ACT, WR);
	}
	if (cmd == RD && cur_open_row == row_i) {
		if (cur_cmd != ACT) {
			stats.row_hits++;
		}
		row_hit = true;
		cmd_latency = getTransitionLatency(ACT, RD);
	}

	if (cur_time != SubsecondTime::Zero()) {
		tot_time += (cmd_time - cur_time);
	}
	cur_time = cmd_time;

	/* Here we calculate command transition latency */
	if (row_hit == false)
		cmd_latency = getTransitionLatency(cur_cmd, cmd);

	// std::cout << "Row index: " << row_i << std::endl;

	switch ( cmd ) {
		case ACT:
			stats.tACT += cmd_latency;
			m_row_state[row_i] = Openned;
			cur_open_row = row_i;
			break;
		case PRE:
			stats.tPRE += cmd_latency;
			m_row_state[row_i] = Closed;
			cur_open_row = -1;
			break;
		case RD:
			stats.tRD += cmd_latency;
			stats.reads ++;
			break;
		case WR:
			stats.tWR += cmd_latency;
			stats.writes ++;
			break;
		default:
			break;
	}
	/* Here we change state */
	cur_cmd = cmd;

	// Because we manage command in vault controller
	// Here we only calculate command transition time
	return cmd_latency;
}

void
BankPerfModel::init_timing() {
	SpeedEntry& s = speed_table;
    // CAS <-> RAS
    timing[int(Command::ACT)].push_back({Command::RD, 1, s.nRCDR});
	
    timing[int(Command::ACT)].push_back({Command::RDA, 1, s.nRCDR});
    timing[int(Command::ACT)].push_back({Command::WR, 1, s.nRCDW});
    timing[int(Command::ACT)].push_back({Command::WRA, 1, s.nRCDW});

    timing[int(Command::RD)].push_back({Command::PRE, 1, s.nRTP});
    timing[int(Command::WR)].push_back({Command::PRE, 1, s.nCWL + s.nBL + s.nWR});

    timing[int(Command::RDA)].push_back({Command::ACT, 1, s.nRTP + s.nRP});
    timing[int(Command::WRA)].push_back({Command::ACT, 1, s.nCWL + s.nBL + s.nWR + s.nRP});

    // RAS <-> RAS
    timing[int(Command::ACT)].push_back({Command::ACT, 1, s.nRC});
    timing[int(Command::ACT)].push_back({Command::PRE, 1, s.nRAS});
    timing[int(Command::PRE)].push_back({Command::ACT, 1, s.nRP});

    // REFSB
    timing[int(Command::PRE)].push_back({Command::REFSB, 1, s.nRP});
    timing[int(Command::REFSB)].push_back({Command::REFSB, 1, s.nRFC});
    timing[int(Command::REFSB)].push_back({Command::ACT, 1, s.nRFC});
}

bool
BankPerfModel::isOpenned(UInt32 row_i)
{
	if (row_i >= n_rows) {
		std::cout << "__ERROR_isOpenning__: Access row number illegal" << std::endl;
	}
	return (m_row_state[row_i] == Openned);
}

int
BankPerfModel::getOpennedRow()
{
	for (int i = 0; i < (int)n_rows; i++) {
		if (m_row_state[i] == Openned) {
			return i;
		}
	}
	return -1;
}
