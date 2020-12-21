#include "AtlasUtil/Traces.h"
#include <algorithm>
#include <fstream>
#include <indicators/progress_bar.hpp>
#include <llvm/Support/CommandLine.h>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <vector>
#include <zlib.h>
#include <iostream>
#include "AtlasUtil/Annotate.h"
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <string>
#include <tuple>


using namespace llvm;
using namespace std;

// This file is used to generate the input/output relationship between kernel instances and serial of kernel instances

cl::opt<std::string> InputFilename("t", cl::desc("Specify input trace"), cl::value_desc("trace filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"), cl::Required);
cl::opt<std::string> KernelFilename("k", cl::desc("Specify kernel json"), cl::value_desc("kernel filename"), cl::Required);
llvm::cl::opt<string> bitcodeFile("b", llvm::cl::desc("Specify bitcode name"), llvm::cl::value_desc("bitcode filename"), llvm::cl::Required);
llvm::cl::opt<bool> noBar("nb", llvm::cl::desc("No progress bar"), llvm::cl::value_desc("No progress bar"));


// kernel instance id
static int UID = 0;
// kernel id
string currentKernel = "-1";
int currentUid = -1;


//maps

// read address -> kernel instance id
map<uint64_t, int> readMap;
// write address -> kernel instance id
map<uint64_t, int> writeMap;
// kernel instance id that only have internal addresses
set<int> internalSet;
// kernel instance id -> kernel id
map<int, string> kernelIdMap;
// kernel id -> basic block id
map<string, set<int>> kernelMap;
// kernel instance id -> its former input kernel instance id
map<int, set<int>> myFormerInput;
// kernel instance id -> its former output kernel instance id
map<int, set<int>> myFormerOutput;

// for data size

map<int64_t,vector<uint64_t>> BBMemInstSize;
// start end byte_count ref_count
typedef tuple <int64_t, int64_t, int64_t,int64_t> AddrRange;
typedef map<int64_t,AddrRange> AddrRangeMap;
map <int, AddrRangeMap> loadAddrRangeMapPerInstance;
map <int, AddrRangeMap> storeAddrRangeMapPerInstance;

set<int64_t> ValidBlock;
int vBlock;
int64_t instCounter = 0;
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
map<uint64_t,KernelWorkingSet> KernelWorkingSetMap;
int64_t timing = 0;

void firstStore(uint64_t addrIndex, int64_t t, bool fromStore, int kernelIndex)
{
    // birth from a store inst
    if (fromStore)
    {
        if(KernelWorkingSetMap[kernelIndex].internalAddressIndexMap.find(addrIndex)==KernelWorkingSetMap[kernelIndex].internalAddressIndexMap.end())
        {
            InternaladdressLiving internalAddress = {.addr =addrIndex, .birthTime = t, .deathTime = -1,.dep =false};
            //store the address into output set temporally
            KernelWorkingSetMap[kernelIndex].outputAddressIndexSet.insert(addrIndex);
            KernelWorkingSetMap[kernelIndex].internalAddressIndexMap[addrIndex] = KernelWorkingSetMap[kernelIndex].internalMapSize;
            KernelWorkingSetMap[kernelIndex].internalAddressLivingVec.push_back(internalAddress);
            KernelWorkingSetMap[kernelIndex].internalMapSize++;
        }
        else
        {
            InternaladdressLiving internalAddress = {.addr =addrIndex, .birthTime = t, .deathTime = -1,.dep =false};
            //store the address into output set temporally
            KernelWorkingSetMap[kernelIndex].outputAddressIndexSet.insert(addrIndex);
            KernelWorkingSetMap[kernelIndex].internalAddressLivingVec[KernelWorkingSetMap[kernelIndex].internalAddressIndexMap[addrIndex]].dep = true;
            KernelWorkingSetMap[kernelIndex].internalAddressIndexMap[addrIndex] = KernelWorkingSetMap[kernelIndex].internalMapSize;
            KernelWorkingSetMap[kernelIndex].internalAddressLivingVec.push_back(internalAddress);
            KernelWorkingSetMap[kernelIndex].internalMapSize++;
        }
        
    }
    //birth from a load inst
    else
    {
        if (KernelWorkingSetMap[kernelIndex].inputAddressIndexSet.find(addrIndex) == KernelWorkingSetMap[kernelIndex].inputAddressIndexSet.end())
        {
            KernelWorkingSetMap[kernelIndex].inputAddressIndexSet.insert(addrIndex);
        }    
    }
}

