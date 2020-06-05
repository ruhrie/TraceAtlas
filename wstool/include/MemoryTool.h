//extern std::set<uint64_t> kernelBlockValue;
extern map<uint64_t, set<uint64_t>> kernelMap;
extern map<uint64_t, uint64_t> BBCount;
extern map<uint64_t, uint64_t> SetBBCount;
extern set<uint64_t> BBSet;
extern int option;
typedef struct CombinedKernelWorkingSet
{
    map<uint64_t, uint64_t> internalAddressIndexMap;
    vector<InternaladdressLiving> internalAddressLivingVec;
    set<uint64_t> outputAddressIndexSet;
    set<uint64_t> inputAddressIndexSet;
    uint64_t inputMapSize;
    uint64_t internalMapSize;
} CombinedKernelWorkingSet;