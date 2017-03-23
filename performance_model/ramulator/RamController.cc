#include "RamController.h"

using namespace ramulator;

namespace ramulator
{

	RamController::RamController(const RamConfig& configs, RamDRAM* channel) :
        channel(channel),
        scheduler(new RamScheduler(this)),
        rowpolicy(new RowPolicy(this)),
        rowtable(new RowTable(this)),
        refresh(new Refresh(this)),
        cmd_trace_files(channel->children.size())
    {
        record_cmd_trace = configs.record_cmd_trace();
        print_cmd_trace = configs.print_cmd_trace();
        if (record_cmd_trace){
            if (configs["cmd_trace_prefix"] != "") {
              cmd_trace_prefix = configs["cmd_trace_prefix"];
            }
            string prefix = cmd_trace_prefix + "chan-" + to_string(channel->id) + "-rank-";
            string suffix = ".cmdtrace";
            for (unsigned int i = 0; i < channel->children.size(); i++)
                cmd_trace_files[i].open(prefix + to_string(i) + suffix);
        }

        // regStats

        row_hits
            .name("row_hits_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row hits per channel per core")
            .precision(0)
            ;
        row_misses
            .name("row_misses_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row misses per channel per core")
            .precision(0)
            ;
        row_conflicts
            .name("row_conflicts_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row conflicts per channel per core")
            .precision(0)
            ;

        read_row_hits
            .init(configs.get_core_num())
            .name("read_row_hits_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row hits for read requests per channel per core")
            .precision(0)
            ;
        read_row_misses
            .init(configs.get_core_num())
            .name("read_row_misses_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row misses for read requests per channel per core")
            .precision(0)
            ;
        read_row_conflicts
            .init(configs.get_core_num())
            .name("read_row_conflicts_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row conflicts for read requests per channel per core")
            .precision(0)
            ;

        write_row_hits
            .init(configs.get_core_num())
            .name("write_row_hits_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row hits for write requests per channel per core")
            .precision(0)
            ;
        write_row_misses
            .init(configs.get_core_num())
            .name("write_row_misses_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row misses for write requests per channel per core")
            .precision(0)
            ;
        write_row_conflicts
            .init(configs.get_core_num())
            .name("write_row_conflicts_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row conflicts for write requests per channel per core")
            .precision(0)
            ;
		/* Bank Stats*/
		bank_reads
			.init(configs.get_ranks() * 2)
			.name("read_channel_" + to_string(channel->id) + "_core")
			.desc("Number of reads for read requests per bank per channel per core")
			.precision(0)
			;
		bank_writes
			.init(configs.get_ranks() * 2)
			.name("write_channel_" + to_string(channel->id) + "_core")
			.desc("Number of writes for read requests per bank per channel per core")
			.precision(0)
			;
		bank_read_row_hits
			.init(configs.get_ranks() * 2)
			.name("read_row_hits_channel_" + to_string(channel->id) + "_core")
			.desc("Number of row hits for read requests per bank per channel per core")
			.precision(0)
			;
		bank_read_row_misses
			.init(configs.get_ranks() * 2)
			.name("read_row_misses_channel_" + to_string(channel->id) + "_core")
			.desc("Number of row misses for read requests per bank per channel per core")
			.precision(0)
			;
		bank_read_row_conflicts
			.init(configs.get_ranks() * 2)
			.name("read_row_conflicts_channel_" + to_string(channel->id) + "_core")
			.desc("Number of row conflicts for read requests per bank per channel per core")
			.precision(0)
			;

		bank_write_row_hits
			.init(configs.get_ranks() * 2)
			.name("write_row_hits_channel_" + to_string(channel->id) + "_core")
			.desc("Number of row hits for write requests per bank per channel per core")
			.precision(0)
			;
		bank_write_row_misses
			.init(configs.get_ranks() * 2)
			.name("write_row_misses_channel_" + to_string(channel->id) + "_core")
			.desc("Number of row misses for write requests per bank per channel per core")
			.precision(0)
			;
		bank_write_row_conflicts
			.init(configs.get_ranks() * 2)
			.name("write_row_conflicts_channel_" + to_string(channel->id) + "_core")
			.desc("Number of row conflicts for write requests per bank per channel per core")
			.precision(0)
			;
		/* End of Bank Stats*/

        read_transaction_bytes
            .name("read_transaction_bytes_"+to_string(channel->id))
            .desc("The total byte of read transaction per channel")
            .precision(0)
            ;
        write_transaction_bytes
            .name("write_transaction_bytes_"+to_string(channel->id))
            .desc("The total byte of write transaction per channel")
            .precision(0)
            ;

        read_latency_sum
            .name("read_latency_sum_"+to_string(channel->id))
            .desc("The memory latency cycles (in memory time domain) sum for all read requests in this channel")
            .precision(0)
            ;
        read_latency_avg
            .name("read_latency_avg_"+to_string(channel->id))
            .desc("The average memory latency cycles (in memory time domain) per request for all read requests in this channel")
            .precision(6)
            ;

        req_queue_length_sum
            .name("req_queue_length_sum_"+to_string(channel->id))
            .desc("Sum of read and write queue length per memory cycle per channel.")
            .precision(0)
            ;
        req_queue_length_avg
            .name("req_queue_length_avg_"+to_string(channel->id))
            .desc("Average of read and write queue length per memory cycle per channel.")
            .precision(6)
            ;

        read_req_queue_length_sum
            .name("read_req_queue_length_sum_"+to_string(channel->id))
            .desc("Read queue length sum per memory cycle per channel.")
            .precision(0)
            ;
        read_req_queue_length_avg
            .name("read_req_queue_length_avg_"+to_string(channel->id))
            .desc("Read queue length average per memory cycle per channel.")
            .precision(6)
            ;

        write_req_queue_length_sum
            .name("write_req_queue_length_sum_"+to_string(channel->id))
            .desc("Write queue length sum per memory cycle per channel.")
            .precision(0)
            ;
        write_req_queue_length_avg
            .name("write_req_queue_length_avg_"+to_string(channel->id))
            .desc("Write queue length average per memory cycle per channel.")
            .precision(6)
            ;

    }
	RamController::~RamController() {
        delete scheduler;
        delete rowpolicy;
        delete rowtable;
        delete channel;
        delete refresh;
        for (auto& file : cmd_trace_files)
            file.close();
        cmd_trace_files.clear();
    }

