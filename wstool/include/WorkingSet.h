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
        int64_t address;
        int64_t brithTime;
        int64_t deathTime;
    } InternaladdressLiving;
    typedef struct InputaddressLiving
    {
        int64_t address;
        int64_t firstLoadTime;
        int64_t deathTime;
    } InputaddressLiving;
    extern int64_t timing;
    extern vector<InputaddressLiving> inputAddressLivingVec;
    extern vector<InternaladdressLiving> internalAddressLivingVec;
    extern uint64_t inputMapSize;
    extern uint64_t internalMapSize;
    void Process(std::string &key, std::string &value);
} // namespace WorkingSet