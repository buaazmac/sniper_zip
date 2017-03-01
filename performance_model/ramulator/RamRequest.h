#ifndef __RAM_REQUEST_H
#define __RAM_REQUEST_H

#include <vector>
#include <functional>

using namespace std;

namespace ramulator
{

class RamRequest
{
public:
    bool is_first_command;
    long addr;
    // long addr_row;
    vector<int> addr_vec;
    // specify which core this request sent from, for virtual address translation
    int coreid;

    enum class Type
    {
        READ,
        WRITE,
        REFRESH,
        POWERDOWN,
        SELFREFRESH,
        EXTENSION,
        MAX
    } type;

    long arrive = -1;
    long depart;
    function<void(RamRequest&)> callback; // call back with more info

    RamRequest(long addr, Type type, int coreid = 0)
        : is_first_command(true), addr(addr), coreid(coreid), type(type),
      callback([](RamRequest& req){}) {}

    RamRequest(long addr, Type type, function<void(RamRequest&)> callback, int coreid = 0)
        : is_first_command(true), addr(addr), coreid(coreid), type(type), callback(callback) {}

    RamRequest(vector<int>& addr_vec, Type type, function<void(RamRequest&)> callback, int coreid = 0)
        : is_first_command(true), addr_vec(addr_vec), coreid(coreid), type(type), callback(callback) {}

    RamRequest()
        : is_first_command(true), coreid(0) {}
};

} /*namespace ramulator*/

#endif /*__REQUEST_H*/

