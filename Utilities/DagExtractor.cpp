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


int vBlock;
int64_t instCounter = 0;

int64_t timing = 0;


void Process(string &key, string &value)
{
    //kernel instance detection
    if (key == "BBEnter")
    {
        // block represents current processed block id in the trace
        
        int block = stoi(value, nullptr, 0);
        vBlock = block;
        //printf("block:%d \n",block);
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


            if(currentKernel != "-1" &&innerKernel == "-1")
            {  
                currentUid = UID;
                kernelIdMap[UID++] = innerKernel;    
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
    }
    else if (key == "LoadAddress")
    {
        uint64_t address = stoul(value, nullptr, 0);
        //Maintain a read-map that maps from the addresses that are loaded from
        int prodUid;
        if (writeMap.find(address) != writeMap.end())
        {
            prodUid = writeMap[address];
        }
        else
        {
            prodUid = -2;
        }
        if (prodUid != -2 && prodUid != currentUid)
        {
            //former kernel instances that store to me
            myFormerOutput[currentUid].insert(prodUid);
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



    //bbid -> instruction data size
    ProcessTrace(InputFilename, Process, "Generating DAG", noBar);
    vector<vector<int>> serialAll;
    vector<int> serial;
    int i = 0;
    int maxUID = UID;

    // while (i < maxUID)
    // {
    //     bool inSerial = false;
    //     for (auto &it : serialAll)
    //     {
    //         bool finished = false;
    //         if (myFormerInput.find(i) != myFormerInput.end())
    //         {
    //             for (auto In : myFormerInput[i])
    //             {
    //                 if (it.back() == In)
    //                 {
    //                     it.push_back(i);
    //                     finished = true;
    //                     inSerial = true;
    //                     break;
    //                 }
    //             }
    //         }
    //         if (finished == false && myFormerOutput.find(i) != myFormerOutput.end())
    //         {
    //             for (auto Out : myFormerOutput[i])
    //             {
    //                 if (it.back() == Out)
    //                 {
    //                     it.push_back(i);
    //                     inSerial = true;
    //                     break;
    //                 }
    //             }
    //         }
    //     }

    //     if (inSerial == false)
    //     {
    //         serial.push_back(i);
    //         serialAll.push_back(serial);
    //         serial.clear();
    //     }
    //     i++;
    // }

    nlohmann::json jOut;
    // jOut["serialAll"] = serialAll;
    // jOut["myFormerInput"] = myFormerInput;
    // jOut["myFormerOutput"] = myFormerOutput;
    // jOut["KernelInstanceMap"] = kernelIdMap;

    std::ofstream file;
    file.open(OutputFilename);
    file << jOut;
    file.close();

    spdlog::info("Successfully detected kernel instance serial");
    return 0;
}

/// problem: 1.severl serial has same element but this is not same serial
//2.  -1 problem