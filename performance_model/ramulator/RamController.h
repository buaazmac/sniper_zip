#ifndef __RAM_CONTROLLER_H
#define __RAM_CONTROLLER_H

#include <cassert>
#include <cstdio>
#include <deque>
#include <fstream>
#include <list>
#include <string>
#include <vector>

#include "RamConfig.h"
#include "RamDRAM.h"
#include "RamRefresh.h"
#include "RamRequest.h"
#include "RamScheduler.h"
#include "RamStatistics.h"

#include "HBM.h"

using namespace std;

namespace ramulator
{
class HBM;
class RamScheduler;
class RowTable;
class RowPolicy;

class RamController
{
public:
    // For counting bandwidth
    ScalarStat read_transaction_bytes;
    ScalarStat write_transaction_bytes;

    ScalarStat row_hits;
    ScalarStat row_misses;
    ScalarStat row_conflicts;
    VectorStat read_row_hits;
    VectorStat read_row_misses;
    VectorStat read_row_conflicts;
    VectorStat write_row_hits;
    VectorStat write_row_misses;
    VectorStat write_row_conflicts;

	VectorStat bank_reads;
	VectorStat bank_writes;
	VectorStat bank_read_row_hits;
	VectorStat bank_read_row_misses;
	VectorStat bank_read_row_conflicts;
	VectorStat bank_write_row_hits;
	VectorStat bank_write_row_misses;
	VectorStat bank_write_row_conflicts;

    ScalarStat read_latency_avg;
    ScalarStat read_latency_sum;

    ScalarStat req_queue_length_avg;
    ScalarStat req_queue_length_sum;
    ScalarStat read_req_queue_length_avg;
    ScalarStat read_req_queue_length_sum;
    ScalarStat write_req_queue_length_avg;
    ScalarStat write_req_queue_length_sum;

    /* Member Variables */
    long clk = 0;
    RamDRAM* channel;

    RamScheduler* scheduler;  // determines the highest priority request whose commands will be issued
    RowPolicy* rowpolicy;  // determines the row-policy (e.g., closed-row vs. open-row)
    RowTable* rowtable;  // tracks metadata about rows (e.g., which are open and for how long)
    Refresh* refresh;

    struct Queue {
        list<RamRequest> q;
        unsigned int max = 32;
        unsigned int size() {return q.size();}
    };

    Queue readq;  // queue for read requests
    Queue writeq;  // queue for write requests
    Queue otherq;  // queue for all "other" requests (e.g., refresh)

    deque<RamRequest> pending;  // read requests that are about to receive data from DRAM
    bool write_mode = false;  // whether write requests should be prioritized over reads
    //long refreshed = 0;  // last time refresh requests were generated

    /* Command trace for DRAMPower 3.1 */
    string cmd_trace_prefix = "cmd-trace-";
    vector<ofstream> cmd_trace_files;
    bool record_cmd_trace = false;
    /* Commands to stdout */
    bool print_cmd_trace = false;

    /* Constructor */

	RamController(const RamConfig & configs, RamDRAM * channel);

	~RamController();
    void finish(long read_req, long dram_cycles) {
      read_latency_avg = read_latency_sum.value() / read_req;
      req_queue_length_avg = req_queue_length_sum.value() / dram_cycles;
      read_req_queue_length_avg = read_req_queue_length_sum.value() / dram_cycles;
      write_req_queue_length_avg = write_req_queue_length_sum.value() / dram_cycles;
      // call finish function of each channel
      channel->finish(dram_cycles);
    }

    /* Member Functions */
    Queue& get_queue(RamRequest::Type type)
    {
        switch (int(type)) {
            case int(RamRequest::Type::READ): return readq;
            case int(RamRequest::Type::WRITE): return writeq;
            default: return otherq;
        }
    }

    bool enqueue(RamRequest& req)
    {
        Queue& queue = get_queue(req.type);
        if (queue.max == queue.size())
            return false;

        req.arrive = clk;
        queue.q.push_back(req);
        // shortcut for read requests, if a write to same addr exists
        // necessary for coherence
        if (req.type == RamRequest::Type::READ && find_if(writeq.q.begin(), writeq.q.end(),
                [req](RamRequest& wreq){ return req.addr == wreq.addr;}) != writeq.q.end()){
            req.depart = clk + 1;
            pending.push_back(req);
            readq.q.pop_back();
        }
        return true;
    }

	// Move to .cc because of forward declaration
	void tick();

	// ZMAC ADDED: Set refresh rate for banks
	void setBankRef(int bank_i, bool hot);

    bool is_ready(list<RamRequest>::iterator req)
    {
        Command cmd = get_first_cmd(req);
        return channel->check(cmd, req->addr_vec.data(), clk);
    }

    bool is_ready(Command cmd, const vector<int>& addr_vec)
    {
        return channel->check(cmd, addr_vec.data(), clk);
    }

    bool is_row_hit(list<RamRequest>::iterator req)
    {
        // cmd must be decided by the request type, not the first cmd
        Command cmd = channel->spec->translate[int(req->type)];
        return channel->check_row_hit(cmd, req->addr_vec.data());
    }

    bool is_row_hit(Command cmd, const vector<int>& addr_vec)
    {
        return channel->check_row_hit(cmd, addr_vec.data());
    }

    bool is_row_open(list<RamRequest>::iterator req)
    {
        // cmd must be decided by the request type, not the first cmd
        Command cmd = channel->spec->translate[int(req->type)];
        return channel->check_row_open(cmd, req->addr_vec.data());
    }

    bool is_row_open(Command cmd, const vector<int>& addr_vec)
    {
        return channel->check_row_open(cmd, addr_vec.data());
    }

	/*
    void update_temp(ALDRAM::Temp current_temperature)
    {
    }
	*/

    // For telling whether this channel is busying in processing read or write
    bool is_active() {
      return (channel->cur_serving_requests > 0);
    }

    // For telling whether this channel is under refresh
    bool is_refresh() {
      return clk <= channel->end_of_refreshing;
    }

    void record_core(int coreid) {

    }

private:
    Command get_first_cmd(list<RamRequest>::iterator req)
    {
        Command cmd = channel->spec->translate[int(req->type)];
        return channel->decode(cmd, req->addr_vec.data());
    }

	// Move to .cc because of forward decalaration
	void issue_cmd(Command cmd, const vector<int>& addr_vec);

    vector<int> get_addr_vec(Command cmd, list<RamRequest>::iterator req){
        return req->addr_vec;
    }
};

/*
template <>
vector<int> Controller<SALP>::get_addr_vec(
    SALP::Command cmd, list<Request>::iterator req);

template <>
bool Controller<SALP>::is_ready(list<Request>::iterator req);

template <>
void Controller<ALDRAM>::update_temp(ALDRAM::Temp current_temperature);

template <>
void Controller<TLDRAM>::tick();

*/

} /*namespace ramulator*/

#endif /*__CONTROLLER_H*/