void MergeAfterProcess()
{
    int errorRate = 100;
    map<int64_t,vector<int64_t>> toBeMerged;
    
    for(auto &it :loadAddrRangeMapPerInstance)
    {
        int64_t lastStart = -1;
        int64_t lastEnd = -1;
        toBeMerged.clear();
        map<int64_t,AddrRange> itAddrRangeMap = it.second ;
        for(auto itr:itAddrRangeMap)
        {
            tuple <int64_t, int64_t, int64_t,int64_t> itAddrRange = itr.second;
            
            if(lastStart == -1)
            {
                lastStart = get<0> (itAddrRange);
                lastEnd = get<1> (itAddrRange);
            }
            else if ((get<0> (itAddrRange) - lastEnd)/(get<1> (itAddrRange) - lastStart) < errorRate)
            //else if (get<0> (itAddrRange) - lastEnd < 1)
            {
                toBeMerged[lastStart].push_back(get<0> (itAddrRange));
                lastEnd = get<1> (itAddrRange);              
            }
            else
            {
                lastStart = get<0> (itAddrRange);
                lastEnd = get<1> (itAddrRange);             
            } 
            //printf("start : %ld end : %ld, org start : %ld org end : %ld \n",lastStart,lastEnd,get<0> (itAddrRange),get<1> (itAddrRange)) ;        
        }
        map<int64_t,AddrRange> updatedMap;

        for(auto itm:toBeMerged) 
        {
            int64_t endAddr =  get<1>(itAddrRangeMap[itm.second.back()]);
            tuple<int64_t, int64_t, int64_t,int64_t> AddrRange(itm.first, endAddr, endAddr-itm.first, itm.second.size());
            updatedMap[itm.first] = AddrRange;
        }
        it.second = updatedMap;
        int size = 0;
        for (auto itv :toBeMerged)
        {
            size += itv.second.size();
        }
        //printf("merge size:%d,before size: %lu, ki:%d \n",size,itAddrRangeMap.size(),it.first);
    }
}

void MergeBeforeProcess(int uid)
{
    int errorRate = 100;
    map<int64_t,vector<int64_t>> toBeMerged;
    int64_t lastStart = -1;
    int64_t lastEnd = -1;
    map<int64_t,AddrRange> itAddrRangeMap = loadAddrRangeMapPerInstance[uid];
    for(auto itr:itAddrRangeMap)
    {
        tuple <int64_t, int64_t, int64_t,int64_t> itAddrRange = itr.second;
        
        if(lastStart == -1)
        {
            lastStart = get<0> (itAddrRange);
            lastEnd = get<1> (itAddrRange);
        }
        else if ((get<0> (itAddrRange) - lastEnd)/(get<1> (itAddrRange) - lastStart) < errorRate)
        //else if (get<0> (itAddrRange) - lastEnd < 1)
        {
            toBeMerged[lastStart].push_back(get<0> (itAddrRange));
            lastEnd = get<1> (itAddrRange);              
        }
        else
        {
            lastStart = get<0> (itAddrRange);
            lastEnd = get<1> (itAddrRange);             
        } 
        //printf("start : %ld end : %ld, org start : %ld org end : %ld \n",lastStart,lastEnd,get<0> (itAddrRange),get<1> (itAddrRange)) ;        
    }
    map<int64_t,AddrRange> updatedMap;

    for(auto itm:toBeMerged) 
    {
        int64_t endAddr =  get<1>(itAddrRangeMap[itm.second.back()]);
        tuple<int64_t, int64_t, int64_t,int64_t> AddrRange(itm.first, endAddr, endAddr-itm.first, itm.second.size());
        updatedMap[itm.first] = AddrRange;
    }
    loadAddrRangeMapPerInstance[uid] = updatedMap;
    int size = 0;
    for (auto itv :toBeMerged)
    {
        size += itv.second.size();
    }
    //printf("merge size:%d,before size: %lu, ki:%d \n",size,itAddrRangeMap.size(),it.first);   
}