    void RamController::tick()
    {
        clk++;
        req_queue_length_sum += readq.size() + writeq.size() + pending.size();
        read_req_queue_length_sum += readq.size() + pending.size();
        write_req_queue_length_sum += writeq.size();

        /*** 1. Serve completed reads ***/
        if (pending.size()) {
            RamRequest& req = pending[0];
            if (req.depart <= clk) {
                if (req.depart - req.arrive > 1) { // this request really accessed a row
                  read_latency_sum += req.depart - req.arrive;
                  channel->update_serving_requests(
                      req.addr_vec.data(), -1, clk);
                }
                req.callback(req);
                pending.pop_front();
            }
        }

        /*** 2. Refresh scheduler ***/
        refresh->tick_ref();

        /*** 3. Should we schedule writes? ***/
        if (!write_mode) {
            // yes -- write queue is almost full or read queue is empty
            if (writeq.size() >= int(0.8 * writeq.max) || readq.size() == 0)
                write_mode = true;
        }
        else {
            // no -- write queue is almost empty and read queue is not empty
            if (writeq.size() <= int(0.2 * writeq.max) && readq.size() != 0)
                write_mode = false;
        }

        /*** 4. Find the best command to schedule, if any ***/
        Queue* queue = !write_mode ? &readq : &writeq;
        if (otherq.size())
            queue = &otherq;  // "other" requests are rare, so we give them precedence over reads/writes

        auto req = scheduler->get_head(queue->q);
        if (req == queue->q.end() || !is_ready(req)) {
            // we couldn't find a command to schedule -- let's try to be speculative
            auto cmd = Command::PRE;
            vector<int> victim = rowpolicy->get_victim(cmd);
            if (!victim.empty()){
                issue_cmd(cmd, victim);
            }
            return;  // nothing more to be done this cycle
        }
		// ZMAC_ADDED: bank stats
		int bank_i = req->addr_vec[int(Level::Rank)] * 2 + req->addr_vec[int(Level::Bank)];
		if (req->type == RamRequest::Type::READ) {
			// ZMAC_ADDED: bank stats
			++bank_reads[bank_i];
		}
		else if (req->type == RamRequest::Type::WRITE) {
			// ZMAC_ADDED: bank stats
			++bank_writes[bank_i];
		}

        if (req->is_first_command) {
            req->is_first_command = false;
            int coreid = req->coreid;
            if (req->type == RamRequest::Type::READ || req->type == RamRequest::Type::WRITE) {
              channel->update_serving_requests(req->addr_vec.data(), 1, clk);
            }
            int tx = (channel->spec->prefetch_size * channel->spec->channel_width / 8);
			
            if (req->type == RamRequest::Type::READ) {
                if (is_row_hit(req)) {
                    ++read_row_hits[coreid];
                    ++row_hits;
                } else if (is_row_open(req)) {
                    ++read_row_conflicts[coreid];
                    ++row_conflicts;
                } else {
                    ++read_row_misses[coreid];
                    ++row_misses;
                }
              read_transaction_bytes += tx;
            } else if (req->type == RamRequest::Type::WRITE) {
				if (is_row_hit(req)) {
					++write_row_hits[coreid];
					++row_hits;
				} else if (is_row_open(req)) {
					++write_row_conflicts[coreid];
					++row_conflicts;
				} else {
					++write_row_misses[coreid];
					++row_misses;
				}
				write_transaction_bytes += tx;
			}
        }

        // issue command on behalf of request
        auto cmd = get_first_cmd(req);
        issue_cmd(cmd, get_addr_vec(cmd, req));

        // check whether this is the last command (which finishes the request)
        if (cmd != channel->spec->translate[int(req->type)])
            return;

        // set a future completion time for read requests
        if (req->type == RamRequest::Type::READ) {
            req->depart = clk + channel->spec->read_latency;
            pending.push_back(*req);
        }

        if (req->type == RamRequest::Type::WRITE) {
            channel->update_serving_requests(req->addr_vec.data(), -1, clk);
        }

        // remove request from queue
        queue->q.erase(req);
    }

