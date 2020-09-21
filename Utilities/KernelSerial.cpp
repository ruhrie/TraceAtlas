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
using namespace llvm;
using namespace std;

// This file is used to generate the input/output relationship between kernel instances and serial of kernel instances

cl::opt<std::string> InputFilename("t", cl::desc("Specify input trace"), cl::value_desc("trace filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"), cl::Required);
cl::opt<std::string> KernelFilename("k", cl::desc("Specify kernel json"), cl::value_desc("kernel filename"), cl::Required);
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

void Process(string &key, string &value)
{
//kernel instance detection
    timing++;
    if (key == "BBEnter")
    {
        // block represents current processed block id in the trace
        int block = stoi(value, nullptr, 0);
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
            currentKernel = innerKernel;
            if (innerKernel != "-1")
            {
                // uid represents the current kernel instance id
                currentUid = UID;
                // kernelIdMap records the map from kernel instance id to kernel id
                kernelIdMap[UID++] = currentKernel;
            }
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
        if (prodUid != -1 && prodUid != currentUid)
        {
            //former kernel instances that I load from 
            myFormerInput[currentUid].insert(prodUid);
        }
        else if (prodUid == currentUid)
        {
            internalSet.insert(prodUid);
        }
        firstStore(address, timing, true, currentUid);
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
        if (prodUid != -1 && prodUid != currentUid)
        {
            //former kernel instances that store to me
            myFormerOutput[currentUid].insert(prodUid);
        }
        else if (prodUid == currentUid)
        {
            internalSet.insert(prodUid);
        }

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
    

    map<pair<uint64_t,uint64_t>,vector<uint64_t>> kernelInterSection; 
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



    nlohmann::json jOut;
    jOut["serialAll"] = serialAll;
    jOut["myFormerInput"] = myFormerInput;
    jOut["myFormerOutput"] = myFormerOutput;

    for (const auto &key : kernelInterSection)
    {
        jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Input,Input"] = key.second[0];
        jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Input,Internal"] = key.second[1];
        jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Input,Output"] = key.second[2];
        jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Internal,Input"] = key.second[3];
        jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Internal,Internal"] = key.second[4];
        jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Internal,Output"] = key.second[5];
        jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Output,Input"] = key.second[6];
        jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Output,Internal"] = key.second[7];
        jOut["overlaps"][to_string(key.first.first)+","+to_string(key.first.second)]["Output,Output"] = key.second[8];
    }


    std::ofstream file;
    file.open(OutputFilename);
    file << jOut;
    file.close();

    spdlog::info("Successfully detected kernel instance serial");
    return 0;
}


/// problem: 1.severl serial has same element but this is not same serial
//2.  -1 problem 