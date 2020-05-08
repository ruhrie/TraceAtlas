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
        int64_t brithTime;
        int64_t deathTime;
    } InternaladdressLiving;
    extern vector<InternaladdressLiving> internalAddressLivingVec;
    extern uint64_t inputMapSize;
    extern uint64_t internalMapSize;
    extern uint64_t maxInternalSize;
    extern uint64_t maxOutputSize;
    void Process(std::string &key, std::string &value);
} // namespace WorkingSet