	void RamController::setBankRef(int bank_i, bool hot)
	{
		refresh->set_ref_interval(bank_i, hot);
	}

    void RamController::issue_cmd(Command cmd, const vector<int>& addr_vec)
    {
        assert(is_ready(cmd, addr_vec));
        channel->update(cmd, addr_vec.data(), clk);
        rowtable->update(cmd, addr_vec, clk);
        if (record_cmd_trace){
            // select rank
            auto& file = cmd_trace_files[addr_vec[1]];
            string& cmd_name = channel->spec->command_name[int(cmd)];
            file<<clk<<','<<cmd_name;
            // TODO bad coding here
            if (cmd_name == "PREA" || cmd_name == "REF")
                file<<endl;
            else{
                int bank_id = addr_vec[int(Level::Bank)];
                if (channel->spec->standard_name == "DDR4" || channel->spec->standard_name == "GDDR5")
                    bank_id += addr_vec[int(Level::Bank) - 1] * channel->spec->org_entry.count[int(Level::Bank)];
                file<<','<<bank_id<<endl;
            }
        }
        if (print_cmd_trace){
            printf("%5s %10ld:", channel->spec->command_name[int(cmd)].c_str(), clk);
            for (int lev = 0; lev < int(Level::MAX); lev++)
                printf(" %5d", addr_vec[lev]);
            printf("\n");
        }
    }

} /* namespace ramulator */
