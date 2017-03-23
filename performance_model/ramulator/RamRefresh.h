/*
 * Refresh.h
 *
 * This is a refresh scheduler. A list of refresh policies implemented:
 *
 * 1. All-bank refresh
 * 2. Per-bank refresh (only DSARP memory module has been completed to work with REFpb).
 *     The other modules (LPDDRx) have not been updated to pass a knob to turn on/off REFpb.
 * 3. A re-implementation of DSARP from the refresh mechanisms proposed in Chang et al.,
 * "Improving DRAM Performance by Parallelizing Refreshes with Accesses", HPCA 2014.
 *
 *  Created on: Mar 17, 2015
 *      Author: kevincha
 */

#ifndef __RAM_REFRESH_H_
#define __RAM_REFRESH_H_

#include <stddef.h>
#include <cassert>
#include <iostream>
#include <vector>

#include "RamRequest.h"

using namespace std;
using namespace ramulator;

namespace ramulator {

class HBM;

class RamController;

class Refresh {
public:
  RamController* ctrl;
  long clk, refreshed;
  long *bank_ref_interval;
  long *bank_refreshed;
  // Per-bank refresh counter to track the refresh progress for each rank
  vector<int> bank_ref_counters;
  int max_rank_count, max_bank_count;
  int n_banks;
  int level_chan, level_rank, level_bank, level_sa;

  // ctor
  Refresh(RamController* ctrl);

  // dtor
  virtual ~Refresh() {
    // Clean up backlog
    for (unsigned int i = 0; i < bank_refresh_backlog.size(); i++)
      delete bank_refresh_backlog[i];
	delete bank_ref_interval;
	delete bank_refreshed;
  }

  // Basic refresh scheduling for all bank refresh that is applicable to all DRAM types
  void tick_ref();
  void set_ref_interval(int bank_i, bool hot);

private:
  // Keeping track of refresh status of every bank: + means ahead of schedule, - means behind schedule
  vector<vector<int>*> bank_refresh_backlog;
  // Keeping track of which subarray to refresh next
  vector<vector<int>> subarray_ref_counters;
  int max_sa_count = 0;
  // As defined in the standards
  int backlog_max = 8;
  int backlog_min = -8;
  int backlog_early_pull_threshold = -6;
  bool ctrl_write_mode = false;

  // Refresh based on the specified address
  void refresh_target(RamController* ctrl, int rank, int bank, int sa);

  // Inject refresh at either rank or bank level
  void inject_refresh(bool b_ref_rank);

  void inject_bank_refresh(int bank_i);

  // DSARP
  void early_inject_refresh();
  void wrp();
};

// Declaration of specialized constructor and tick_ref, so the compiler knows
// where to look for these definitions when controller calls them!
//template<> Refresh<DSARP>::Refresh(Controller<DSARP>* ctrl);
//template<> void Refresh<DSARP>::tick_ref();

} /* namespace ramulator */

#endif /* SRC_REFRESH_H_ */
