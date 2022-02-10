#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Traces.h"
#include "llvm/ADT/SmallVector.h"
#include <algorithm>
#include <fstream>
#include <indicators/progress_bar.hpp>
#include <iostream>
#include <llvm/Analysis/DependenceAnalysis.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <time.h>
#include <tuple>
#include <vector>
#include <zlib.h>

using namespace llvm;
using namespace std;

// This file is used to generate the input/output relationship between kernel instances and serial of kernel instances

cl::opt<std::string> InputFilename("t", cl::desc("Specify input trace"), cl::value_desc("trace filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"), cl::Required);
cl::opt<std::string> KernelFilename("k", cl::desc("Specify kernel json"), cl::value_desc("kernel filename"), cl::Required);
llvm::cl::opt<string> bitcodeFile("b", llvm::cl::desc("Specify bitcode name"), llvm::cl::value_desc("bitcode filename"), llvm::cl::Required);
cl::opt<int> nth("n", cl::desc("nth"), cl::value_desc("nth"), cl::Required);
cl::opt<float> eth("e", cl::desc("eth"), cl::value_desc("eth"), cl::Required);
cl::opt<int> en("en", cl::desc("en"), cl::value_desc("en"), cl::Required);
cl::opt<int> koi("koi", cl::desc("koi"), cl::value_desc("koi"), cl::Required);

llvm::cl::opt<bool> noBar("nb", llvm::cl::desc("No progress bar"), llvm::cl::value_desc("No progress bar"));

typedef struct wsTuple
{
    uint64_t start;
    uint64_t end;
    uint64_t byte_count;
    uint64_t ref_count;
    float reuse_distance;
    uint64_t timing;
} wsTuple;
typedef map<int64_t, wsTuple> wsTupleMap;

map<int, wsTupleMap> loadwsTupleMap;
map<int, wsTupleMap> storewsTupleMap;
map<int, wsTupleMap> loadAfterStorewsTupleMap;

wsTupleMap loadwsTuples;
wsTupleMap storewsTuples;
wsTupleMap loadAftertorewsTuples;

set<int> loadlastHitTimeSet;
set<int> storelastHitTimeSet;
set<int> loadAfterStorelastHitTimeSet;

typedef struct livenessTuple
{
    int64_t start;
    int64_t end;
} livenessTuple;

// addr -> related time tuples
vector<livenessTuple> livenessTupleVec;
// addr -> tuple to be update in the vector
map<uint64_t, int> livenessUpdate;
map<int, int> maxLivenessPerKI;

class kernel
{
    vector<wsTuple> Tuples;

public:
    void addTuple(wsTuple wt)
    {
        Tuples.push_back(wt);
    }
};
class kernelInstance : kernel
{
    //private:
};
class application
{
    vector<kernelInstance> kernelInstances;

public:
    void addInstance(kernelInstance ki)
    {
        kernelInstances.push_back(ki);
    }
};

class inputInfos
{
};
class KernelSerial
{
private:
    /* data */
public:
    KernelSerial(/* args */);
    ~KernelSerial();
};

KernelSerial::KernelSerial(/* args */)
{
}

KernelSerial::~KernelSerial()
{
}

// kernel instance id
static int UID = 0;

// kernel id
string currentKernel = "-1";
int currentUid = -1;
bool noerrorInTrace;

// kernel instance id that only have internal addresses
set<int> internalSet;
// kernel instance id -> kernel id
map<int, string> kernelIdMap;
// kernel id -> basic block id
map<string, set<int>> kernelMap;

// for data size

map<int64_t, vector<uint64_t>> BBMemInstSize;
// start end byte_count ref_count

// uint64_t start;
// uint64_t end;
// uint64_t byte_count;
// uint64_t ref_count;
// uint64_t reuse_dist;
typedef tuple<int64_t, int64_t, int64_t, vector<uint64_t>> AddrRange;
typedef map<int64_t, AddrRange> AddrRangeMap;
map<int, AddrRangeMap> loadAddrRangeMapPerInstance;
map<int, AddrRangeMap> storeAddrRangeMapPerInstance;

uint64_t timing = 0;
uint64_t timingIn = 0;

uint64_t livenessTiming = 0;
set<int64_t> ValidBlock;
int vBlock;
int64_t instCounter = 0;

// for register address prediction
map<uint64_t, uint64_t> registerBuffer;
set<uint64_t> registerVariable;
uint64_t memfootPrintInKI; //memory footprint recording for register buffer cleaning

// for non-trivial merge performance testing
int NumTrivialMerge = 0;
int NumNonTrivialMerge = 0;

int instNum = 0;
int peakStoreTNum = 0;
int peakLoadTNum = 0;
double ErrOverAll = 0.0;
int TupleNumChangedNonTri = 0;

int NontriOpen = true;
float NonTriErrTh = 0.5;
int NontriTh = 10;

// bb id, inst id, set of tuple, bb id, inst id
map<int64_t, map<int64_t, set<tuple<int64_t, int64_t>>>> loadStaticLookupTable;
map<int64_t, map<int64_t, set<tuple<int64_t, int64_t>>>> storeStaticLookupTable;

