#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>

using namespace std;

namespace WorkingSet
{
    typedef struct InternaladdressLiving
    {
        uint64_t address;
        int64_t birthTime;
        int64_t deathTime;
    } InternaladdressLiving;
    extern int64_t timing;
    extern vector<InternaladdressLiving> internalAddressLivingVec;
    extern uint64_t inputMapSize;
    extern uint64_t maxinternal;

    extern map<uint64_t, int64_t> AddrEndtimeMap;
    void Process(std::string &key, std::string &value);
    void ProcessFirst(string &key, string &value);
} // namespace WorkingSet