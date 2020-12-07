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
        uint64_t addr;
        bool dep;
        int64_t birthTime;
        int64_t deathTime;
    } InternaladdressLiving;

    typedef struct KernelWorkingSet
    {
        map<uint64_t, uint64_t> internalAddressIndexMap;
        vector<InternaladdressLiving> internalAddressLivingVec;
        set<uint64_t> outputAddressIndexSet;
        set<uint64_t> inputAddressIndexSet;
        set<uint64_t> internalAddressIndexSet;
        uint64_t internalMapSize;
    } KernelWorkingSet;

    typedef struct KernelLiveness
    {
        int64_t birthTime;
        int64_t deathTime;
    } KernelLiveness;

    extern map<uint64_t,KernelWorkingSet> KernelWorkingSetMap;
    extern int64_t timing;

    extern map<uint64_t,vector<uint64_t>> internalTimeStamp;
    extern map<uint64_t,uint64_t> importantAddrToDatasize;
    extern vector<InternaladdressLiving> internalAddressLivingVec;
    extern uint64_t internalMapSize;
    extern set<uint64_t> outputAddressIndexSet;
    extern map<uint64_t,uint64_t> maxinternalfiring;
    
    // for data size restoring
    extern map<uint64_t,uint64_t> importantAddrToDatasize;


    //for kernel liveness
    extern map<uint64_t,KernelLiveness> KernelLivenessMap;
    void Process(std::string &key, std::string &value);
    void ProcessBlock(std::string &key, std::string &value);


} // namespace WorkingSet