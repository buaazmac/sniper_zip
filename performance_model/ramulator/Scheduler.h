#ifndef __SCHEDULER_H
#define __SCHEDULER_H

#include "DRAM.h"
#include "Request.h"
#include "Controller.h"
#include <vector>
#include <map>
#include <list>
#include <functional>
#include <cassert>

#include "HBM.h"

using namespace std;

namespace ramulator
{

class Controller;

class Scheduler
{
public:
    Controller* ctrl;

    enum class Type {
        FCFS, FRFCFS, FRFCFS_Cap, FRFCFS_PriorHit, MAX
    } type = Type::FRFCFS_PriorHit;

    long cap = 16;

    Scheduler(Controller* ctrl);

    list<Request>::iterator get_head(list<Request>& q);

private:
    typedef list<Request>::iterator ReqIter;
    function<ReqIter(ReqIter, ReqIter)> compare[int(Type::MAX)];
};


class RowPolicy
{
public:
    Controller* ctrl;

    enum class Type {
        Closed, Opened, Timeout, MAX
    } type = Type::Opened;

    int timeout = 50;

    RowPolicy(Controller* ctrl);

    vector<int> get_victim(Command cmd)
    {
        return policy[int(type)](cmd);
    }

private:
    function<vector<int>(Command)> policy[int(Type::MAX)];
};


class RowTable
{
public:
    Controller* ctrl;

    struct Entry {
        int row;
        int hits;
        long timestamp;
    };

    map<vector<int>, Entry> table;

    RowTable(Controller* ctrl);

    void update(Command cmd, const vector<int>& addr_vec, long clk);
    
    int get_hits(vector<int>& addr_vec);
};

} /*namespace ramulator*/

#endif /*__SCHEDULER_H*/