bool overlap(wsTuple a, wsTuple b, int64_t error)
{
    if (max(a.start, b.start) <= (min(a.end, b.end)) + error)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool included(wsTuple newer, wsTuple existed)
{
    return (newer.start >= existed.start && newer.end <= existed.end && (newer.start + newer.end - existed.end - existed.start != 0));
}

wsTuple tp_or(wsTuple a, wsTuple b, bool dynamic, set<int> &lastHitTimeSet)
{

    wsTuple wksTuple;
    float reuse_distance = 0;
    if (dynamic)
    {

        // last time hit tuple is not the same with current tuple
        if (a.start != timingIn && b.start != timingIn)
        {
            timing++;
            int timingRemove;
            if (a.timing == 0)
            {
                a.timing = timing;
                lastHitTimeSet.insert(timing);
                timingRemove = b.timing;
            }
            else if (b.timing == 0)
            {
                b.timing = timing;
                lastHitTimeSet.insert(timing);
                timingRemove = a.timing;
            }

            auto timingLarge = lastHitTimeSet.find(timing);
            auto timingSmall = lastHitTimeSet.find(timingRemove);

            reuse_distance = distance(timingSmall, timingLarge);

            reuse_distance = (reuse_distance + a.reuse_distance * a.ref_count + b.reuse_distance * b.ref_count) / (a.ref_count + b.ref_count);

            lastHitTimeSet.erase(timingRemove);

        } // last time hit tuple is the same with current tuple
        else
        {
            reuse_distance = (reuse_distance + a.reuse_distance * a.ref_count + b.reuse_distance * b.ref_count) / (a.ref_count + b.ref_count);
        }
        timingIn = min(a.start, b.start);
    }
    else
    {
        //Todo: this should be memory weighted
        reuse_distance = (a.reuse_distance * a.ref_count + b.reuse_distance * b.ref_count);
        reuse_distance = reuse_distance / (a.ref_count + b.ref_count);
    }
    // todo might need to be changed
    memfootPrintInKI += 8;
    wksTuple = (wsTuple){min(a.start, b.start), max(a.end, b.end), a.byte_count + b.byte_count, a.ref_count + b.ref_count, reuse_distance, timing};
    return wksTuple;
}

// void trivialMerge (wsTupleMap &processMap, wsTuple t_new)
// {
//     // 1. check overlaps 2. merge them all 3.update the map
//     bool overlaps = false;
//     vector<int64_t> toMerge;
//     vector<int64_t> notMerge;
//     wsTupleMap res;
//     for (auto t : processMap)
//     {
//         if (overlap(t.second, t_new,0))
//         {
//             toMerge.push_back(t.first);
//             overlaps = true;
//         }
//         else
//         {
//             notMerge.push_back(t.first);
//         }
//     }
//     // what if no one to merge?
//     for (auto i : toMerge)
//     {
//         t_new = tp_or(t_new,processMap[i],true,NULL);
//     }
//     // new tuple doesn't overlap with anyone, need timing incresing
//     if (overlaps == false)
//     {
//         t_new.timing = timing;
//         timingIn = t_new.start;
//         timing++;
//     }
//     res[t_new.start] = t_new;
//     for (auto i : notMerge)
//     {
//         res[i] = processMap[i];
//     }
//     processMap.clear();
//     processMap = res;
// }

// online changing the map to speed up the processing
int updateRegister = 0;
void registerUpdate(wsTuple t_new)
{

    float registerFlushT = 0.8;
    int accessTh = 1000;
    vector<uint64_t> remove;
    if (registerBuffer.find(t_new.start) == registerBuffer.end())
    {
        updateRegister++;
        registerBuffer[t_new.start] = 1;
    }
    else
    {
        updateRegister++;
        registerBuffer[t_new.start]++;
    }

    if (registerBuffer.size() > registerFlushT * memfootPrintInKI)
    {
        int avg = updateRegister / registerBuffer.size();
        for (auto it : registerBuffer)
        {
            if (it.second < avg)
            {
                remove.push_back(it.first);
            }
        }
        for (auto it : remove)
        {
            registerBuffer.erase(it);
        }
    }

    if (registerBuffer[t_new.start] > accessTh)
    {
        registerBuffer.erase(t_new.start);
        registerVariable.insert(t_new.start);
    }
}
void trivialMergeOptRegister(wsTupleMap &processMap, wsTuple t_new, set<int> &lastHitTimeSet)
{

    // locating
    if (processMap.size() == 0)
    {
        timingIn = t_new.start;
        timing++;
        t_new.timing = timing;
        lastHitTimeSet.insert(timing);
        processMap[t_new.start] = t_new;
        memfootPrintInKI += t_new.ref_count;
    }
    else if (processMap.size() == 1)
    {
        auto iter = processMap.begin();
        // the condition to decide (add and delete) or update
        if (overlap(t_new, iter->second, 0))
        {
            // if (included(t_new,iter->second))
            // {
            //     //registerUpdate(t_new);
            //     // return;
            // }
            // else
            {
                t_new = tp_or(t_new, iter->second, true, lastHitTimeSet);
                processMap[t_new.start] = t_new;
                if (t_new.start != iter->first)
                {
                    processMap.erase(iter);
                }
            }
        }
        else
        {

            timingIn = t_new.start;
            timing++;
            t_new.timing = timing;
            lastHitTimeSet.insert(timing);
            processMap[t_new.start] = t_new;
            memfootPrintInKI += t_new.ref_count;
        }
    }
    else
    {
        if (processMap.find(t_new.start) == processMap.end())
        {
            processMap[t_new.start] = t_new;
            auto iter = processMap.find(t_new.start);
            // need to delete someone

            if (processMap.find(prev(iter)->first) != processMap.end() && included(t_new, prev(iter)->second))
            {
                //registerUpdate(t_new);
                // processMap.erase(t_new.start);
                // return;
            }
            if (processMap.find(prev(iter)->first) != processMap.end() && overlap(processMap[t_new.start], prev(iter)->second, 0) &&
                processMap.find(next(iter)->first) != processMap.end() && overlap(processMap[t_new.start], next(iter)->second, 0))
            {
                processMap[prev(iter)->first] = tp_or(prev(iter)->second, processMap[t_new.start], true, lastHitTimeSet);
                processMap[prev(iter)->first] = tp_or(prev(iter)->second, next(iter)->second, false, lastHitTimeSet);
                processMap.erase(next(iter));
                processMap.erase(iter);
            }
            else if (processMap.find(prev(iter)->first) != processMap.end() && overlap(processMap[t_new.start], prev(iter)->second, 0))
            {
                processMap[prev(iter)->first] = tp_or(prev(iter)->second, processMap[t_new.start], true, lastHitTimeSet);
                processMap.erase(iter);
            }
            else if (processMap.find(next(iter)->first) != processMap.end() && overlap(processMap[t_new.start], next(iter)->second, 0))
            {
                processMap[iter->first] = tp_or(iter->second, next(iter)->second, true, lastHitTimeSet);
                processMap.erase(next(iter));
            }
            else
            {
                timingIn = t_new.start;
                timing++;
                lastHitTimeSet.insert(timing);
                processMap[t_new.start].timing = timing;
                memfootPrintInKI += t_new.ref_count;
            }
        }
        else
        {
            auto iter = processMap.find(t_new.start);
            if (included(t_new, processMap[t_new.start]))
            {
                //registerUpdate(t_new);
                // return;
            }
            processMap[t_new.start] = tp_or(t_new, processMap[t_new.start], true, lastHitTimeSet);
            if (processMap.find(next(iter)->first) != processMap.end() && overlap(processMap[t_new.start], next(iter)->second, 0))
            {
                processMap[t_new.start] = tp_or(next(iter)->second, processMap[t_new.start], false, lastHitTimeSet);
                processMap.erase(next(iter));
            }
        }
    }
}

void trivialMergeOpt(wsTupleMap &processMap, wsTuple t_new, set<int> &lastHitTimeSet)
{

    // locating
    if (processMap.size() == 0)
    {
        timingIn = t_new.start;
        timing++;
        t_new.timing = timing;
        lastHitTimeSet.insert(timing);
        processMap[t_new.start] = t_new;
    }
    else if (processMap.size() == 1)
    {
        auto iter = processMap.begin();
        // the condition to decide (add and delete) or update
        if (overlap(t_new, iter->second, 0))
        {
            t_new = tp_or(t_new, iter->second, true, lastHitTimeSet);
            processMap[t_new.start] = t_new;
            if (t_new.start != iter->first)
            {
                processMap.erase(iter);
            }
        }
        else
        {

            timingIn = t_new.start;
            timing++;
            t_new.timing = timing;
            lastHitTimeSet.insert(timing);
            processMap[t_new.start] = t_new;
        }
    }
    else
    {
        if (processMap.find(t_new.start) == processMap.end())
        {
            processMap[t_new.start] = t_new;
            auto iter = processMap.find(t_new.start);
            // need to delete someone
            if (processMap.find(prev(iter)->first) != processMap.end() && overlap(processMap[t_new.start], prev(iter)->second, 0) &&
                processMap.find(next(iter)->first) != processMap.end() && overlap(processMap[t_new.start], next(iter)->second, 0))
            {
                processMap[prev(iter)->first] = tp_or(prev(iter)->second, processMap[t_new.start], true, lastHitTimeSet);
                processMap[prev(iter)->first] = tp_or(prev(iter)->second, next(iter)->second, false, lastHitTimeSet);
                processMap.erase(next(iter));
                processMap.erase(iter);
            }
            else if (processMap.find(prev(iter)->first) != processMap.end() && overlap(processMap[t_new.start], prev(iter)->second, 0))
            {
                processMap[prev(iter)->first] = tp_or(prev(iter)->second, processMap[t_new.start], true, lastHitTimeSet);
                processMap.erase(iter);
            }
            else if (processMap.find(next(iter)->first) != processMap.end() && overlap(processMap[t_new.start], next(iter)->second, 0))
            {
                processMap[iter->first] = tp_or(iter->second, next(iter)->second, true, lastHitTimeSet);
                processMap.erase(next(iter));
            }
            else
            {
                timingIn = t_new.start;
                timing++;
                lastHitTimeSet.insert(timing);
                processMap[t_new.start].timing = timing;
            }
        }
        else
        {
            auto iter = processMap.find(t_new.start);
            processMap[t_new.start] = tp_or(t_new, processMap[t_new.start], true, lastHitTimeSet);
            if (processMap.find(next(iter)->first) != processMap.end() && overlap(processMap[t_new.start], next(iter)->second, 0))
            {
                processMap[t_new.start] = tp_or(next(iter)->second, processMap[t_new.start], false, lastHitTimeSet);
                processMap.erase(next(iter));
            }
        }
    }
}

// void trivialMergeOpt2 (wsTupleMap &processMap, wsTuple t_new)
// {

//     // locating
//     if (processMap.size() == 0)
//     {
//         timingIn = t_new.start;
//         timing++;
//         processMap[t_new.start] = t_new;
//     }
//     else if (processMap.size() == 1)
//     {
//         auto iter = processMap.begin();
//         // the condition to decide (add and delete) or update
//         if (overlap(t_new,iter->second,0))
//         {
//             t_new = tp_or (t_new, iter->second,true);
//             processMap[t_new.start] = t_new;
//             if (t_new.start != iter->first)
//             {
//                 processMap.erase(iter);
//             }
//         }
//         else
//         {
//             timingIn = t_new.start;
//             timing++;
//             processMap[t_new.start] = t_new;
//         }

//     }
//     else
//     {
//         for (auto iter = processMap.begin(); iter != processMap.end();++iter)
//         {
//             if (iter->first >= t_new.start)
//             {
//                 if (iter != processMap.begin()) // the prev or current tuple needs to be merged
//                 {
//                     auto iterPrev  = std::prev(iter);
//                     if(overlap(t_new, iterPrev->second,0)&&overlap(t_new,iter->second,0))
//                     {
//                         t_new = tp_or (t_new, iterPrev->second,true);
//                         t_new = tp_or (t_new, iter->second,true);
//                         processMap[t_new.start] = t_new;
//                         if(t_new.start != iter->first)
//                         {
//                             processMap.erase(iter);
//                         }
//                         break;

//                     }
//                     else if (overlap(t_new, iterPrev->second,0))
//                     {
//                         t_new = tp_or (t_new, iterPrev->second,true);
//                         processMap[t_new.start] = t_new;
//                         break;
//                     }
//                     else if (overlap(t_new,iter->second,0))
//                     {
//                         t_new = tp_or (t_new, iter->second,true);
//                         processMap[t_new.start] = t_new;
//                         if(t_new.start != iter->first)
//                         {
//                             processMap.erase(iter);
//                         }

//                         break;
//                     }
//                     else
//                     {
//                         timingIn = t_new.start;
//                         timing++;
//                         processMap[t_new.start] = t_new;
//                         break;
//                     }
//                 }
//                 else // add new elements
//                 {
//                     if (overlap(t_new,iter->second,0))
//                     {
//                         t_new = tp_or (t_new, iter->second,true);
//                         processMap[t_new.start] = t_new;
//                         if(t_new.start != iter->first)
//                         {
//                             processMap.erase(iter);
//                         }
//                         break;
//                     }
//                     else
//                     {
//                         timingIn = t_new.start;
//                         timing++;
//                         processMap[t_new.start] = t_new;
//                         break;
//                     }
//                 }
//             }
//             else // check next tuple
//             {
//                 continue;
//             }

//         }

//     }
// }

// void nontrivialMergeOpt (wsTupleMap &processMap)
// {

//     uint64_t start = processMap.begin()->first;
//     map<uint64_t,vector<uint64_t>> toMerge;
//     vector<uint64_t> notMerge;
//     wsTupleMap res;
//     wsTuple afterMerge;
//     for (auto iter = processMap.begin(); iter != processMap.end();++iter)
//     {
//         auto iterNext  = std::next(iter);
//         if(processMap.find(iterNext->first) != processMap.end())
//         {
//             if (overlap(iter->second, iterNext->second,20))
//             {
//                 toMerge[start].push_back(iterNext->first);
//             }
//             else
//             {
//                 if (iter->first ==start)
//                 {
//                     notMerge.push_back(iter->first);
//                 }
//                 if(std::next(iterNext) == processMap.end())
//                 {
//                     notMerge.push_back(iterNext->first);
//                 }
//                 start = iterNext->first;
//             }
//         }
//     }
//     for (auto m : toMerge)
//     {
//         afterMerge = processMap[m.first];
//         for (auto mi: m.second)
//         {
//             set<int>lastHitTimeSet;
//             afterMerge = tp_or(afterMerge,processMap[mi],false,lastHitTimeSet);
//         }
//         res[m.first] = afterMerge;
//     }
//     for (auto m : notMerge)
//     {
//         res[m] = processMap[m];
//     }
//     processMap.clear();
//     processMap = res;
// }

// todo optimizing it according to trivial merge
void nontrivialMerge(wsTupleMap &processMap)
{
    // if (NonTriErrTh > 0.2 && NonTriErrTh< 0.6)
    // {
    //     if(TupleNumChangedNonTri > NontriTh*0.5)
    //     {
    //         NonTriErrTh -= 0.05;
    //     }
    //     else
    //     {
    //         NonTriErrTh += 0.05;
    //     }
    // }

    if (processMap.size() < 2)
    {
        return;
    }
    auto iter = processMap.begin();
    auto iterNext = next(iter);

    while (iterNext != processMap.end())
    {
        if (processMap.find(iterNext->first) != processMap.end() && overlap(iter->second, iterNext->second, NonTriErrTh * (iterNext->second.end - iter->second.start)))
        {
            set<int> lastHitTimeSet;
            processMap[iter->first] = tp_or(iter->second, iterNext->second, false, lastHitTimeSet);
            auto iterDel = iterNext;
            iterNext = next(iterNext);
            processMap.erase(iterDel);
        }
        else
        {
            iter = next(iter);
            iterNext = next(iterNext);
        }
    }
}

void LoadAterStore(wsTupleMap storeMap, wsTuple t_new, wsTupleMap &loadAfterStore)
{

    bool overlaps;
    if (storeMap.find(t_new.start) != storeMap.end())
    {
        overlaps = true;
    }
    else
    {
        storeMap[t_new.start] = t_new;
        auto iter = storeMap.find(t_new.start);
        if ((storeMap.find(next(iter)->first) != storeMap.end() && overlap(storeMap[next(iter)->first], t_new, 0)) || (storeMap.find(prev(iter)->first) != storeMap.end() && overlap(storeMap[prev(iter)->first], t_new, 0)))
        {
            overlaps = true;
        }
    }

    if (overlaps)
    {
        trivialMergeOpt(loadAfterStore, t_new, loadAfterStorelastHitTimeSet);
        if (loadAfterStore.size() > 10)
        {
            nontrivialMerge(loadAfterStore);
        }
    }
}

void LivingStore(uint64_t addrIndex, int64_t t, bool fromStore)
{
    // birth from a store inst
    if (fromStore)
    {
        if (livenessUpdate.find(addrIndex) == livenessUpdate.end())
        {
            livenessTuple internalAddress;
            internalAddress = {t, -1};
            //store the address into output set temporally

            livenessUpdate[addrIndex] = livenessTupleVec.size();
            livenessTupleVec.push_back(internalAddress);
        }
        else
        {
            if (livenessTupleVec[livenessUpdate[addrIndex]].end != -1)
            {
                livenessTuple internalAddress;
                internalAddress = {t, -1};
                livenessUpdate[addrIndex] = livenessTupleVec.size();
                livenessTupleVec.push_back(internalAddress);
            }
        }
    }
}

void Process(string &key, string &value)
{
    //kernel instance detection
    //printf("key:%s, value:%s \n",key.c_str(),value.c_str());

    // timing to calculate the reuse distance in one kernel instance

    if (key == "BBEnter")
    {
        // todo saving the tuple trace per instance, and load them while processing
        // block represents current processed block id in the trace
        instCounter = 0;
        int block = stoi(value, nullptr, 0);

        // if(currentUid == 2)
        // cout << "bb id:"<< block<< endl;

        vBlock = block;
        if (BBMemInstSize.find(vBlock) != BBMemInstSize.end())
        {
            noerrorInTrace = true;
        }
        else
        {
            noerrorInTrace = false;
        }

        if (currentKernel == "-1" || kernelMap[currentKernel].find(block) == kernelMap[currentKernel].end())
        {

            string innerKernel = "-1";
            for (auto k : kernelMap)
            {
                if (k.second.find(block) != k.second.end())
                {
                    innerKernel = k.first;
                    break;
                }
            }

            // non-kerenl, only from kernel-> non-kernel will this branch be triggered
            if (currentKernel != "-1" && innerKernel == "-1")
            {
                uint64_t maxinternal = 0;
                set<int64_t> endTimeSet;
                for (auto vec : livenessTupleVec)
                {
                    if (vec.end > 0)
                    {
                        endTimeSet.insert(vec.end);
                        while (vec.start > *(endTimeSet.begin()))
                        {
                            endTimeSet.erase(endTimeSet.begin());
                        }
                        // add internal addresses here
                        if (endTimeSet.size() > maxinternal)
                        {
                            maxinternal = endTimeSet.size();
                        }
                    }
                }
                for (auto addmap : livenessUpdate)
                {
                    uint64_t address = addmap.first;
                    if (livenessTupleVec[addmap.second].end == -1)
                    {
                        wsTuple storewksTuple;
                        storewksTuple = (wsTuple){address, address + 8, 8, 1, 0, 0};
                        // trivialMergeOptRegister (storewsTupleMap[currentUid], storewksTuple,storelastHitTimeSet);
                    }
                }
                maxLivenessPerKI[currentUid] = maxinternal;

                currentUid = UID;
                kernelIdMap[UID++] = innerKernel;

                timing = 0;
                timingIn = 0;

                livenessUpdate.clear();
                livenessTupleVec.clear();
                livenessTiming = 0;
                loadlastHitTimeSet.clear();
                storelastHitTimeSet.clear();
                loadAfterStorelastHitTimeSet.clear();
                registerVariable.clear();
                registerBuffer.clear();
                memfootPrintInKI = 0;
                NumTrivialMerge = 0;
                NumNonTrivialMerge = 0;

                instNum = 0;
                // loadAftertorewsTuples.clear();
                // loadwsTuples.clear();
                // storewsTuples.clear();
            }
            currentKernel = innerKernel;
            if (innerKernel != "-1")
            {
                timing = 0;
                timingIn = 0;
                // uid represents the current kernel instance id

                uint64_t maxinternal = 0;
                set<int64_t> endTimeSet;
                for (auto vec : livenessTupleVec)
                {
                    if (vec.end > 0)
                    {
                        endTimeSet.insert(vec.end);
                        while (vec.start > *(endTimeSet.begin()))
                        {
                            endTimeSet.erase(endTimeSet.begin());
                        }
                        // add internal addresses here
                        if (endTimeSet.size() > maxinternal)
                        {
                            maxinternal = endTimeSet.size();
                        }
                    }
                }

                for (auto addmap : livenessUpdate)
                {
                    uint64_t address = addmap.first;
                    if (livenessTupleVec[addmap.second].end == -1)
                    {
                        wsTuple storewksTuple;
                        storewksTuple = (wsTuple){address, address + 8, 8, 1, 0, 0};
                        // trivialMergeOptRegister (storewsTupleMap[currentUid], storewksTuple,storelastHitTimeSet);
                    }
                }
                maxLivenessPerKI[currentUid] = maxinternal;

                currentUid = UID;
                // kernelIdMap records the map from kernel instance id to kernel id
                kernelIdMap[UID++] = currentKernel;

                //printf("maxinternal: %lu \n",maxinternal);

                livenessUpdate.clear();
                livenessTupleVec.clear();
                livenessTiming = 0;
                loadlastHitTimeSet.clear();
                storelastHitTimeSet.clear();
                loadAfterStorelastHitTimeSet.clear();
                registerVariable.clear();
                registerBuffer.clear();
                memfootPrintInKI = 0;
                NumTrivialMerge = 0;
                NumNonTrivialMerge = 0;

                instNum = 0;
            }
        }
    }
    else if (key == "StoreAddress")
    {
        // printf("key:%s, value:%ld，values:%s,  \n",key.c_str(),stoul(value, nullptr, 0),value.c_str());
        uint64_t address = stoul(value, nullptr, 0);
        // if (registerVariable.find(address)!= registerVariable.end())
        // {
        //     return;
        // }
        if (noerrorInTrace)
        {
            if (BBMemInstSize[vBlock].size() <= instCounter)
            {
                return;
            }

            // construct the tuple
            uint64_t dataSize = BBMemInstSize[vBlock][instCounter];
            //if(currentUid == 2)
            // cout << "store:"<< address << " size:"<< dataSize << endl;
            instCounter++;
            wsTuple storewksTuple;
            storewksTuple = (wsTuple){address, address + dataSize, dataSize, 1, 0, 0};

            LivingStore(address, livenessTiming, true);
            livenessTiming++;

            NumTrivialMerge++;
            trivialMergeOptRegister(storewsTupleMap[currentUid], storewksTuple, storelastHitTimeSet);

            instNum++;
            if (currentUid == koi)
            {
                int64_t memfoot = 0;
                for (auto i : storewsTupleMap[currentUid])
                {
                    memfoot += i.second.end - i.second.start;
                }
                for (auto i : loadwsTupleMap[currentUid])
                {
                    memfoot += i.second.end - i.second.start;
                }
                // printf("%d %lu %lu \n", instNum,storewsTupleMap[currentUid].size()+loadwsTupleMap[currentUid].size(), memfoot);
            }

            int beforeSize = storewsTupleMap[currentUid].size();
            if (storewsTupleMap[currentUid].size() > NontriTh && NontriOpen)
            {
                nontrivialMerge(storewsTupleMap[currentUid]);
                // TupleNumChangedNonTri = beforeSize - storewsTupleMap[currentUid].size();
                // if(NumTrivialMerge < NontriTh && NontriTh < 15)
                // {
                //     NontriTh++;
                // }
                // else if (NumTrivialMerge > NontriTh && NontriTh > 5)
                // {
                //     NontriTh--;
                // }
                // NumTrivialMerge =0;
            }

            if (storewsTupleMap[currentUid].size() > peakStoreTNum)
            {
                peakStoreTNum = storewsTupleMap[currentUid].size();
            }
        }
    }
    else if (key == "LoadAddress")
    {

        uint64_t address = stoul(value, nullptr, 0);
        // if (registerVariable.find(address)!= registerVariable.end())
        // {
        //     return;
        // }
        //Maintain a read-map that maps from the addresses that are loaded from
        if (noerrorInTrace)
        {

            uint64_t dataSize = BBMemInstSize[vBlock][instCounter];
            if (BBMemInstSize[vBlock].size() <= instCounter)
            {
                return;
            }

            //if(currentUid == 2)
            // cout << "load:"<< address << " size:"<< dataSize << endl;

            instNum++;
            if (currentUid == koi)
            {
                int64_t memfoot = 0;
                for (auto i : storewsTupleMap[currentUid])
                {
                    memfoot += i.second.end - i.second.start;
                }
                for (auto i : loadwsTupleMap[currentUid])
                {
                    memfoot += i.second.end - i.second.start;
                }
                // printf("%d %lu %lu \n", instNum,storewsTupleMap[currentUid].size()+loadwsTupleMap[currentUid].size(), memfoot);
            }

            instCounter++;
            wsTuple loadwksTuple;
            loadwksTuple = (wsTuple){address, address + dataSize, dataSize, 1, 0, 0};

            if (livenessUpdate.find(address) != livenessUpdate.end())
            {
                livenessTupleVec[livenessUpdate[address]].end = livenessTiming;
            }
            else
            {

                //    trivialMergeOptRegister(loadwsTupleMap[currentUid], loadwksTuple,loadlastHitTimeSet);
            }
            livenessTiming++;

            //instNum++;
            //NumTrivialMerge++;
            trivialMergeOptRegister(loadwsTupleMap[currentUid], loadwksTuple, loadlastHitTimeSet);
            //LoadAterStore(storewsTupleMap[currentUid], loadwksTuple,loadAfterStorewsTupleMap[currentUid]);

            if (loadwsTupleMap[currentUid].size() > NontriTh && NontriOpen)
            {
                // NumNonTrivialMerge++;
                nontrivialMerge(loadwsTupleMap[currentUid]);
                // if(NumTrivialMerge < NontriTh && NontriTh < 10)
                // {
                //     NontriTh = NontriTh+1;
                // }
                // else if (NumTrivialMerge > NontriTh && NontriTh > 5)
                // {
                //     NontriTh = NontriTh-1;
                // }
                // NumTrivialMerge =0;
            }

            if (loadwsTupleMap[currentUid].size() > peakLoadTNum)
            {
                peakLoadTNum = loadwsTupleMap[currentUid].size();
            }
        }
    }
}

// kernel instance id, bb id, set of load or store instructions
map<int, map<int, set<int>>> loadNonKernelOverlapInst;
map<int, map<int, set<int>>> loadNonKernelProjectInst;
map<int, map<int, set<int>>> storeNonKernelOverlapInst;
map<int, map<int, set<int>>> storeNonKernelProjectlapInst;
bool nonKernelFlag = false;

// process trace to generate instruction index of insterest
void ProcessDepIndex(string &key, string &value)
{
    //kernel instance detection
    //printf("key:%s, value:%s \n",key.c_str(),value.c_str());

    // if a new bb come, need to know
    // 1. whether it is in kernel/non-kernel
    // 2. if at the boundry, need to save and initialize the data structure

    if (key == "BBEnter")
    {
        // todo saving the tuple trace per instance, and load them while processing
        // block represents current processed block id in the trace
        instCounter = 0;
        int block = stoi(value, nullptr, 0);

        // vBlock is used as a global
        vBlock = block;
        // kernel/non-kernel
        if (currentKernel == "-1" || kernelMap[currentKernel].find(block) == kernelMap[currentKernel].end())
        {

            string innerKernel = "-1";
            for (auto k : kernelMap)
            {
                if (k.second.find(block) != k.second.end())
                {
                    innerKernel = k.first;
                    break;
                }
            }

            if (innerKernel == "-1")
            {
                nonKernelFlag = true;
            }
            // non-kerenl, only from kernel-> non-kernel will this branch be triggered
            if (currentKernel != "-1" && innerKernel == "-1")
            {
                currentUid = UID;
                UID++;
            }

            if (innerKernel != "-1")
            {
                nonKernelFlag = false;
                currentUid = UID;
                UID++;
            }

            currentKernel = innerKernel;
        }
    }
    else if (key == "StoreAddress" && nonKernelFlag == true)
    {

        uint64_t address = stoul(value, nullptr, 0);
        wsTuple storewksTuple;
        storewksTuple = (wsTuple){address, address + 8, 8, 1, 0, 0};
        int nextKI = currentUid + 1;

        for (auto i : loadwsTupleMap[nextKI])
        {
            if (overlap(i.second, storewksTuple, 0))
            {
                storeNonKernelOverlapInst[currentUid][vBlock].insert(instCounter);
                for (auto tpstatic : storeStaticLookupTable[vBlock][instCounter])
                {
                    loadNonKernelOverlapInst[currentUid][get<0>(tpstatic)].insert(get<1>(tpstatic));
                }
                break;
            }
        }
        instCounter++;
    }
    else if (key == "LoadAddress" && nonKernelFlag == true)
    {

        uint64_t address = stoul(value, nullptr, 0);
        wsTuple loadwksTuple;
        loadwksTuple = (wsTuple){address, address + 8, 8, 1, 0, 0};
        int beforeID = currentUid - 1;

        if (beforeID > -1)
        {
            for (auto i : storewsTupleMap[beforeID])
            {
                if (overlap(i.second, loadwksTuple, 0))
                {
                    loadNonKernelOverlapInst[currentUid][vBlock].insert(instCounter);
                    for (auto tpstatic : loadStaticLookupTable[vBlock][instCounter])
                    {
                        storeNonKernelOverlapInst[currentUid][get<0>(tpstatic)].insert(get<1>(tpstatic));
                    }
                    break;
                }
            }
        }
        instCounter++;
    }
}

void ProcessInstofInterest(string &key, string &value)
{
    //kernel instance detection
    //printf("key:%s, value:%s \n",key.c_str(),value.c_str());

    // if a new bb come, need to know
    // 1. whether it is in kernel/non-kernel
    // 2. if at the boundry, need to save and initialize the data structure

    if (key == "BBEnter")
    {
        // todo saving the tuple trace per instance, and load them while processing
        // block represents current processed block id in the trace
        instCounter = 0;
        int block = stoi(value, nullptr, 0);

        // vBlock is used as a global
        vBlock = block;
        // kernel/non-kernel
        if (currentKernel == "-1" || kernelMap[currentKernel].find(block) == kernelMap[currentKernel].end())
        {

            string innerKernel = "-1";
            for (auto k : kernelMap)
            {
                if (k.second.find(block) != k.second.end())
                {
                    innerKernel = k.first;
                    break;
                }
            }

            if (innerKernel == "-1")
            {
                nonKernelFlag = true;
            }
            // non-kerenl, only from kernel-> non-kernel will this branch be triggered
            if (currentKernel != "-1" && innerKernel == "-1")
            {
                currentUid = UID;
                UID++;
            }

            if (innerKernel != "-1")
            {
                nonKernelFlag = false;
                currentUid = UID;
                UID++;
            }

            currentKernel = innerKernel;
        }
    }
    else if (key == "StoreAddress" && nonKernelFlag == true)
    {

        uint64_t address = stoul(value, nullptr, 0);
        wsTuple storewksTuple;
        storewksTuple = (wsTuple){address, address + 8, 8, 1, 0, 0};
        int nextKI = currentUid + 1;

        for (auto i : loadwsTupleMap[nextKI])
        {
            if (overlap(i.second, storewksTuple, 0))
            {
                storeNonKernelOverlapInst[currentUid][vBlock].insert(instCounter);
                for (auto tpstatic : storeStaticLookupTable[vBlock][instCounter])
                {
                    loadNonKernelProjectInst[currentUid][get<0>(tpstatic)].insert(get<1>(tpstatic));
                }
                break;
            }
        }
        instCounter++;
    }
    else if (key == "LoadAddress" && nonKernelFlag == true)
    {
        uint64_t address = stoul(value, nullptr, 0);
        wsTuple loadwksTuple;
        loadwksTuple = (wsTuple){address, address + 8, 8, 1, 0, 0};
        int beforeID = currentUid - 1;

        if (beforeID > -1)
        {
            for (auto i : storewsTupleMap[beforeID])
            {
                if (overlap(i.second, loadwksTuple, 0))
                {
                    loadNonKernelOverlapInst[currentUid][vBlock].insert(instCounter);
                    for (auto tpstatic : loadStaticLookupTable[vBlock][instCounter])
                    {
                        storeNonKernelProjectlapInst[currentUid][get<0>(tpstatic)].insert(get<1>(tpstatic));
                    }
                    break;
                }
            }
        }
        instCounter++;
    }
}

map<int, map<int, set<int>>> storeNonKernelOverlapTupleMap;
map<int, map<int, set<int>>> storeNonKernelProjectTupleMap;
map<int, map<int, set<int>>> loadNonKernelOverlapTupleMap;
map<int, map<int, set<int>>> loadNonKernelProjectTupleMap;
//overlaptuplemap[uid][startaddr] = tuple,  projecttuplemap[uid][startaddr] = tuple 
void ProcessTupleofInterest(string &key, string &value)
{
    if (key == "BBEnter")
    {
        // todo saving the tuple trace per instance, and load them while processing
        // block represents current processed block id in the trace
        instCounter = 0;
        int block = stoi(value, nullptr, 0);

        // vBlock is used as a global
        vBlock = block;
        // kernel/non-kernel
        if (currentKernel == "-1" || kernelMap[currentKernel].find(block) == kernelMap[currentKernel].end())
        {

            string innerKernel = "-1";
            for (auto k : kernelMap)
            {
                if (k.second.find(block) != k.second.end())
                {
                    innerKernel = k.first;
                    break;
                }
            }

            if (innerKernel == "-1")
            {
                nonKernelFlag = true;
            }
            // non-kerenl, only from kernel-> non-kernel will this branch be triggered
            if (currentKernel != "-1" && innerKernel == "-1")
            {
                currentUid = UID;
                UID++;
            }

            if (innerKernel != "-1")
            {
                nonKernelFlag = false;
                currentUid = UID;
                UID++;
            }

            currentKernel = innerKernel;
        }
    }
    else if (key == "StoreAddress" && nonKernelFlag == true)
    {
        instCounter++;
    }
    else if (key == "LoadAddress" && nonKernelFlag == true)
    {
        instCounter++;
    }

}

void get_user(Instruction *CI, map<Instruction *, tuple<int64_t, int64_t>> memInstIndexMap)
{
    for (User *U : CI->users())
    {
        if (Instruction *Inst = dyn_cast<Instruction>(U))
        {
            // errs() << "F is used in instruction:\n";
            // errs() << *Inst << "\n";
            if (auto *stinst = dyn_cast<StoreInst>(Inst))
            {
                loadStaticLookupTable[get<0>(memInstIndexMap[CI])][get<1>(memInstIndexMap[CI])].insert(tuple<int64_t, int64_t>{get<0>(memInstIndexMap[Inst]), get<1>(memInstIndexMap[Inst])});
                storeStaticLookupTable[get<0>(memInstIndexMap[Inst])][get<1>(memInstIndexMap[Inst])].insert(tuple<int64_t, int64_t>{get<0>(memInstIndexMap[CI]), get<1>(memInstIndexMap[CI])});
                // errs() <<"bbid: " << get<0>(memInstIndexMap[Inst]) <<" index: " << get<1>(memInstIndexMap[Inst])<< "\n";
            }
            get_user(Inst, memInstIndexMap);
        }
    }
}

int parsingKernelInfo(string KernelFilename)
{
    ifstream inputJson(KernelFilename);
    nlohmann::json j;
    inputJson >> j;
    inputJson.close();

    for (auto &[k, l] : j["Kernels"].items())
    {
        string index = k;
        nlohmann::json kernel = l["Blocks"];
        kernelMap[index] = kernel.get<set<int>>();
    }

    // build memory instructions to data size map
    LLVMContext context;
    SMDiagnostic smerror;
    unique_ptr<Module> sourceBitcode;
    try
    {
        sourceBitcode = parseIRFile(bitcodeFile, smerror, context);
    }
    catch (exception &e)
    {
        return 0;
    }

    Module *M = sourceBitcode.get();
    Annotate(M);

    map<Instruction *, tuple<int64_t, int64_t>> memInstIndexMap;

    for (auto &mi : *M)
    {
        for (auto fi = mi.begin(); fi != mi.end(); fi++)
        {
            auto *bb = cast<BasicBlock>(fi);
            auto dl = bb->getModule()->getDataLayout();
            int64_t id = GetBlockID(bb);
            int64_t instIndex = 0;
            for (auto bi = fi->begin(); bi != fi->end(); bi++)
            {
                if (auto *inst = dyn_cast<LoadInst>(bi))
                {
                    auto *type = inst->getType();
                    uint64_t dataSize = dl.getTypeAllocSize(type);
                    BBMemInstSize[id].push_back(dataSize);
                    memInstIndexMap[inst] = tuple<int64_t, int64_t>{id, instIndex};
                    instIndex++;
                }
                else if (auto *inst = dyn_cast<StoreInst>(bi))
                {
                    auto *type = inst->getValueOperand()->getType();
                    uint64_t dataSize = dl.getTypeAllocSize(type);
                    BBMemInstSize[id].push_back(dataSize);
                    memInstIndexMap[inst] = tuple<int64_t, int64_t>{id, instIndex};
                    instIndex++;
                }
            }
        }
    }
    vector<Instruction *> storeinstVect;
    DependenceInfo *depinfo;

    for (auto &mi : *M)
    {
        for (auto fi = mi.begin(); fi != mi.end(); fi++)
        {
            auto *bb = cast<BasicBlock>(fi);
            auto dl = bb->getModule()->getDataLayout();
            int64_t id = GetBlockID(bb);
            for (auto bi = fi->begin(); bi != fi->end(); bi++)
            {
                auto *CI = dyn_cast<Instruction>(bi);
                if (auto *inst = dyn_cast<LoadInst>(bi))
                {
                    errs() << "CI:" << *CI << "\n";
                    errs() << "F is used in instruction:\n";
                    errs() << "bbid: " << get<0>(memInstIndexMap[inst]) << " index: " << get<1>(memInstIndexMap[inst]) << "\n";
                    get_user(inst, memInstIndexMap);
                }
                // else if (auto *ld = dyn_cast<LoadInst>(bi))
                // {
                //     for (auto st : storeinstVect)
                //     {
                //         errs()<<"ld:"<<*ld<<"\n";
                //         errs()<<"st::"<<*st<<"\n";

                //         // auto Dep = depinfo->depends(ins, inst, true);
                //         auto allocald = ld->getPointerOperand();
                //         // st = dyn_cast<StoreInst>(st);
                //         auto allocast = st->getOperand(1);
                //         errs()<<"allocald:"<<*allocald<<"\n";
                //         errs()<<"allocast::"<<*allocast<<"\n";
                //         // if (Dep)
                //         // {
                //         //     bool res1 = Dep->isFlow();
                //         //     bool res2 = Dep->isInput();
                //         //     bool res3 = Dep->isOutput();
                //         //     bool res4 = Dep->isAnti();
                //         //     errs()<<"1";
                //         // }
                //         // auto res = Dep->isFlow();
                //         // errs()<<"1";
                //         if (allocald == allocast)
                //         {
                //             errs()<<"g \n";
                //         }

                //     }
                // }
            }
        }
    }

    return 0;
}

void unionVector(AddrRange a, AddrRange b, vector<pair<int64_t, int64_t>> &result)
{
    if (result.size() > 0)
    {
        result.pop_back();
    }
    if (
        (get<0>(a) <= get<0>(b)) && (get<1>(a) <= get<0>(b)))
    {
        pair<int64_t, int64_t> r1(get<0>(a), get<1>(a));
        result.push_back(r1);
        pair<int64_t, int64_t> r2(get<0>(b), get<1>(b));
        result.push_back(r2);
    }
    else if ((get<0>(a) <= get<0>(b)) && (get<1>(a) >= get<0>(b)) && (get<1>(a) <= get<1>(b)))
    {
        pair<int64_t, int64_t> r1(get<0>(a), get<1>(b));
        result.push_back(r1);
    }
    else if ((get<0>(b) <= get<0>(a)) && (get<1>(b) >= get<0>(a)) && (get<1>(b) <= get<1>(a)))
    {
        pair<int64_t, int64_t> r1(get<0>(b), get<1>(a));
        result.push_back(r1);
    }
    else if ((get<0>(b) <= get<0>(a)) && (get<1>(b) <= get<0>(a)))
    {
        pair<int64_t, int64_t> r1(get<0>(b), get<1>(b));
        result.push_back(r1);
        pair<int64_t, int64_t> r2(get<0>(a), get<1>(a));
        result.push_back(r2);
    }
}

// calculate the intersection of load and store in one instance
void calMemUsePerIns(map<int, int64_t> &totalMemUsage, map<int, vector<pair<int64_t, int64_t>>> &addrTouchedPerInst)
{
    int64_t memU;
    // kernel instance
    map<int, AddrRangeMap> iterstore = storeAddrRangeMapPerInstance;

    // iteration in instances
    for (auto il : loadAddrRangeMapPerInstance)
    {
        // restult vector: addr range
        vector<pair<int64_t, int64_t>> resultVec;
        // load tuples in ki
        for (auto ill : il.second)
        {
            // store tuples in ki
            vector<int64_t> tobeErase;
            for (auto ils : iterstore[il.first])
            {
                if (resultVec.size() == 0)
                {
                    unionVector(ils.second, ill.second, resultVec);
                    for (auto ite : tobeErase)
                    {
                        iterstore[il.first].erase(ite);
                    }

                    break;
                }
                else if (get<0>(ils.second) <= get<0>(ill.second))
                {
                    tobeErase.push_back(ils.first);
                    unionVector(ils.second, AddrRange(resultVec.back().first, resultVec.back().second, 0, 0), resultVec);
                }
                else if (get<0>(ils.second) >= get<0>(ill.second))
                {
                    unionVector(ill.second, AddrRange(resultVec.back().first, resultVec.back().second, 0, 0), resultVec);
                    for (auto ite : tobeErase)
                    {
                        iterstore[il.first].erase(ite);
                    }
                    break;
                }
                else if (get<0>(ils.second) == get<0>(ill.second))
                {
                    if (get<1>(ils.second) >= get<1>(ill.second))
                    {
                        tobeErase.push_back(ils.first);
                        unionVector(ils.second, AddrRange(resultVec.back().first, resultVec.back().second, 0, 0), resultVec);
                    }
                    else
                    {
                        unionVector(ill.second, AddrRange(resultVec.back().first, resultVec.back().second, 0, 0), resultVec);
                        for (auto ite : tobeErase)
                        {
                            iterstore[il.first].erase(ite);
                        }
                        break;
                    }
                }
            }
            for (auto ite : tobeErase)
            {
                iterstore[il.first].erase(ite);
            }
        }
        addrTouchedPerInst[il.first] = resultVec;
        resultVec.clear();
    }

    for (auto il : addrTouchedPerInst)
    {
        memU = 0;
        for (auto ill : il.second)
        {
            memU += (ill.second - ill.first);
        }
        totalMemUsage[il.first] = memU;
    }
}

wsTuple intersectionTuple(wsTuple load, wsTuple store)
{
    wsTuple result;
    result.start = max(load.start, store.start);
    result.end = min(load.end, store.end);
    // todo updates these
    float ldr = float(result.end - result.start) / float(load.end - load.start);
    float str = float(result.end - result.start) / float(store.end - store.start);
    result.byte_count = load.byte_count * ldr + store.byte_count * str;
    result.ref_count = load.ref_count * ldr + store.ref_count * str;
    result.reuse_distance = load.reuse_distance * ldr + store.reuse_distance * str;
    result.timing = 0;
    return result;
}

wsTupleMap intersectionTupleMap(wsTupleMap load, wsTupleMap store)
{

    wsTupleMap intersecMap;
    for (auto l : load)
    {
        for (auto s : store)
        {
            if (overlap(l.second, s.second, 0))
            {
                wsTuple intersecTuple = intersectionTuple(l.second, s.second);
                intersecMap[intersecTuple.start] = intersecTuple;
            }
        }
    }
    return intersecMap;
}

void calDependency(map<int, map<int, wsTupleMap>> &dependency)
{
    for (auto il : loadwsTupleMap)
    {
        if (il.first <= 0)
        {
            continue;
        }
        else
        {
            //TODO: should be a kernel
            for (auto is : storewsTupleMap)
            {
                if (is.first <= 0)
                {
                    continue;
                }
                wsTupleMap intersecMap;
                if (is.first >= il.first)
                {
                    break;
                }
                else
                {
                    intersecMap = intersectionTupleMap(il.second, is.second);
                }
                dependency[il.first][is.first] = intersecMap;
            }
        }
    }
}

wsTupleMap Aggregate(wsTupleMap a, wsTupleMap b)
{
    wsTupleMap res;
    for (auto i : a)
    {
        res[i.first] = i.second;
    }
    for (auto i : b)
    {
        if (res.find(i.first) == res.end())
        {
            res[i.first] = i.second;
        }
        else
        {
            set<int> numbSet;
            res[i.first] = tp_or(i.second, a[i.first], false, numbSet);
        }
    }
    return res;
}
tuple<int, int, float, int> calTotalSize(wsTupleMap a, int liveness)
{
    int size = 0;
    int access = 1;
    float locality = 0;
    for (auto i : a)
    {
        size += i.second.end - i.second.start;
        locality = (locality * access + i.second.reuse_distance * i.second.ref_count);
        locality = locality / (access + i.second.ref_count);
        access += i.second.ref_count;
    }
    tuple<int, int, float, int> res(size, access, locality, liveness);
    return res;
}

int main(int argc, char **argv)
{

    clock_t start_time, end_time;
    start_time = clock();
    cl::ParseCommandLineOptions(argc, argv);

    NontriOpen = en;
    NontriTh = nth; // length of single pulse
    NonTriErrTh = eth;

    //read the json
    parsingKernelInfo(KernelFilename);
    application a;
    kernelIdMap[-1] = "-1";
    ProcessTrace(InputFilename, Process, "Generating DAG", noBar);
    UID = 0;
    currentUid = -1;
    ProcessTrace(InputFilename, ProcessDepIndex, "Generating DAG", noBar);

    end_time = clock();
    printf("\n time %ld \n", (end_time - start_time));
    printf("err_th %f \n n_th %d \n", NonTriErrTh, NontriTh);
    printf("peakTupleNum %d\n", peakLoadTNum + peakStoreTNum);
    // for (auto &i : storewsTupleMap)
    // {
    //     nontrivialMerge(i.second);
    // }
    // for (auto &i : loadwsTupleMap)
    // {
    //     nontrivialMerge(i.second);
    // }

    // map<int64_t, wsTupleMap> aggreated;
    // map<string, wsTupleMap> aggreatedKernel;
    // // aggregate sizes and access number
    // map<int64_t,tuple<int,int,float,int>> aggreatedSize;
    // map<string,tuple<int,int,float,int>> aggreatedSizeKernel;
    // for (auto i :loadwsTupleMap)
    // {
    //     //todo here and notrivial is too complicated wtring
    //     aggreated[i.first] = Aggregate(i.second,storewsTupleMap[i.first]);
    //     nontrivialMerge(aggreated[i.first]);
    //     aggreatedSize[i.first] = calTotalSize(aggreated[i.first],maxLivenessPerKI[i.first]);
    // }

    // //map<int, string> kernelIdMap;

    // for(auto i : aggreated)
    // {
    //     string kernelid = kernelIdMap[i.first];
    //     for (auto itp :i.second)
    //     {
    //         aggreatedKernel[kernelid] = Aggregate(i.second,aggreated[i.first]);
    //     }
    // }

    // map<int, vector<pair<int64_t, int64_t>>> addrTouchedPerInst;
    // map<int, int64_t> totalMemUsage;
    // calMemUsePerIns (totalMemUsage, addrTouchedPerInst);
    // // ki1: kernel instance, ki2: the former , map of (first addr, tuple)
    // map<int, map<int, wsTupleMap>> dependency;
    // calDependency(dependency);

    nlohmann::json jOut;
    jOut["KernelInstanceMap"] = kernelIdMap;
    // jOut["aggreatedSize"] = aggreatedSize;

    for (auto sti : storewsTupleMap)
    {
        for (auto stii : sti.second)
        {
            if (stii.second.ref_count > 1 && stii.second.byte_count > 1)
            {
                jOut["tuplePerInstance"]["store"][to_string(sti.first)][to_string(stii.first)]["1"] = stii.second.start;
                jOut["tuplePerInstance"]["store"][to_string(sti.first)][to_string(stii.first)]["2"] = stii.second.end;
                jOut["tuplePerInstance"]["store"][to_string(sti.first)][to_string(stii.first)]["3"] = stii.second.byte_count;
                jOut["tuplePerInstance"]["store"][to_string(sti.first)][to_string(stii.first)]["4"] = stii.second.ref_count;
                jOut["tuplePerInstance"]["store"][to_string(sti.first)][to_string(stii.first)]["5"] = stii.second.reuse_distance;
            }
        }
    }
    for (auto sti : loadwsTupleMap)
    {
        for (auto stii : sti.second)
        {
            if (stii.second.ref_count > 1 && stii.second.byte_count > 1)
            {
                jOut["tuplePerInstance"]["load"][to_string(sti.first)][to_string(stii.first)]["1"] = stii.second.start;
                jOut["tuplePerInstance"]["load"][to_string(sti.first)][to_string(stii.first)]["2"] = stii.second.end;
                jOut["tuplePerInstance"]["load"][to_string(sti.first)][to_string(stii.first)]["3"] = stii.second.byte_count;
                jOut["tuplePerInstance"]["load"][to_string(sti.first)][to_string(stii.first)]["4"] = stii.second.ref_count;
                jOut["tuplePerInstance"]["load"][to_string(sti.first)][to_string(stii.first)]["5"] = stii.second.reuse_distance;
            }
        }
    }

    for (auto sti : loadAfterStorewsTupleMap)
    {
        for (auto stii : sti.second)
        {
            jOut["tuplePerInstance"]["store-load"][to_string(sti.first)][to_string(stii.first)]["1"] = stii.second.start;
            jOut["tuplePerInstance"]["store-load"][to_string(sti.first)][to_string(stii.first)]["2"] = stii.second.end;
            jOut["tuplePerInstance"]["store-load"][to_string(sti.first)][to_string(stii.first)]["3"] = stii.second.byte_count;
            jOut["tuplePerInstance"]["store-load"][to_string(sti.first)][to_string(stii.first)]["4"] = stii.second.ref_count;
            jOut["tuplePerInstance"]["store-load"][to_string(sti.first)][to_string(stii.first)]["5"] = stii.second.reuse_distance;
        }
    }

    // for (auto d :dependency)
    // {
    //     for (auto di: d.second)
    //     {
    //         for (auto dii: di.second)
    //         {
    //             jOut["dependency"][to_string(d.first)][to_string(di.first)][to_string(dii.first)]["1"] = dii.second.start;
    //             jOut["dependency"][to_string(d.first)][to_string(di.first)][to_string(dii.first)]["2"] = dii.second.end;
    //             jOut["dependency"][to_string(d.first)][to_string(di.first)][to_string(dii.first)]["3"] = dii.second.byte_count;
    //             jOut["dependency"][to_string(d.first)][to_string(di.first)][to_string(dii.first)]["4"] = dii.second.ref_count;
    //             jOut["dependency"][to_string(d.first)][to_string(di.first)][to_string(dii.first)]["5"] = dii.second.reuse_distance;
    //         }
    //     }
    // }

    std::ofstream file;
    file.open(OutputFilename);
    file << std::setw(4) << jOut << std::endl;
    file.close();

    spdlog::info("Successfully detected kernel instance serial");
    return 0;
}

/// problem: 1.severl serial has same element but this is not same serial
//2.  -1 problem