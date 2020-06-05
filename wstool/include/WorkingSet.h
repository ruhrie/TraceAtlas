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

    typedef struct KernelWorkingSet
    {
        map<uint64_t, uint64_t> internalAddressIndexMap;
        vector<InternaladdressLiving> internalAddressLivingVec;
        set<uint64_t> outputAddressIndexSet;
        set<uint64_t> inputAddressIndexSet;
        uint64_t inputMapSize;
        uint64_t internalMapSize;
    } KernelWorkingSet;


    typedef struct BlockWorkingSet
    {
        map<uint64_t, uint64_t> internalAddressIndexMap;
        vector<InternaladdressLiving> internalAddressLivingVec;
        set<uint64_t> outputAddressIndexSet;
        set<uint64_t> inputAddressIndexSet;
        uint64_t inputMapSize;
        uint64_t internalMapSize;
    } BlockWorkingSet;

    extern map<uint64_t,BlockWorkingSet> BlockWorkingSetMap;
    extern map<uint64_t,KernelWorkingSet> KernelWorkingSetMap;
    extern int64_t timing;

    void Process(std::string &key, std::string &value);
} // namespace WorkingSet