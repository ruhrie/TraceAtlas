#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Traces.h"
#include "WorkingSet.h"
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <string>
#include <tuple>
#include <vector>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

using namespace std;
using namespace llvm;
using namespace WorkingSet;
map<uint64_t, set<uint64_t>> kernelMap;
map<uint64_t, uint64_t> BBCount;
set<uint64_t> BBSet;
map<uint64_t, uint64_t> SetBBCount;
map<uint64_t, CombinedKernelWorkingSet> CombinedKernelWorkingSetMap;
int option = 3;
llvm::cl::opt<string> inputTrace("i", llvm::cl::desc("Specify the input trace filename"), llvm::cl::value_desc("trace filename"));
cl::opt<std::string> KernelFilename("k", cl::desc("Specify kernel json"), cl::value_desc("kernel filename"), cl::Required);
int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    ifstream inputStream(KernelFilename);
    if (!inputStream.is_open())
    {
        cout << "open json file failed." << endl;
        return -1;
    }
    nlohmann::json j;
    inputStream >> j;
    inputStream.close();
    for (auto &[key, value] : j["Kernels"].items())
    {
        uint64_t index = stoul(key, nullptr, 0);
        nlohmann::json kernel = value["Blocks"];
        kernelMap[index] = kernel.get<set<uint64_t>>();
        set<uint64_t> temp = kernel.get<set<uint64_t>>();
        BBSet.insert(temp.begin(),temp.end());
        BBCount[index]=0;
    }
    for (auto & it:BBSet)
    {
        SetBBCount[it] = 0;
    }
    ProcessTrace(inputTrace, &WorkingSet::Process, "working set analysis", false);

    if (option == 3)
    {
        for (auto &it: kernelMap)
        {
            for (auto &itBlock: it.second)
            {
                for(auto &itAddressStruct: BlockWorkingSetMap[itBlock].internalAddressLivingVec)
                {
                    for (auto &itAddressStructCombine: CombinedKernelWorkingSetMap[it.first].internalAddressLivingVec)
                    {
                        if (itAddressStruct.address == itAddressStructCombine.address)
                        {


                        }
                        else
                        {
                            
                        }
                    }
                }                
            }
        }
    }
    //here calculates the maximum internal working set size
    if (option == 2)
    {
        for (auto &itout: KernelWorkingSetMap)
        {
            //store max size of input output internal working set
            uint64_t maxInput = 0;
            uint64_t maxOutput = 0;
            uint64_t maxinternal = 0;
            set<int64_t> endTimeSet; //using this set of end time to calculate the maximum internal working set
            for (auto it : itout.second.internalAddressLivingVec)
            {
                if (it.deathTime > 0)
                {
                    endTimeSet.insert(it.deathTime);
                    while (it.birthTime > *(endTimeSet.begin()))
                    {
                        endTimeSet.erase(endTimeSet.begin());
                    }
                    if (endTimeSet.size() > maxinternal)
                    {
                        maxinternal = endTimeSet.size();
                    }
                }
            }
            maxInput = itout.second.inputMapSize;
            maxOutput = itout.second.outputAddressIndexSet.size();
            printf("maxInput: %lu \n maxinternal: %lu \n maxOutput: %lu \n", maxInput, maxinternal, maxOutput);
        }   
    }
    
}