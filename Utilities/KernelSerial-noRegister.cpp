#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Traces.h"
#include <algorithm>
#include <fstream>
#include <indicators/progress_bar.hpp>
#include <iostream>
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

typedef struct wsTuple
{
    uint64_t start;
    uint64_t end;
    uint64_t byte_count;
    uint64_t ref_count;
    float regular;
    uint64_t timing;
} wsTuple;
typedef map<int64_t, wsTuple> wsTupleMap;

map<int, wsTupleMap> loadwsTupleMap;
map<int, wsTupleMap> storewsTupleMap;
map<int, wsTupleMap> loadAftertorewsTupleMap;


wsTupleMap loadwsTuples;
wsTupleMap storewsTuples;
wsTupleMap loadAftertorewsTuples;

set <int> loadlastHitTimeSet;
set <int> storelastHitTimeSet;
set <int> loadAfterStorelastHitTimeSet;

typedef struct livenessTuple
{
    int64_t start;
    int64_t end;
} livenessTuple;


// addr -> related time tuples
vector<livenessTuple> livenessTupleVec;
// addr -> tuple to be update in the vector
map <uint64_t,int>livenessUpdate;
map <int, int>maxLivenessPerKI;


class kernel
{
    vector<wsTuple> Tuples;
public:
    void addTuple( wsTuple wt)
    {
        Tuples.push_back(wt);
    }     
};
class kernelInstance:kernel
{
//private:    
};
class application
{
    vector<kernelInstance> kernelInstances;
public:
    void addInstance( kernelInstance ki)
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
static int mUID = 0;
// kernel id
string currentKernel = "-1";
int currentUid = -1;
bool noerrorInTrance;

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
typedef tuple<int64_t, int64_t, int64_t,vector <uint64_t>> AddrRange;
typedef map<int64_t, AddrRange> AddrRangeMap;
map<int, AddrRangeMap> loadAddrRangeMapPerInstance;
map<int, AddrRangeMap> storeAddrRangeMapPerInstance;

uint64_t timing = 0;
uint64_t timingIn = 0;

uint64_t livenessTiming = 0;
set<int64_t> ValidBlock;
int vBlock;
int64_t instCounter = 0;




map<uint64_t,uint64_t> registerBuffer;
set<uint64_t> registerVariable;

bool overlap (wsTuple a, wsTuple b,int64_t error)
{
    return (max(a.start, b.start) <= (min(a.end, b.end) + error));
}

wsTuple tp_or (wsTuple a, wsTuple b, bool dynamic,set <int> &lastHitTimeSet)
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
            else if (b.timing ==0)
            {
                b.timing = timing;
                lastHitTimeSet.insert(timing);
                timingRemove = a.timing;
            }
            
            auto timingLarge = lastHitTimeSet.find(timing);
            auto timingSmall = lastHitTimeSet.find(timingRemove);
            
            reuse_distance = distance(timingSmall, timingLarge); 

            
                
            reuse_distance = (reuse_distance+a.regular*a.ref_count + b.regular* b.ref_count)/(a.ref_count+b.ref_count);

