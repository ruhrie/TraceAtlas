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
    extern uint64_t internalMapSize;
    extern set<uint64_t> outputAddressIndexSet;
    void Process(std::string &key, std::string &value);
} // namespace WorkingSet