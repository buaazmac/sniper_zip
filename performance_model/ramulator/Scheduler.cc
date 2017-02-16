#include "Scheduler.h"
namespace ramulator {

Scheduler::Scheduler(Controller* ctrl) : ctrl(ctrl) {
    // FCFS
    compare[0] = [this] (ReqIter req1, ReqIter req2) {
            if (req1->arrive <= req2->arrive) return req1;
            return req2;};

    // FRFCFS
    compare[1] = [this] (ReqIter req1, ReqIter req2) {
            bool ready1 = this->ctrl->is_ready(req1);
            bool ready2 = this->ctrl->is_ready(req2);

            if (ready1 ^ ready2) {
                if (ready1) return req1;
                return req2;
            }

            if (req1->arrive <= req2->arrive) return req1;
            return req2;};

    // FRFCFS_CAP
    compare[2] = [this] (ReqIter req1, ReqIter req2) {
            bool ready1 = this->ctrl->is_ready(req1);
            bool ready2 = this->ctrl->is_ready(req2);

            ready1 = ready1 && (this->ctrl->rowtable->get_hits(req1->addr_vec) <= this->cap);
            ready2 = ready2 && (this->ctrl->rowtable->get_hits(req2->addr_vec) <= this->cap);

            if (ready1 ^ ready2) {
                if (ready1) return req1;
                return req2;
            }

            if (req1->arrive <= req2->arrive) return req1;
            return req2;};
    // FRFCFS_PriorHit
    compare[3] = [this] (ReqIter req1, ReqIter req2) {
            bool ready1 = this->ctrl->is_ready(req1) && this->ctrl->is_row_hit(req1);
            bool ready2 = this->ctrl->is_ready(req2) && this->ctrl->is_row_hit(req2);

            if (ready1 ^ ready2) {
                if (ready1) return req1;
                return req2;
            }

            if (req1->arrive <= req2->arrive) return req1;
            return req2;};
}

    list<Request>::iterator Scheduler::get_head(list<Request>& q)
    {
      // TODO make the decision at compile time
      if (type != Type::FRFCFS_PriorHit) {
        if (!q.size())
            return q.end();

        auto head = q.begin();
        for (auto itr = next(q.begin(), 1); itr != q.end(); itr++)
            head = compare[int(type)](head, itr);

        return head;
      } else {
        if (!q.size())
            return q.end();

        auto head = q.begin();
        for (auto itr = next(q.begin(), 1); itr != q.end(); itr++) {
            head = compare[int(Type::FRFCFS_PriorHit)](head, itr);
        }

        if (this->ctrl->is_ready(head) && this->ctrl->is_row_hit(head)) {
          return head;
        }

        // prepare a list of hit request
        vector<vector<int>> hit_reqs;
        for (auto itr = q.begin() ; itr != q.end() ; ++itr) {
          if (this->ctrl->is_row_hit(itr)) {
            auto begin = itr->addr_vec.begin();
            // TODO Here it assumes all DRAM standards use PRE to close a row
            // It's better to make it more general.
            auto end = begin + int(ctrl->channel->spec->scope[int(Command::PRE)]) + 1;
            vector<int> rowgroup(begin, end); // bank or subarray
            hit_reqs.push_back(rowgroup);
          }
        }
        // if we can't find proper request, we need to return q.end(),
        // so that no command will be scheduled
        head = q.end();
        for (auto itr = q.begin(); itr != q.end(); itr++) {
          bool violate_hit = false;
          if ((!this->ctrl->is_row_hit(itr)) && this->ctrl->is_row_open(itr)) {
            // so the next instruction to be scheduled is PRE, might violate hit
            auto begin = itr->addr_vec.begin();
            // TODO Here it assumes all DRAM standards use PRE to close a row
            // It's better to make it more general.
            auto end = begin + int(ctrl->channel->spec->scope[int(Command::PRE)]) + 1;
            vector<int> rowgroup(begin, end); // bank or subarray
            for (const auto& hit_req_rowgroup : hit_reqs) {
              if (rowgroup == hit_req_rowgroup) {
                  violate_hit = true;
                  break;
              }
            }
          }
          if (violate_hit) {
            continue;
          }
          // If it comes here, that means it won't violate any hit request
          if (head == q.end()) {
            head = itr;
          } else {
            head = compare[int(Type::FRFCFS)](head, itr);
          }
        }

        return head;
      }
    }

RowPolicy::RowPolicy(Controller* ctrl) : ctrl(ctrl) {
    // Closed
    policy[0] = [this] (Command cmd) -> vector<int> {
            for (auto& kv : this->ctrl->rowtable->table) {
                if (!this->ctrl->is_ready(cmd, kv.first))
                    continue;
                return kv.first;
            }
            return vector<int>();};

    // Opened
    policy[1] = [this] (Command cmd) {
            return vector<int>();};

    // Timeout
    policy[2] = [this] (Command cmd) -> vector<int> {
            for (auto& kv : this->ctrl->rowtable->table) {
                auto& entry = kv.second;
                if (this->ctrl->clk - entry.timestamp < timeout)
                    continue;
                if (!this->ctrl->is_ready(cmd, kv.first))
                    continue;
                return kv.first;
            }
            return vector<int>();};
}

RowTable::RowTable(Controller* ctrl) : ctrl(ctrl) {
}

    void RowTable::update(Command cmd, const vector<int>& addr_vec, long clk)
    {
        auto begin = addr_vec.begin();
        auto end = begin + int(Level::Row);
        vector<int> rowgroup(begin, end); // bank or subarray
        int row = *end;

        HBM* spec = ctrl->channel->spec;

        if (spec->is_opening(cmd))
            table.insert({rowgroup, {row, 0, clk}});

        if (spec->is_accessing(cmd)) {
            // we are accessing a row -- update its entry
            auto match = table.find(rowgroup);
            assert(match != table.end());
            assert(match->second.row == row);
            match->second.hits++;
            match->second.timestamp = clk;
        } /* accessing */

        if (spec->is_closing(cmd)) {
          // we are closing one or more rows -- remove their entries
          int n_rm = 0;
          int scope = int(spec->scope[int(cmd)]);
          for (auto it = table.begin(); it != table.end();) {
            if (equal(begin, begin + scope + 1, it->first.begin())) {
              n_rm++;
              it = table.erase(it);
            }
            else
              it++;
          }
          assert(n_rm > 0);
        } /* closing */
    }

    int RowTable::get_hits(vector<int>& addr_vec)
    {
        auto begin = addr_vec.begin();
        auto end = begin + int(Level::Row);

        vector<int> rowgroup(begin, end);
        int row = *end;

        auto itr = table.find(rowgroup);
        auto entry = table[rowgroup];
        if (itr == table.end() || entry.row != row)
            return 0;

        return entry.hits;
    }
}
