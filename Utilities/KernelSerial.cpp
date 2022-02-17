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
#include <queue>
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
static int NodeID = 0;
// kernel id
string currentKernel = "-1";
int currentNodeID = -1;
bool noerrorInTrace;

// kernel instance id that only have internal addresses
set<int> internalSet;
// kernel instance id -> kernel id
map<int, string> kernelIdMap;
// kernel id -> basic block id
map<string, set<int>> kernelMap;

// kernel id -> control flow
map<int, set<int>> kernelControlMap;
set<int> barrierRemoval;
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

int peakStoreTNum = 0;
int peakLoadTNum = 0;
double ErrOverAll = 0.0;
int TupleNumChangedNonTri = 0;

int NontriOpen = true;
float NonTriErrTh = 0.5;
int NontriTh = 10;

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


bool overlapDependenceChecking(wsTuple a, wsTuple b, int64_t error)
{
    if (max(a.start, b.start) < (min(a.end, b.end)) + error)
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


wsTuple tp_or_tuple(wsTuple a, wsTuple b)
{
    wsTuple wksTuple;
    wksTuple = (wsTuple){min(a.start, b.start), max(a.end, b.end), a.byte_count + b.byte_count, a.ref_count + b.ref_count, 0, 0};
    return wksTuple;
}


// online changing the map to speed up the processing
int updateRegister = 0;

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

bool CheckLoadAfterStore(wsTupleMap storeMap, wsTuple t_new)
{
    bool overlaps = false;
    if (storeMap.find(t_new.start) != storeMap.end())
    {
        overlaps = true;
    }
    else
    {
        storeMap[t_new.start] = t_new;
        auto iter = storeMap.find(t_new.start);
        if ((storeMap.find(next(iter)->first) != storeMap.end() && overlap(storeMap[next(iter)->first], t_new, 0)) || (prev(iter)->first != iter->first && overlap(storeMap[prev(iter)->first], t_new, 0)))
        {
            overlaps = true;
        }
    }
    return overlaps;
}

void ControlParse(int block)
{

    if (kernelControlMap[currentNodeID].find(block) == kernelControlMap[currentNodeID].end())
    {
        timing = 0;
        timingIn = 0;
        loadlastHitTimeSet.clear();
        storelastHitTimeSet.clear();
        currentNodeID = NodeID;
        NodeID++;
    }

    // if (currentKernel == "-1" || kernelMap[currentKernel].find(block) == kernelMap[currentKernel].end())
    // {
    //     string innerKernel = "-1";
    //     for (auto k : kernelMap)
    //     {
    //         if (k.second.find(block) != k.second.end())
    //         {
    //             innerKernel = k.first;
    //             break;
    //         }
    //     }

    //     // non-kerenl, only from kernel-> non-kernel will this branch be triggered
    //     if (currentKernel != "-1" && innerKernel == "-1")
    //     {
    //         currentNodeID = NodeID;
    //         kernelIdMap[NodeID++] = innerKernel;
    //         timing = 0;
    //         timingIn = 0;
    //         loadlastHitTimeSet.clear();
    //         storelastHitTimeSet.clear();
    //     }
    //     currentKernel = innerKernel;
    //     if (innerKernel != "-1")
    //     {
    //         timing = 0;
    //         timingIn = 0;
    //         currentNodeID = NodeID;
    //         // kernelIdMap records the map from kernel instance id to kernel id
    //         kernelIdMap[NodeID++] = currentKernel;
    //         loadlastHitTimeSet.clear();
    //         storelastHitTimeSet.clear();
    //     }
    // }
}



// this is for nested basic blocks
stack<int> basicBlockBuff;
stack<int> instCounterBuff;
void Process(string &key, string &value)
{

    // timing to calculate the reuse distance in one kernel instance

    if (key == "BBEnter")
    {
        // todo saving the tuple trace per instance, and load them while processing
        // block represents current processed block id in the trace  
        int block = stoi(value, nullptr, 0);
        basicBlockBuff.push(block);
        instCounterBuff.push(instCounter);
        
        instCounter = 0;
        vBlock = block;
        if (BBMemInstSize.find(vBlock) != BBMemInstSize.end())
        {
            noerrorInTrace = true;
        }
        else
        {
            noerrorInTrace = false;
        }
        ControlParse(block);
    }
    else if(key == "BBExit")
    {
        if(basicBlockBuff.size()>1)
        {      
            basicBlockBuff.pop();
            vBlock = basicBlockBuff.top();
            instCounter = instCounterBuff.top();
            instCounterBuff.pop();
            
            
            if (BBMemInstSize.find(vBlock) != BBMemInstSize.end())
            {
                noerrorInTrace = true;
            }
            else
            {
                noerrorInTrace = false;
            }
        }
        else
        {
            basicBlockBuff.pop();
            instCounterBuff.pop();
        }
    }
    else if (key == "StoreAddress")
    {

        uint64_t address = stoul(value, nullptr, 0);

        if (noerrorInTrace)
        {
            if (BBMemInstSize[vBlock].size() <= instCounter)
            {
                return;
            }

            uint64_t dataSize = BBMemInstSize[vBlock][instCounter];
            instCounter++;
            wsTuple storewksTuple;
            storewksTuple = (wsTuple){address, address + dataSize, dataSize, 1, 0, 0};
            trivialMergeOptRegister(storewsTupleMap[currentNodeID], storewksTuple, storelastHitTimeSet);

            if (storewsTupleMap[currentNodeID].size() > peakStoreTNum)
            {
                peakStoreTNum = storewsTupleMap[currentNodeID].size();
            }
        }
    }
    else if (key == "LoadAddress")
    {

        uint64_t address = stoul(value, nullptr, 0);
        // printf("address:%lu \n",address);

        if (noerrorInTrace)
        {

            uint64_t dataSize = BBMemInstSize[vBlock][instCounter];
            if (BBMemInstSize[vBlock].size() <= instCounter)
            {
                return;
            }

            instCounter++;
            wsTuple loadwksTuple;
            loadwksTuple = (wsTuple){address, address + dataSize, dataSize, 1, 0, 0};

            // if the load address is from the kernel's store tuple, then not counting this load
            if (!CheckLoadAfterStore(storewsTupleMap[currentNodeID], loadwksTuple))
            {
                trivialMergeOptRegister(loadwsTupleMap[currentNodeID], loadwksTuple, loadlastHitTimeSet);
            }

            if (loadwsTupleMap[currentNodeID].size() > peakLoadTNum)
            {
                peakLoadTNum = loadwsTupleMap[currentNodeID].size();
            }
        }
    }
    else if (key == "MemCpy")
    {
        string srcStr,destStr,lenStr;
        // printf("value: %s \n",value.c_str());
        stringstream  streamData(value);
        getline(streamData, srcStr, ',');
        getline(streamData, destStr, ',');
        getline(streamData, lenStr, ',');
        uint64_t src = stoul(srcStr, nullptr, 0);
        uint64_t dest = stoul(destStr, nullptr, 0);
        uint64_t len = stoul(lenStr, nullptr, 0);
        // printf("src:%lu,dest:%lu,len:%lu \n",src,dest,len);

        if (noerrorInTrace)
        {

            // load tuples
            wsTuple loadwksTuple;
            loadwksTuple = (wsTuple){src, src + len, len, 1, 0, 0};

            // // if the load address is from the kernel's store tuple, then not counting this load
            if (!CheckLoadAfterStore(storewsTupleMap[currentNodeID], loadwksTuple))
            {
                trivialMergeOptRegister(loadwsTupleMap[currentNodeID], loadwksTuple, loadlastHitTimeSet);
            }


            wsTuple storewksTuple;
            storewksTuple = (wsTuple){dest, dest + len, len, 1, 0, 0};
            trivialMergeOptRegister(storewsTupleMap[currentNodeID], storewksTuple, storelastHitTimeSet);
        }
        
    }
    
}

void print_user(Instruction *CI, map<Instruction *, tuple<int64_t, int64_t>> memInstIndexMap)
{
    for (User *U : CI->users())
    {
        if (Instruction *Inst = dyn_cast<Instruction>(U))
        {
            // errs() << "F is used in instruction:\n";
            errs() << *Inst << "\n";
            if (auto *stinst = dyn_cast<StoreInst>(Inst))
            {
                errs() << "bbid: " << get<0>(memInstIndexMap[Inst]) << " index: " << get<1>(memInstIndexMap[Inst]) << "\n";
            }
            print_user(Inst, memInstIndexMap);
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

    for (auto &[k, l] : j["Control"].items())
    {
        int index = stoul(k, nullptr, 0);
        nlohmann::json kernel = l["Blocks"];
        kernelControlMap[index] = kernel.get<set<int>>();
        kernelIdMap[index] = l["Label"];
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
                // if (auto *inst = dyn_cast<LoadInst>(bi))
                // {
                //     errs() << "CI:" << *CI << "\n";
                //     errs() << "F is used in instruction:\n";
                //     errs() << "bbid: " << get<0>(memInstIndexMap[inst]) << " index: " << get<1>(memInstIndexMap[inst]) << "\n";
                //     print_user(inst, memInstIndexMap);
                // }
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

bool DepCheck(wsTuple t_new, wsTupleMap processMap)
{
    if (processMap.size() == 0)
    {
        return false;
    }
    else if (processMap.size() == 1)
    {
        auto iter = processMap.begin();
        // the condition to decide (add and delete) or update
        if (overlap(t_new, iter->second, 0))
        {
            return true;
        }
    }
    else
    {
        if (processMap.find(t_new.start) == processMap.end())
        {
            processMap[t_new.start] = t_new;
            auto iter = processMap.find(t_new.start);
            // need to delete someone

            if (processMap.find(prev(iter)->first) != processMap.end() && overlapDependenceChecking(processMap[t_new.start], prev(iter)->second, 0) &&
                processMap.find(next(iter)->first) != processMap.end() && overlapDependenceChecking(processMap[t_new.start], next(iter)->second, 0))
            {
                return true;
            }
            else if (processMap.find(prev(iter)->first) != processMap.end() && overlapDependenceChecking(processMap[t_new.start], prev(iter)->second, 0))
            {
                return true;
            }
            else if (processMap.find(next(iter)->first) != processMap.end() && overlapDependenceChecking(processMap[t_new.start], next(iter)->second, 0))
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            return true;
        }
    }
    return false;
}



void UpdateResultTupleMap(wsTuple t_new, wsTupleMap &processMap)
{
    if (processMap.size() == 0)
    {
        processMap[t_new.start] = t_new;
        return;
    }
    else if (processMap.size() == 1)
    {
        auto iter = processMap.begin();
        // the condition to decide (add and delete) or update
        if (overlap(t_new, iter->second, 0))
        {
            wsTuple res_tuple = tp_or_tuple(t_new, iter->second);
            processMap[res_tuple.start] = res_tuple;
            if (t_new.start != iter->first)
            {
                processMap.erase(iter);
            }
        }
        else
        {
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

            if (processMap.find(prev(iter)->first) != processMap.end() && overlapDependenceChecking(processMap[t_new.start], prev(iter)->second, 0) &&
                processMap.find(next(iter)->first) != processMap.end() && overlapDependenceChecking(processMap[t_new.start], next(iter)->second, 0))
            {
                processMap[prev(iter)->first] = tp_or_tuple(prev(iter)->second, processMap[t_new.start]);
                processMap[prev(iter)->first] = tp_or_tuple(prev(iter)->second, next(iter)->second);
                processMap.erase(next(iter));
                processMap.erase(iter);
            }
            else if (processMap.find(prev(iter)->first) != processMap.end() && overlapDependenceChecking(processMap[t_new.start], prev(iter)->second, 0))
            {
                processMap[prev(iter)->first] = tp_or_tuple(prev(iter)->second, processMap[t_new.start]);
                processMap.erase(iter);
            }
            else if (processMap.find(next(iter)->first) != processMap.end() && overlapDependenceChecking(processMap[t_new.start], next(iter)->second, 0))
            {
                processMap[iter->first] = tp_or_tuple(iter->second, next(iter)->second);
                processMap.erase(next(iter));
            }
        }
        else
        {
            auto iter = processMap.find(t_new.start);
            processMap[t_new.start] = tp_or_tuple(t_new, processMap[t_new.start]);
            if (processMap.find(next(iter)->first) != processMap.end() && overlap(processMap[t_new.start], next(iter)->second, 0))
            {
                processMap[t_new.start] = tp_or_tuple(next(iter)->second, processMap[t_new.start]);
                processMap.erase(next(iter));
            }
        }
    }
    return;
}

void DepCheckResTuple(wsTuple t_new, wsTupleMap processMap,wsTupleMap &resultMap)
{
    for (auto tp: processMap)
    {
        if (overlap(t_new, tp.second, 0))
        {
            wsTuple res_tuple = tp_or_tuple(t_new, tp.second);
            UpdateResultTupleMap(t_new,resultMap);
        }
    }
}

bool DepCheckMaps(int checkpoint, vector<int> nodeCheckVector)
{
    bool dep = false;
    int current = checkpoint - 1;
    while (current >= 0)
    {
        for (auto i : storewsTupleMap[nodeCheckVector[current]])
        {
            dep = DepCheck(i.second, loadwsTupleMap[nodeCheckVector[checkpoint]]);
            if (dep == true)
            {
                return true;
            }
        }
        current--;
    }
    return dep;
}

void checkNodeVecDep(vector<int> nodeCheckVector)
{
    int iter = 0;
    int prevNode;

    if (nodeCheckVector.size() < 2)
    {
        return;
    }

    // for (auto i :nodeCheckVector)
    // {
    //     if (iter == 0)
    //     {
    //         prevNode = i;
    //         iter++;
    //         continue;
    //     }
    //     if(!DepCheckMaps(storewsTupleMap[prevNode],loadwsTupleMap[i]))
    //     {
    //         // barrier removal i
    //         barrierRemoval.insert(i);
    //     }
    //     prevNode = i;
    //     iter++;
    // }

    for (int i = 1; i < nodeCheckVector.size(); i++)
    {
        if (!DepCheckMaps(i, nodeCheckVector))
        {
            barrierRemoval.insert(nodeCheckVector[i]);
        }
    }
}

void DAGGenerationCEDR()
{
    vector<int> nodeCheckVector;
    int nonKernelCounter = 0;

    for (auto i : kernelIdMap)
    {
        if (i.second == "-1")
        {
            nonKernelCounter++;
            if (nonKernelCounter == 2)
            {
                //check the kernel nodes
                checkNodeVecDep(nodeCheckVector);

                //reset the non kernel counter
                nonKernelCounter = 0;
                //reset the kernel check set
                nodeCheckVector.clear();
            }
        }
        else
        {
            // push the node into kernel check set
            nodeCheckVector.push_back(i.first);
        }
    }
}

// set<pair<int, int>> NormalDAG;
// map<int, int> indegree;
// void topo(queue<int> &output, int size)
// {
//     stack<int> tempStack;
//     queue<int> out;
//     // store the temporal result
//     queue<int> q;
//     map<int, int> indegreeInner = indegree;

//     for (int i = 0; i < size; i++)
//     {
//         if (indegree[i] == 0)
//         {
//             q.push(i);
//         }
//     }

//     int temp;
//     while (!q.empty())
//     {
//         temp = q.front();
//         q.pop();
 
//         tempStack.push(temp);
      
//         // temp degree = 0

//         // i cant represent node
//         for (int i = temp + 1; i < size; i++)
//         {
//             pair<int, int> edge = {temp, i};
//             if (NormalDAG.find(edge) != NormalDAG.end())
//             {
//                 indegreeInner[i] = 0;
//                 if (indegreeInner[i] == 0)
//                 {  
//                     q.push(i);
//                 }
//             }
//         }
//     }
//     //reverse the queue
//     while (!tempStack.empty())
//     {
//         int temp = tempStack.top();
//         tempStack.pop();
//         out.push(temp);
//     }
//     output = out;
// }

bool CheckNodeDep(int source, int target)
{
    int dep = false;
    for (auto i : storewsTupleMap[source])
    {
        dep = DepCheck(i.second, loadwsTupleMap[target]);
        if (dep == true)
        {
            return true;
        }
    }
    return dep;
}


// check if the store overlap of 1 and 2, overlaps with the load of node3
bool StoreSetOverlaping(int node1, int node2, int node3)
{
    bool dep = false;
    wsTupleMap Overlaps;
    for (auto i : storewsTupleMap[node1])
    {
        DepCheckResTuple(i.second, storewsTupleMap[node2],Overlaps);
    }

    for (auto i : Overlaps)
    {
        dep = DepCheck(i.second, loadwsTupleMap[node3]);
        if (dep == true)
        {
            return true;
        }
    }
    return dep;
}


// void DAGGenNormal()
// {
//     queue<int> DAGTopoOrder;
//     int processedSize = 0;
//     for (auto i : kernelIdMap)
//     {
//         processedSize++;
//         bool inserted = false;
//         if (DAGTopoOrder.size() == 0)
//         {
//             DAGTopoOrder.push(i.first);
//             inserted = true;
//             indegree[i.first] = 0;
//         }
//         else
//         {
//             queue<int> innerTopo = DAGTopoOrder;
//             int temp;
//             // set<int> checkedNodes;

//             // for every node in topo queue, check the dep
//             while (!innerTopo.empty())
//             {
//                 temp = innerTopo.front();
//                 innerTopo.pop();
//                 // bool repeat = false;
//                 // check the depedence

//                 // if checked node have edge with temp, then break
//                 // for (auto checked :checkedNodes)
//                 // {
//                 //     pair<int, int> edge = {temp, checked};
//                 //     if (NormalDAG.find(edge) != NormalDAG.end())
//                 //     {
                        
//                 //         repeat = true;
//                 //         break;
//                 //     }
//                 // }
//                 // if(repeat && inserted)
//                 // {
//                 //     // checkedNodes.clear();
//                 //     continue;
//                 // }
//                 // checkedNodes.insert(temp);
//                 if (CheckNodeDep(temp, i.first))
//                 {

//                     // insert to the pair result
//                     pair<int, int> edge = {temp, i.first};
//                     NormalDAG.insert(edge);

//                     // update the indegree
//                     indegree[i.first] = indegree[temp] + 1;

//                     // update the topo vector
//                     topo(DAGTopoOrder, processedSize);
//                     inserted = true;
                    
//                 }
//             }
//             // checkedNodes.clear();
//         }
//         if (!inserted)
//         {
//             indegree[i.first] = 0;
//             topo(DAGTopoOrder, processedSize);
//         }
//     }
// }


set<pair<int, int>> DAGEdge;

map <int,set<int>> DAGPrevNodeMap;

//https://stackoverflow.com/questions/8833938/is-the-stdset-iteration-order-always-ascending-according-to-the-c-specificat
set<int, greater<int>> LiveNodeSet;



// node position for networks graph 

map <int,int> NodePosition;


// find which node in the graph should the new node connect
// return node value if success else return -1 
int RecursiveCheckPrevNode(int liveNode, int newNode)
{
    int checkNode = liveNode;

    if (CheckNodeDep(liveNode,newNode))
    {
        return liveNode;
    }
    else // continue the recursion
    {
        // disable the recursion, this will introduce some problem that new node depend on nodes that appeared very early
        if (DAGPrevNodeMap[checkNode].size()>0)
        {
            for (auto prevNode : DAGPrevNodeMap[checkNode])
            {
                // problem here !!!
                int res = RecursiveCheckPrevNode(prevNode,newNode);
                if (res != -1)
                {
                    return res;
                }
            }
        }
    }
    return -1;
}

void DAGGenNormal()
{
    bool inserted = false;
    set<int, greater<int>> tempNodeSet = LiveNodeSet;
    set<int> LiveNodeCheckingSet;
    for (auto i : kernelIdMap)
    {
        // check the dep with live node, and recursive check all the prev node of live node
        for (auto node : LiveNodeSet)
        {  
            // connected to the graph
            int ConnectedNode = RecursiveCheckPrevNode(node, i.first);
            bool overlapedWithBefore = false;
            if (ConnectedNode != -1)
            {
                
                
                // check if the store set of this node overlap with the store set of prev checked live node, if so means the live node should cover this one
                // therefore not include this one
                for (auto i1: LiveNodeCheckingSet)
                {
                    if(StoreSetOverlaping(i1,ConnectedNode,i.first))
                    {
                        // not include this one
                        overlapedWithBefore = true;
                        break;
                    }
                }


                if (overlapedWithBefore)
                {
                    continue;
                }
                else
                {
                    //update the checking set
                    LiveNodeCheckingSet.insert(ConnectedNode);
                    // update the edges

                    pair<int, int> edge = {ConnectedNode, i.first};
                    DAGEdge.insert(edge);
                    if(NodePosition[i.first] < NodePosition[ConnectedNode]+1)
                    {
                        NodePosition[i.first] = NodePosition[ConnectedNode]+1;
                    }

                    // update the live node
                    inserted = true;
                    DAGPrevNodeMap[i.first].insert(node);
                    tempNodeSet.insert(i.first);

                    if (ConnectedNode == node)
                    {      
                        tempNodeSet.erase(node);
                    }              
                }           
            }         
        }

        LiveNodeSet = tempNodeSet;
        LiveNodeCheckingSet.clear();
        if(!inserted)
        {       
            LiveNodeSet.insert(i.first);
            NodePosition[i.first] = 0;          
        }
        inserted = false;
    }
}



int main(int argc, char **argv)
{

    clock_t start_time, end_time;
    start_time = clock();
    cl::ParseCommandLineOptions(argc, argv);

    NontriOpen = false;
    NontriTh = 0;
    NonTriErrTh = 0;

    //read the json
    parsingKernelInfo(KernelFilename);
    application a;
    ProcessTrace(InputFilename, Process, "Generating DAG", noBar);

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

    DAGGenerationCEDR();
    DAGGenNormal();

    for (auto sti : storewsTupleMap)
    {
        for (auto stii : sti.second)
        {
            // if (stii.second.ref_count > 1 && stii.second.byte_count > 1)
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
            // if (stii.second.ref_count > 1 && stii.second.byte_count > 1)
            {
                jOut["tuplePerInstance"]["load"][to_string(sti.first)][to_string(stii.first)]["1"] = stii.second.start;
                jOut["tuplePerInstance"]["load"][to_string(sti.first)][to_string(stii.first)]["2"] = stii.second.end;
                jOut["tuplePerInstance"]["load"][to_string(sti.first)][to_string(stii.first)]["3"] = stii.second.byte_count;
                jOut["tuplePerInstance"]["load"][to_string(sti.first)][to_string(stii.first)]["4"] = stii.second.ref_count;
                jOut["tuplePerInstance"]["load"][to_string(sti.first)][to_string(stii.first)]["5"] = stii.second.reuse_distance;
            }
        }
    }
    jOut["barrierRemoval"] = barrierRemoval;
    jOut["DAGEdge"] = DAGEdge;
    jOut["NodePosition"] = NodePosition;
  

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