void Process(string &key, string &value)
{
//kernel instance detection
    timing++;
    if (key == "BBEnter")
    {
        // block represents current processed block id in the trace
        instCounter = 0;
        int block = stoi(value, nullptr, 0);
        //printf("block:%d \n",block);
        if (currentKernel == "-1" || kernelMap[currentKernel].find(block) == kernelMap[currentKernel].end())
        {
            //printf("111 \n");
            
            vBlock = block;
            string innerKernel = "-1";
            for (auto k : kernelMap)
            {
                if (k.second.find(block) != k.second.end())
                {                   
                    innerKernel = k.first;
                    break;
                }
            }
            currentKernel = innerKernel;
            if (innerKernel != "-1")
            {
                // uid represents the current kernel instance id
                currentUid = UID;
                // kernelIdMap records the map from kernel instance id to kernel id
                kernelIdMap[UID++] = currentKernel;
            }
            else
            {
                currentUid = -1;
            }
        }
        else if (kernelMap[currentKernel].find(block) != kernelMap[currentKernel].end())
        {
            vBlock = block;
        }
    }
    else if (key == "StoreAddress")
    {
        uint64_t address = stoul(value, nullptr, 0);
        //Maintain a write-map that maps from the addresses that are stored to
        writeMap[address] = currentUid;
        int prodUid;

        if (readMap.find(address) != readMap.end())
        {
            prodUid = readMap[address];
        }
        else
        {
            prodUid = -1;
        }
        if (prodUid != -1 &&prodUid != 0 && prodUid != currentUid)
        {
            //former kernel instances that I load from 
            myFormerInput[currentUid].insert(prodUid);
        }
        else if (prodUid == currentUid)
        {
            internalSet.insert(prodUid);
        }
        if (currentUid != -1)
        {
            
            firstStore(address, timing, true, currentUid);
            
            uint64_t dataSize = BBMemInstSize[vBlock][instCounter];
            instCounter++;
            //printf("vblock:%d,ins:%lu,data size:%lu \n",vBlock,instCounter,dataSize);
            if (storeAddrRangeMapPerInstance[currentUid].find(address) == storeAddrRangeMapPerInstance[currentUid].end())
            {
                std::tuple<int64_t, int64_t, int64_t,int64_t> AddrRange(address, address+dataSize, dataSize, 1); 
                storeAddrRangeMapPerInstance[currentUid][address] = AddrRange;
            }
            else
            {
                std::tuple<int64_t, int64_t, int64_t,int64_t> ar = storeAddrRangeMapPerInstance[currentUid][address];
                int64_t stop = std::get<1> (ar);
                if (stop < address+ dataSize)
                {
                    std::tuple<int64_t, int64_t, int64_t,int64_t> addrRange(address, address+dataSize,std::get<2> (ar) + dataSize,std::get<3> (ar) + 1); 
                    storeAddrRangeMapPerInstance[currentUid][address] = addrRange;
                }
            }
            //printf("load tuple size: %lu ,store tuple size: %lu, uid %d,addr:%ld \n",loadAddrRangeMapPerInstance[currentUid].size(),storeAddrRangeMapPerInstance[currentUid].size(),currentUid,address);
        }
        
    }
    else if (key == "LoadAddress")
    {
        uint64_t address = stoul(value, nullptr, 0);
        //Maintain a read-map that maps from the addresses that are loaded from
        readMap[address] = currentUid;
        int prodUid;
        if (writeMap.find(address) != writeMap.end())
        {
            prodUid = writeMap[address];
        }
        else
        {
            prodUid = -1;
        }
        if (prodUid != -1 &&prodUid != 0 && prodUid != currentUid)
        {
            //former kernel instances that store to me
            myFormerOutput[currentUid].insert(prodUid);
        }
        else if (prodUid == currentUid)
        {
            internalSet.insert(prodUid);
        }

        if (currentUid != -1)
        {
            if (KernelWorkingSetMap[currentUid].internalAddressIndexMap.find(address) != KernelWorkingSetMap[currentUid].internalAddressIndexMap.end())
            {
                KernelWorkingSetMap[currentUid].internalAddressLivingVec[KernelWorkingSetMap[currentUid].internalAddressIndexMap[address]].deathTime = timing;
                KernelWorkingSetMap[currentUid].internalAddressIndexSet.insert(address);
                //remove the address from output set, if there is a load corresponding to a store
                if (KernelWorkingSetMap[currentUid].outputAddressIndexSet.find(address) !=KernelWorkingSetMap[currentUid].outputAddressIndexSet.end())
                {
                    KernelWorkingSetMap[currentUid].outputAddressIndexSet.erase(address);
                }         
            }
            else if (KernelWorkingSetMap[currentUid].inputAddressIndexSet.find(address) == KernelWorkingSetMap[currentUid].inputAddressIndexSet.end())
            {
                firstStore(address, timing, false, currentUid);
            }

            int mergeThreshould = 10;
            uint64_t dataSize = BBMemInstSize[vBlock][instCounter];
            instCounter++;
            if (loadAddrRangeMapPerInstance[currentUid].find(address) == loadAddrRangeMapPerInstance[currentUid].end())
            {
                std::tuple<int64_t, int64_t, int64_t,int64_t> AddrRange(address, address+dataSize, dataSize, 1); 
                loadAddrRangeMapPerInstance[currentUid][address] = AddrRange;
            }
            else
            {
                std::tuple<int64_t, int64_t, int64_t,int64_t> ar = loadAddrRangeMapPerInstance[currentUid][address];
                int64_t stop = std::get<1> (ar);
                if (stop < address+ dataSize)
                {
                    std::tuple<int64_t, int64_t, int64_t,int64_t> addrRange(address, address+dataSize,get<2> (ar) + dataSize,get<3> (ar) + 1); 
                    loadAddrRangeMapPerInstance[currentUid][address] = addrRange;
                }
            }
            if (loadAddrRangeMapPerInstance[currentUid].size()>10)
            {
                MergeBeforeProcess(currentUid);
            }
            
            //printf("load tuple size: %lu ,data size: %lu, uid %d,addr:%ld \n",loadAddrRangeMapPerInstance[currentUid].size(),dataSize,currentUid,address);
        }
        
    }    
}




