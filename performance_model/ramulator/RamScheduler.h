#ifndef __RAM_SCHEDULER_H
#define __RAM_SCHEDULER_H

#include "RamDRAM.h"
#include "RamRequest.h"
#include "RamController.h"
#include <vector>
#include <map>
#include <list>
#include <functional>
#include <cassert>

#include "HBM.h"

using namespace std;

namespace ramulator
{

class RamController;

class RamScheduler
{
public:
    RamController* ctrl;

    enum class Type {
        FCFS, FRFCFS, FRFCFS_Cap, FRFCFS_PriorHit, MAX
    } type = Type::FRFCFS_PriorHit;

    long cap = 16;

    RamScheduler(RamController* ctrl);

    list<RamRequest>::iterator get_head(list<RamRequest>& q);

private:
    typedef list<RamRequest>::iterator ReqIter;
    function<ReqIter(ReqIter, ReqIter)> compare[int(Type::MAX)];
};


class RowPolicy
{
public:
    RamController* ctrl;

    enum class Type {
        Closed, Opened, Timeout, MAX
    } type = Type::Opened;

    int timeout = 50;

    RowPolicy(RamController* ctrl);

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
    RamController* ctrl;

    struct Entry {
        int row;
        int hits;
        long timestamp;
    };

    map<vector<int>, Entry> table;

    RowTable(RamController* ctrl);

    void update(Command cmd, const vector<int>& addr_vec, long clk);
    
    int get_hits(vector<int>& addr_vec);
};

} /*namespace ramulator*/

#endif /*__SCHEDULER_H*/
