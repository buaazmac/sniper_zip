#ifndef __DRAM_BANK_H__
#define __DRAM_BANK_H__

#include <map>
#include <vector>

#include "fixed_types.h"
#include "subsecond_time.h"

using namespace std;

enum Command {
	ACT, PRE, PREA,
	RD, WR, RDA, WRA,
	REF, REFSB, PDE, PDX, SRE, SRX,
	MAX
};

// Statistics
struct BankStatEntry {
	SubsecondTime tACT, tPRE, tRD, tWR;
	UInt32 reads, writes, row_hits;
};
class BankPerfModel {
	public:
		UInt32 m_bank_size;
		UInt32 m_row_size;
		UInt32 n_rows;

		UInt32 cur_open_row;
		Command cur_cmd;
		SubsecondTime cur_time, tot_time;
		enum RowState {
			Openned, Closed, MAX
		};
		// Timing
		struct TimingEntry {
			Command cmd;
			int dist;
			int val;
		};
		vector<TimingEntry> timing[13]; // we have 13 states
		//Statistics
		BankStatEntry stats;

		// Speed
		struct SpeedEntry {
			int rate;
			double freq, tCK;
			int nBL, nCCDS, nCCDL;
			int nCL, nRCDR, nRCDW, nRP, nCWL;
			int nRAS, nRC;
			int nRTP, nWTRS, nWTRL, nWR;
			int nRRDS, nRRDL, nFAW;
			int nRFC, nREFI, nREFI1B;
			int nPD, nXP;
			int nCKESR, nXS;
		} speed_table = {1000, 500, 2.0, 2, 2, 3, 7, 7, 6, 7, 4, 17, 24, 7, 2, 4, 8, 4, 5, 20, 0, 1950, 0, 5, 5, 5, 0};
		
		BankPerfModel(UInt32 bank_size, UInt32 row_size);
		~BankPerfModel();

		RowState *m_row_state;

		RowState getRowState(UInt32 row_i);

		SubsecondTime getTransitionLatency(Command p_cmd, Command a_cmd);

		SubsecondTime processCommand(Command cmd, UInt32 row_i, SubsecondTime cmd_time);
		bool isOpenned(UInt32 row_i);
		int getOpennedRow();

	private:
		void init_timing();
	
};

#endif
