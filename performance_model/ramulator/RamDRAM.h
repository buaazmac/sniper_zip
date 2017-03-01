#ifndef __RAM_DRAM_H
#define __RAM_DRAM_H

#include "RamStatistics.h"
#include <iostream>
#include <vector>
#include <deque>
#include <map>
#include <functional>
#include <algorithm>
#include <cassert>
#include <type_traits>

#include "HBM.h"

using namespace std;

namespace ramulator
{
class HBM;
enum class Level;
enum class Command;
enum class State;
struct TimingEntry;
enum class Org;
enum class Speed;

class RamDRAM
{
public:
    ScalarStat active_cycles;
    ScalarStat refresh_cycles;
    ScalarStat busy_cycles;
    ScalarStat active_refresh_overlap_cycles;

    ScalarStat serving_requests;
    ScalarStat average_serving_requests;

	ScalarStat serving_reads;
	ScalarStat serving_writes;

    // Constructor
    RamDRAM(HBM* spec, Level level);
    ~RamDRAM();

    // Specification (e.g., DDR3)
    HBM* spec;

    // Tree Organization (e.g., Channel->Rank->Bank->Row->Column)
    Level level;
    int id;
    long size;
    RamDRAM* parent;
    vector<RamDRAM*> children;

    // State (e.g., Opened, Closed)
    State state;

    // State of Rows:
    // There are too many rows for them to be instantiated individually
    // Instead, their bank (or an equivalent entity) tracks their state for them
    map<int, State> row_state;

    // Insert a node as one of my child nodes
    void insert(RamDRAM* child);

    // Decode a command into its "prerequisite" command (if any is needed)
    Command decode(Command cmd, const int* addr);

    // Check whether a command is ready to be scheduled
    bool check(Command cmd, const int* addr, long clk);

    // Check whether a command is a row hit
    bool check_row_hit(Command cmd, const int* addr);

    // Check whether a row is open
    bool check_row_open(Command cmd, const int* addr);

    // Return the earliest clock when a command is ready to be scheduled
    long get_next(Command cmd, const int* addr);

    // Update the timing/state of the tree, signifying that a command has been issued
    void update(Command cmd, const int* addr, long clk);
    // Update statistics:

    // Update the number of requests it serves currently
    void update_serving_requests(const int* addr, int delta, long clk);

    // TIANSHI: current serving requests count
    int cur_serving_requests = 0;
    long begin_of_serving = -1;
    long end_of_serving = -1;
    long begin_of_cur_reqcnt = -1;
    long begin_of_refreshing = -1;
    long end_of_refreshing = -1;
    std::vector<std::pair<long, long>> refresh_intervals;

    // register statistics
    void regStats(const std::string& identifier);

    void finish(long dram_cycles);

private:
    // Constructor
    RamDRAM() {};

    // Timing
    long cur_clk = 0;
    //long next[int(Command::MAX)]; // the earliest time in the future when a command could be ready
    //deque<long> prev[int(Command::MAX)]; // the most recent history of when commands were issued
    long next[13]; // the earliest time in the future when a command could be ready
    deque<long> prev[13]; // the most recent history of when commands were issued

    // Lookup table for which commands must be preceded by which other commands (i.e., "prerequisite")
    // E.g., a read command to a closed bank must be preceded by an activate command
    function<Command(RamDRAM*, Command cmd, int)>* prereq;

    // SAUGATA: added table for row hits
    // Lookup table for whether a command is a row hit
    // E.g., a read command to a closed bank must be preceded by an activate command
    function<bool(RamDRAM*, Command cmd, int)>* rowhit;
    function<bool(RamDRAM*, Command cmd, int)>* rowopen;

    // Lookup table between commands and the state transitions they trigger
    // E.g., an activate command to a closed bank opens both the bank and the row
    function<void(RamDRAM*, int)>* lambda;

    // Lookup table for timing parameters
    // E.g., activate->precharge: tRAS@bank, activate->activate: tRC@bank
    vector<TimingEntry>* timing;

    // Helper Functions
    void update_state(Command cmd, const int* addr);
    void update_timing(Command cmd, const int* addr, long clk);
}; /* class DRAM */


} /* namespace ramulator */

#endif /* __DRAM_H */