            lastHitTimeSet.erase(timingRemove);           
            
        }// last time hit tuple is the same with current tuple
        else
        {
            reuse_distance = (reuse_distance+a.regular*a.ref_count + b.regular* b.ref_count)/(a.ref_count+b.ref_count);
        }
        timingIn = min(a.start, b.start);
    }
    else
    {
        //Todo: this should be memory weighted 
        reuse_distance = (a.regular *a.ref_count + b.regular * b.ref_count);
        reuse_distance = reuse_distance/(a.ref_count+b.ref_count);
    }
    
    wksTuple = (wsTuple){min(a.start, b.start), max(a.end, b.end), a.byte_count+b.byte_count,a.ref_count + b.ref_count,reuse_distance,timing};
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
void trivialMergeOpt (wsTupleMap &processMap, wsTuple t_new, set <int>&lastHitTimeSet)
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
        if (overlap(t_new,iter->second,0))
        {
            t_new = tp_or (t_new, iter->second,true,lastHitTimeSet);
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
        if (processMap.find(t_new.start)==processMap.end())
        {
            processMap[t_new.start] = t_new;
            auto iter = processMap.find(t_new.start);
            // need to delete someone
            if (processMap.find(prev(iter)->first) !=processMap.end() && overlap(processMap[t_new.start],prev(iter)->second,0)&& 
            processMap.find(next(iter)->first) !=processMap.end() && overlap(processMap[t_new.start],next(iter)->second,0))
            {
                processMap[prev(iter)->first] = tp_or (prev(iter)->second, processMap[t_new.start],true,lastHitTimeSet);
                processMap[prev(iter)->first] = tp_or (prev(iter)->second, next(iter)->second,false,lastHitTimeSet);
                processMap.erase(next(iter));
                processMap.erase(iter);
            }
            else if(processMap.find(prev(iter)->first) !=processMap.end() && overlap(processMap[t_new.start],prev(iter)->second,0))
            {
                processMap[prev(iter)->first] = tp_or (prev(iter)->second, processMap[t_new.start],true,lastHitTimeSet);
                processMap.erase(iter);

            }
            else if(processMap.find(next(iter)->first) !=processMap.end() && overlap(processMap[t_new.start],next(iter)->second,0))
            {
                processMap[iter->first] = tp_or (iter->second, next(iter)->second,true,lastHitTimeSet);
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
        {   auto iter = processMap.find(t_new.start);
            processMap[t_new.start] = tp_or (t_new, processMap[t_new.start],true,lastHitTimeSet);
            if (processMap.find(next(iter)->first) !=processMap.end() && overlap(processMap[t_new.start],next(iter)->second,0))
            {
                processMap[t_new.start] = tp_or (next(iter)->second, processMap[t_new.start],false,lastHitTimeSet);
                processMap.erase(next(iter));
            }
        }
    }   
}



void trivialMergeOptRegister (wsTupleMap &processMap, wsTuple t_new, set <int>&lastHitTimeSet)
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
        if (overlap(t_new,iter->second,0))
        {
            t_new = tp_or (t_new, iter->second,true,lastHitTimeSet);
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
        if (processMap.find(t_new.start)==processMap.end())
        {
            processMap[t_new.start] = t_new;
            auto iter = processMap.find(t_new.start);
            // need to delete someone
            if (processMap.find(prev(iter)->first) !=processMap.end() && overlap(processMap[t_new.start],prev(iter)->second,0)&& 
            processMap.find(next(iter)->first) !=processMap.end() && overlap(processMap[t_new.start],next(iter)->second,0))
            {
                processMap[prev(iter)->first] = tp_or (prev(iter)->second, processMap[t_new.start],true,lastHitTimeSet);
                processMap[prev(iter)->first] = tp_or (prev(iter)->second, next(iter)->second,false,lastHitTimeSet);
                processMap.erase(next(iter));
                processMap.erase(iter);
            }
            else if(processMap.find(prev(iter)->first) !=processMap.end() && overlap(processMap[t_new.start],prev(iter)->second,0))
            {
                processMap[prev(iter)->first] = tp_or (prev(iter)->second, processMap[t_new.start],true,lastHitTimeSet);
                processMap.erase(iter);

            }
            else if(processMap.find(next(iter)->first) !=processMap.end() && overlap(processMap[t_new.start],next(iter)->second,0))
            {
                processMap[iter->first] = tp_or (iter->second, next(iter)->second,true,lastHitTimeSet);
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
        {   auto iter = processMap.find(t_new.start);
            processMap[t_new.start] = tp_or (t_new, processMap[t_new.start],true,lastHitTimeSet);
            if (processMap.find(next(iter)->first) !=processMap.end() && overlap(processMap[t_new.start],next(iter)->second,0))
            {
                processMap[t_new.start] = tp_or (next(iter)->second, processMap[t_new.start],false,lastHitTimeSet);
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
void nontrivialMerge (wsTupleMap &processMap)
{

    auto iter = processMap.begin();
    auto iterNext = next(iter);

    while(iterNext != processMap.end())
    {
        if (processMap.find(iterNext->first) !=processMap.end() && overlap(iter->second, iterNext->second,20))
        {
            set<int>lastHitTimeSet;
            processMap[iter->first] = tp_or(iter->second,iterNext->second,false,lastHitTimeSet);
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


void LoadAterStore (wsTupleMap storeMap, wsTuple t_new, wsTupleMap &loadAfterStore)
{

    bool overlaps;
    if (storeMap.find(t_new.start)!=storeMap.end())
    {
        overlaps = true;
    }
    else
    {
        storeMap[t_new.start] = t_new; 
        auto iter = storeMap.find(t_new.start);
        if( (storeMap.find(next(iter)->first)!=storeMap.end() && overlap(storeMap[next(iter)->first],t_new,0))
          ||(storeMap.find(prev(iter)->first)!=storeMap.end() && overlap(storeMap[prev(iter)->first],t_new,0)))
        {
            overlaps = true;
        }
    }
    // for (auto it : storeMap)
    // {
    //     if (overlap(it.second,t_new,0))
    //     {
    //         overlaps = true;
    //         break;
    //     }
    // }
    if (overlaps)
    {
        trivialMergeOpt (loadAfterStore, t_new,loadAfterStorelastHitTimeSet);
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
            if(livenessUpdate.find(addrIndex)==livenessUpdate.end())
            {
                livenessTuple internalAddress;
                internalAddress = {t,  -1};
                //store the address into output set temporally
              
                livenessUpdate[addrIndex] = livenessTupleVec.size();
                livenessTupleVec.push_back(internalAddress);

            }
            else
            {
                if(livenessTupleVec[livenessUpdate[addrIndex]].end != -1)
                {
                    livenessTuple internalAddress;
                    internalAddress = {t,  -1};
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
        vBlock = block;
        if (BBMemInstSize.find(vBlock) != BBMemInstSize.end())
        {
            noerrorInTrance = true;
        }
        else
        {
            noerrorInTrance = false;
        }
        
        if (currentKernel == "-1" || kernelMap[currentKernel].find(block) == kernelMap[currentKernel].end())
        {
            
            //vBlock = block;
            string innerKernel = "-1";
            for (auto k : kernelMap)
            {
                if (k.second.find(block) != k.second.end())
                {
                    innerKernel = k.first;
                    break;
                }
            }
            if(currentKernel != "-1" &&innerKernel == "-1")
            {
                timing = 0;
                timingIn = 0;


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
                maxLivenessPerKI[currentUid] = maxinternal;

                
                currentUid = -1-mUID;
                mUID++;


                

               
                
                livenessUpdate.clear();
                livenessTupleVec.clear();
                livenessTiming = 0;
                loadlastHitTimeSet.clear();
                storelastHitTimeSet.clear();
                loadAfterStorelastHitTimeSet.clear();
                
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
                
            }
        }
    }
    else if (key == "StoreAddress")
    {
        //printf("key:%s, value:%ld \n",key.c_str(),stoul(value, nullptr, 0));
        uint64_t address = stoul(value, nullptr, 0);      
        if (noerrorInTrance)
        {

            // construct the tuple
            uint64_t dataSize = BBMemInstSize[vBlock][instCounter];
            instCounter++;
            wsTuple storewksTuple;           
            storewksTuple = (wsTuple){address, address + dataSize, dataSize,1,0,0};
            
                   
            LivingStore(address, livenessTiming, true);
            
            trivialMergeOpt (storewsTupleMap[currentUid], storewksTuple,storelastHitTimeSet);
            if (storewsTupleMap[currentUid].size() > 10)
            {
                nontrivialMerge(storewsTupleMap[currentUid]);
            }

            livenessTiming++;
        }
    }
    else if (key == "LoadAddress")
    {
        //printf("key:%s, value:%ld \n",key.c_str(),stoul(value, nullptr, 0));
        uint64_t address = stoul(value, nullptr, 0);
        //Maintain a read-map that maps from the addresses that are loaded from
        if (noerrorInTrance)
        {
            uint64_t dataSize = BBMemInstSize[vBlock][instCounter];
            instCounter++;
            wsTuple loadwksTuple;
            loadwksTuple = (wsTuple){address, address + dataSize, dataSize,1,0,0};

            if(livenessUpdate.find(address) !=livenessUpdate.end())
            {
                livenessTupleVec[livenessUpdate[address]].end = livenessTiming;
            }

            trivialMergeOpt(loadwsTupleMap[currentUid], loadwksTuple,loadlastHitTimeSet);            
            //LoadAterStore(storewsTupleMap[currentUid], loadwksTuple,loadAftertorewsTupleMap[currentUid]);
            if (loadwsTupleMap[currentUid].size() > 10)
            {
                nontrivialMerge(loadwsTupleMap[currentUid]);
            }
            livenessTiming++;          
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


    for (auto &mi : *M)
    {
        for (auto fi = mi.begin(); fi != mi.end(); fi++)
        {
            auto *bb = cast<BasicBlock>(fi);
            auto dl = bb->getModule()->getDataLayout();
            int64_t id = GetBlockID(bb);

            for (auto bi = fi->begin(); bi != fi->end(); bi++)
            {
                if (auto *inst = dyn_cast<LoadInst>(bi))
                {
                    auto *type = inst->getType();
                    uint64_t dataSize = dl.getTypeAllocSize(type);
                    BBMemInstSize[id].push_back(dataSize);
                }
                else if (auto *inst = dyn_cast<StoreInst>(bi))
                {
                    auto *type = inst->getValueOperand()->getType();
                    uint64_t dataSize = dl.getTypeAllocSize(type);
                    BBMemInstSize[id].push_back(dataSize);

                }
            }
            
        }
    }
    return 0;
}

void unionVector (AddrRange a, AddrRange b,vector<pair<int64_t, int64_t>> &result)
{
    if (result.size() >0)
    {
        result.pop_back();
    }
    if(
        (get<0>(a)<= get<0>(b)) && (get<1>(a)<= get<0>(b))      
     )
    {
        pair<int64_t, int64_t> r1(get<0>(a),get<1>(a));
        result.push_back(r1);
        pair<int64_t, int64_t> r2(get<0>(b),get<1>(b));
        result.push_back(r2);
    }
    else if ((get<0>(a)<= get<0>(b)) && (get<1>(a)>= get<0>(b))&& (get<1>(a) <= get<1>(b)))
    {
        pair<int64_t, int64_t> r1(get<0>(a),get<1>(b));
        result.push_back(r1);
    }
    else if ((get<0>(b)<= get<0>(a)) && (get<1>(b)>= get<0>(a))&& (get<1>(b) <= get<1>(a)))
    {
        pair<int64_t, int64_t> r1(get<0>(b),get<1>(a));
        result.push_back(r1);
    }
    else if ((get<0>(b)<= get<0>(a)) && (get<1>(b)<= get<0>(a)))
    {
        pair<int64_t, int64_t> r1(get<0>(b),get<1>(b));
        result.push_back(r1);
        pair<int64_t, int64_t> r2(get<0>(a),get<1>(a));
        result.push_back(r2);
    }
}


// calculate the intersection of load and store in one instance
void calMemUsePerIns (map<int, int64_t> &totalMemUsage, map<int, vector<pair<int64_t, int64_t>>> &addrTouchedPerInst)
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
                if (resultVec.size()==0)
                {
                    unionVector (ils.second,ill.second,resultVec);
                    for (auto ite: tobeErase)
                    {
                        iterstore[il.first].erase(ite);
                    }
                    
                    break;
                }
                else if( get<0>(ils.second) <= get<0>(ill.second))
                {
                    tobeErase.push_back(ils.first);
                    unionVector (ils.second,AddrRange(resultVec.back().first,resultVec.back().second,0,0),resultVec);
                }
                else if( get<0>(ils.second) >= get<0>(ill.second))
                {
                    unionVector (ill.second,AddrRange(resultVec.back().first,resultVec.back().second,0,0),resultVec);
                    for (auto ite: tobeErase)
                    {
                        iterstore[il.first].erase(ite);
                    }
                    break;
                }
                else if ( get<0>(ils.second) == get<0>(ill.second))
                {
                    if( get<1>(ils.second) >= get<1>(ill.second))
                    {
                        tobeErase.push_back(ils.first);
                        unionVector (ils.second,AddrRange(resultVec.back().first,resultVec.back().second,0,0),resultVec);
                    }
                    else
                    {
                        unionVector (ill.second,AddrRange(resultVec.back().first,resultVec.back().second,0,0),resultVec);
                        for (auto ite: tobeErase)
                        {
                            iterstore[il.first].erase(ite);
                        }
                        break;
                    }
                }
            }
            for (auto ite: tobeErase)
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

wsTuple intersectionTuple(wsTuple load,wsTuple store)
{
    wsTuple result;
    result.start = max(load.start, store.start);
    result.end = min(load.end, store.end);
    // todo updates these
    float ldr = float(result.end -result.start)/ float(load.end- load.start);
    float str = float(result.end -result.start)/ float(store.end- store.start);
    result.byte_count  = load.byte_count* ldr + store.byte_count* str;
    result.ref_count = load.ref_count* ldr + store.ref_count* str;
    result.regular = load.regular* ldr + store.regular* str;
    result.timing = 0;     
    return result;
}


wsTupleMap intersectionTupleMap(wsTupleMap load, wsTupleMap store)
{

    wsTupleMap intersecMap;
    for (auto l :load)
    {
        for(auto s :store)
        {
            if (overlap(l.second,s.second,0))
            {
            wsTuple intersecTuple = intersectionTuple(l.second,s.second);
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
        if ( il.first <= 0)
        {
            continue;
        }
        else
        {
            //TODO: should be a kernel
            for (auto is : storewsTupleMap)
            {
                if ( is.first <= 0)
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
                    intersecMap = intersectionTupleMap(il.second,is.second);
                }
                dependency[il.first][is.first] = intersecMap;
            }
        }
        
    }
}

wsTupleMap Aggregate(wsTupleMap a, wsTupleMap b)
{
    wsTupleMap res;
    for (auto i :a)
    {
        res[i.first] = i.second;
    }
    for (auto i :b)
    {
        if (res.find(i.first)==res.end())
        {
            res[i.first] = i.second;
        }
        else
        {
            set<int> numbSet;
            res[i.first] = tp_or(i.second,a[i.first],false,numbSet);
        }
    }
    return res;  
}
tuple<int,int,float,int> calTotalSize(wsTupleMap a,int liveness)
{
    int size = 0;
    int access = 1;
    float locality = 0;
    for (auto i : a)
    {
        size += i.second.end - i.second.start;                
        locality = (locality *access + i.second.regular *i.second.ref_count);
        locality = locality/(access+i.second.ref_count);
        access += i.second.ref_count;       
    }
    tuple<int,int,float,int> res(size,access,locality,liveness); 
    return res;
}

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);

    //read the json
    parsingKernelInfo(KernelFilename);
    application a;
    ProcessTrace(InputFilename, Process, "Generating DAG");

    map<int64_t, wsTupleMap> aggreated;
    map<string, wsTupleMap> aggreatedKernel;
    // aggregate sizes and access number
    map<int64_t,tuple<int,int,float,int>> aggreatedSize;
    map<string,tuple<int,int,float,int>> aggreatedSizeKernel;
    for (auto i :loadwsTupleMap)
    {
        //todo here and notrivial is too complicated wtring
        aggreated[i.first] = Aggregate(i.second,storewsTupleMap[i.first]);
        nontrivialMerge(aggreated[i.first]);
        aggreatedSize[i.first] = calTotalSize(aggreated[i.first],maxLivenessPerKI[i.first]);
    }

    //map<int, string> kernelIdMap;

    for(auto i : aggreated)
    {
        string kernelid = kernelIdMap[i.first];
        for (auto itp :i.second)
        {
            aggreatedKernel[kernelid] = Aggregate(i.second,aggreated[i.first]);
        }
    }

    
    // map<int, vector<pair<int64_t, int64_t>>> addrTouchedPerInst;
    // map<int, int64_t> totalMemUsage;    
    // calMemUsePerIns (totalMemUsage, addrTouchedPerInst);
    // // ki1: kernel instance, ki2: the former , map of (first addr, tuple)
    map<int, map<int, wsTupleMap>> dependency;
    calDependency(dependency);

    nlohmann::json jOut;
    jOut["KernelInstanceMap"] = kernelIdMap;
    jOut["aggreatedSize"] = aggreatedSize;
  
    for (auto sti :storewsTupleMap)
    {
        for (auto stii: sti.second)
        {
            jOut["tuplePerInstance"]["store"][to_string(sti.first)][to_string(stii.first)]["1"] = stii.second.start;
            jOut["tuplePerInstance"]["store"][to_string(sti.first)][to_string(stii.first)]["2"] = stii.second.end;
            jOut["tuplePerInstance"]["store"][to_string(sti.first)][to_string(stii.first)]["3"] = stii.second.byte_count;
            jOut["tuplePerInstance"]["store"][to_string(sti.first)][to_string(stii.first)]["4"] = stii.second.ref_count;
            jOut["tuplePerInstance"]["store"][to_string(sti.first)][to_string(stii.first)]["5"] = stii.second.regular;
        }
    }
    for (auto sti :loadwsTupleMap)
    {
        for (auto stii: sti.second)
        {
            jOut["tuplePerInstance"]["load"][to_string(sti.first)][to_string(stii.first)]["1"] = stii.second.start;
            jOut["tuplePerInstance"]["load"][to_string(sti.first)][to_string(stii.first)]["2"] = stii.second.end;
            jOut["tuplePerInstance"]["load"][to_string(sti.first)][to_string(stii.first)]["3"] = stii.second.byte_count;
            jOut["tuplePerInstance"]["load"][to_string(sti.first)][to_string(stii.first)]["4"] = stii.second.ref_count;
            jOut["tuplePerInstance"]["load"][to_string(sti.first)][to_string(stii.first)]["5"] = stii.second.regular;
        }
    }

    for (auto sti :loadAftertorewsTupleMap)
    {
        for (auto stii: sti.second)
        {
            jOut["tuplePerInstance"]["store-load"][to_string(sti.first)][to_string(stii.first)]["1"] = stii.second.start;
            jOut["tuplePerInstance"]["store-load"][to_string(sti.first)][to_string(stii.first)]["2"] = stii.second.end;
            jOut["tuplePerInstance"]["store-load"][to_string(sti.first)][to_string(stii.first)]["3"] = stii.second.byte_count;
            jOut["tuplePerInstance"]["store-load"][to_string(sti.first)][to_string(stii.first)]["4"] = stii.second.ref_count;
            jOut["tuplePerInstance"]["store-load"][to_string(sti.first)][to_string(stii.first)]["5"] = stii.second.regular;
        }
    } 
    
    for (auto d :dependency)
    {
        for (auto di: d.second)
        {
            for (auto dii: di.second)            
            {
                jOut["dependency"][to_string(d.first)][to_string(di.first)][to_string(dii.first)]["1"] = dii.second.start;
                jOut["dependency"][to_string(d.first)][to_string(di.first)][to_string(dii.first)]["2"] = dii.second.end;
                jOut["dependency"][to_string(d.first)][to_string(di.first)][to_string(dii.first)]["3"] = dii.second.byte_count;
                jOut["dependency"][to_string(d.first)][to_string(di.first)][to_string(dii.first)]["4"] = dii.second.ref_count;
                jOut["dependency"][to_string(d.first)][to_string(di.first)][to_string(dii.first)]["5"] = dii.second.regular;
            }          
        }
    }


    std::ofstream file;
    file.open(OutputFilename);
    file << std::setw(4) << jOut << std::endl;
    file.close();

    spdlog::info("Successfully detected kernel instance serial");
    return 0;
}

/// problem: 1.severl serial has same element but this is not same serial
//2.  -1 problem