/*
 * Refresh.cpp
 *
 * Mainly DSARP specialization at the moment.
 *
 *  Created on: Mar 17, 2015
 *      Author: kevincha
 */

#include <stdlib.h>

#include "Refresh.h"
#include "Controller.h"
#include "DRAM.h"

using namespace std;
using namespace ramulator;

namespace ramulator {
  // ctor
	Refresh::Refresh(Controller* ctrl) : ctrl(ctrl) {
    clk = refreshed = 0;
    max_rank_count = ctrl->channel->children.size();
    max_bank_count = ctrl->channel->spec->org_entry.count[(int)Level::Bank];

    // Init refresh counters
    for (int r = 0; r < max_rank_count; r++) {
      bank_ref_counters.push_back(0);
      bank_refresh_backlog.push_back(new vector<int>(max_bank_count, 0));
    }

    level_chan = (int)Level::Channel;
    level_rank = (int)Level::Rank;
    level_bank = (int)Level::Bank;
    level_sa   = -1; // Most DRAM doesn't have subarray level
  }
  // Basic refresh scheduling for all bank refresh that is applicable to all DRAM types
  void Refresh::tick_ref() {
    clk++;

    int refresh_interval = ctrl->channel->spec->speed_entry.nREFI;

    // Time to schedule a refresh
    if ((clk - refreshed) >= refresh_interval) {
      inject_refresh(true);
      // ALDRAM: update timing parameters based on temperatures
      //ALDRAM::Temp current_temperature = ALDRAM::Temp::COLD;
      //ctrl->update_temp(current_temperature);
    }
  }
  // Refresh based on the specified address
  void Refresh::refresh_target(Controller* ctrl, int rank, int bank, int sa)
  {
    vector<int> addr_vec(int(Level::MAX), -1);
    addr_vec[0] = ctrl->channel->id;
    addr_vec[1] = rank;
    addr_vec[2] = bank;
    addr_vec[3] = sa;
    Request req(addr_vec, Request::Type::REFRESH, NULL);
    bool res = ctrl->enqueue(req);
    assert(res);
  }
  // Inject refresh at either rank or bank level
  void Refresh::inject_refresh(bool b_ref_rank) {
    // Rank-level refresh
    if (b_ref_rank) {
      for (auto rank : ctrl->channel->children)
        refresh_target(ctrl, rank->id, -1, -1);
    }
    // Bank-level refresh. Simultaneously issue to all ranks (better performance than staggered refreshes).
    else {
      for (auto rank : ctrl->channel->children)
        refresh_target(ctrl, rank->id, bank_ref_counters[rank->id], -1);
    }
    refreshed = clk;
  }
} /* namespace ramulator */