int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);

    //read the json
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




    nlohmann::json blocks = j["ValidBlocks"];
    ValidBlock = blocks.get<set<int64_t>>();


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

    //bbid -> instruction data size
    

    for (auto &mi : *M)
    {
        for (auto fi = mi.begin(); fi != mi.end(); fi++)
        {
            auto *bb = cast<BasicBlock>(fi);
            auto dl = bb->getModule()->getDataLayout();
            int64_t id = GetBlockID(bb);
            if (ValidBlock.find(id) != ValidBlock.end())
            {
                for (auto bi = fi->begin(); bi != fi->end(); bi++)
                {
                    if (auto *inst = dyn_cast<LoadInst>(bi))
                    {
                        //errs()<< *inst<<"\n";
                        auto *type = inst->getPointerOperand()->getType()->getContainedType(0);
                        uint64_t dataSize = dl.getTypeAllocSize(type);
                        //errs()<< dataSize<<"\n";
                        BBMemInstSize[id].push_back(dataSize);
                        // printf("11111data szie %lu \n",dataSize);
                    }
                    else if(auto *inst = dyn_cast<StoreInst>(bi))
                    {
                        auto *type = inst->getPointerOperand()->getType()->getContainedType(0);
                        uint64_t dataSize = dl.getTypeAllocSize(type);
                        //errs()<< *inst<<"\n";
                        //BBMemInstSize[id]
                        BBMemInstSize[id].push_back(dataSize);
                        // printf("111222 data szie %lu \n",dataSize);
                    }
                }              
            }
        }
    }


    ProcessTrace(InputFilename, Process, "Generating DAG", noBar);
    vector <vector<int>> serialAll; 
    vector <int> serial;
    int i = 0;
    int maxUID = UID;






    
    while (i < maxUID)
    {
        bool inSerial = false;          
        for (auto &it : serialAll)
        {
            bool finished = false; 
            if (myFormerInput.find(i) != myFormerInput.end())
            {
                for (auto In :myFormerInput[i])
                {
                    if (it.back() == In)
                    {
                        it.push_back(i);
                        finished = true;
                        inSerial = true;
                        break;
                    }
                }
            }
            if (finished == false && myFormerOutput.find(i) != myFormerOutput.end())
            {
                for (auto Out :myFormerOutput[i])
                {
                    if (it.back() == Out)
                    {
                        it.push_back(i);
                        inSerial = true;
                        break;
                    }
                }
            }                       
        }

        if (inSerial == false)
        {
            serial.push_back(i);
            serialAll.push_back(serial);
            serial.clear();
        }
        i++;  
    }
    
    bool fast = true;
    map<pair<uint64_t,uint64_t>,vector<uint64_t>> kernelInterSection;

    if (!fast)
    {
         
        for (int it1 = 0; it1 < maxUID; it1++)
        {
            for (int it2 = it1+1 ; it2< maxUID; it2++)
            {
                //calculate intersections here
                vector<uint64_t> overlaps;
                pair<uint64_t,uint64_t> kernelpair(it1,it2);
                overlaps.resize(max(KernelWorkingSetMap[it1].inputAddressIndexSet.size(),KernelWorkingSetMap[it2].inputAddressIndexSet.size()));
                auto it = set_intersection(KernelWorkingSetMap[it1].inputAddressIndexSet.begin(),KernelWorkingSetMap[it1].inputAddressIndexSet.end(), KernelWorkingSetMap[it2].inputAddressIndexSet.begin(),KernelWorkingSetMap[it2].inputAddressIndexSet.end(),overlaps.begin());
                kernelInterSection[kernelpair].push_back(it - overlaps.begin());
                overlaps.clear();

                overlaps.resize(max(KernelWorkingSetMap[it1].inputAddressIndexSet.size(),KernelWorkingSetMap[it2].internalAddressIndexSet.size()));
                it = set_intersection(KernelWorkingSetMap[it1].inputAddressIndexSet.begin(),KernelWorkingSetMap[it1].inputAddressIndexSet.end(), KernelWorkingSetMap[it2].internalAddressIndexSet.begin(),KernelWorkingSetMap[it2].internalAddressIndexSet.end(),overlaps.begin());
                kernelInterSection[kernelpair].push_back(it - overlaps.begin());
                overlaps.clear();

                overlaps.resize(max(KernelWorkingSetMap[it1].inputAddressIndexSet.size(),KernelWorkingSetMap[it2].outputAddressIndexSet.size()));
                it = set_intersection(KernelWorkingSetMap[it1].inputAddressIndexSet.begin(),KernelWorkingSetMap[it1].inputAddressIndexSet.end(), KernelWorkingSetMap[it2].outputAddressIndexSet.begin(),KernelWorkingSetMap[it2].outputAddressIndexSet.end(),overlaps.begin());
                kernelInterSection[kernelpair].push_back(it - overlaps.begin());
                overlaps.clear();

                overlaps.resize(max(KernelWorkingSetMap[it1].internalAddressIndexSet.size(),KernelWorkingSetMap[it2].inputAddressIndexSet.size()));
                it = set_intersection(KernelWorkingSetMap[it1].internalAddressIndexSet.begin(),KernelWorkingSetMap[it1].internalAddressIndexSet.end(), KernelWorkingSetMap[it2].inputAddressIndexSet.begin(),KernelWorkingSetMap[it2].inputAddressIndexSet.end(),overlaps.begin());
                kernelInterSection[kernelpair].push_back(it - overlaps.begin());
                overlaps.clear();

                overlaps.resize(max(KernelWorkingSetMap[it1].internalAddressIndexSet.size(),KernelWorkingSetMap[it2].internalAddressIndexSet.size()));
                it = set_intersection(KernelWorkingSetMap[it1].internalAddressIndexSet.begin(),KernelWorkingSetMap[it1].internalAddressIndexSet.end(), KernelWorkingSetMap[it2].internalAddressIndexSet.begin(),KernelWorkingSetMap[it2].internalAddressIndexSet.end(),overlaps.begin());
                kernelInterSection[kernelpair].push_back(it - overlaps.begin());
                overlaps.clear();

                overlaps.resize(max(KernelWorkingSetMap[it1].internalAddressIndexSet.size(),KernelWorkingSetMap[it2].outputAddressIndexSet.size()));
                it = set_intersection(KernelWorkingSetMap[it1].internalAddressIndexSet.begin(),KernelWorkingSetMap[it1].internalAddressIndexSet.end(), KernelWorkingSetMap[it2].outputAddressIndexSet.begin(),KernelWorkingSetMap[it2].outputAddressIndexSet.end(),overlaps.begin());
                kernelInterSection[kernelpair].push_back(it - overlaps.begin());
                overlaps.clear();

                overlaps.resize(max(KernelWorkingSetMap[it1].outputAddressIndexSet.size(),KernelWorkingSetMap[it2].inputAddressIndexSet.size()));
                it = set_intersection(KernelWorkingSetMap[it1].outputAddressIndexSet.begin(),KernelWorkingSetMap[it1].outputAddressIndexSet.end(), KernelWorkingSetMap[it2].inputAddressIndexSet.begin(),KernelWorkingSetMap[it2].inputAddressIndexSet.end(),overlaps.begin());
                kernelInterSection[kernelpair].push_back(it - overlaps.begin());
                overlaps.clear();

                overlaps.resize(max(KernelWorkingSetMap[it1].outputAddressIndexSet.size(),KernelWorkingSetMap[it2].internalAddressIndexSet.size()));
                it = set_intersection(KernelWorkingSetMap[it1].outputAddressIndexSet.begin(),KernelWorkingSetMap[it1].outputAddressIndexSet.end(), KernelWorkingSetMap[it2].internalAddressIndexSet.begin(),KernelWorkingSetMap[it2].internalAddressIndexSet.end(),overlaps.begin());
                kernelInterSection[kernelpair].push_back(it - overlaps.begin());
                overlaps.clear();

                overlaps.resize(max(KernelWorkingSetMap[it1].outputAddressIndexSet.size(),KernelWorkingSetMap[it2].outputAddressIndexSet.size()));
                it = set_intersection(KernelWorkingSetMap[it1].outputAddressIndexSet.begin(),KernelWorkingSetMap[it1].outputAddressIndexSet.end(), KernelWorkingSetMap[it2].outputAddressIndexSet.begin(),KernelWorkingSetMap[it2].outputAddressIndexSet.end(),overlaps.begin());
                kernelInterSection[kernelpair].push_back(it - overlaps.begin());
                overlaps.clear();
                
            }
        }
    }
    
    
    MergeAfterProcess();

    nlohmann::json jOut;
    jOut["serialAll"] = serialAll;
    jOut["myFormerInput"] = myFormerInput;
    jOut["myFormerOutput"] = myFormerOutput;

    if(!fast)
    {
        for (const auto &key : kernelInterSection)
        {
            jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Input,Input"] = key.second[0]*4;
            jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Input,Internal"] = key.second[1]*4;
            jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Input,Output"] = key.second[2]*4;
            jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Internal,Input"] = key.second[3]*4;
            jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Internal,Internal"] = key.second[4]*4;
            jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Internal,Output"] = key.second[5]*4;
            jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Output,Input"] = key.second[6]*4;
            jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Output,Internal"] = key.second[7]*4;
            jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Output,Output"] = key.second[8]*4;
        }
    }
        
    jOut["KernelInstanceMap"] = kernelIdMap;

    std::ofstream file;
    file.open(OutputFilename);
    file << jOut;
    file.close();


    spdlog::info("Successfully detected kernel instance serial");
    return 0;
}


/// problem: 1.severl serial has same element but this is not same serial
//2.  -1 problem 