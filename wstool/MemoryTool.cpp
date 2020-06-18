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
std::set<uint64_t> kernelBlockValue;
set<uint64_t> outputAddressIndexSet;
llvm::cl::opt<string> inputTrace("i", llvm::cl::desc("Specify the input trace filename"), llvm::cl::value_desc("trace filename"));
cl::opt<std::string> KernelFilename("k", cl::desc("Specify kernel json"), cl::value_desc("kernel filename"), cl::Required);
cl::opt<int> KernelIndex("ki", cl::desc("Specify kernel index to trace"), cl::value_desc("kernel index"));
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
        string index = key;
        if (stoi(index) == KernelIndex)
        {
            nlohmann::json kernel = value["Blocks"];
            kernelBlockValue = kernel.get<set<uint64_t>>();
        }
    }
    //output address index set: to store the output address indexes
    

    ProcessTrace(inputTrace, &WorkingSet::ProcessFirst, "working set analysis first pass", false);
    for ( auto it :AddrEndtimeMap)
    {
        if (it.second ==1)
        {
            outputAddressIndexSet.insert(it.first);
        }
    }
    ProcessTrace(inputTrace, &WorkingSet::Process, "working set analysis", false);

    //store max size of input output internal working set
    uint64_t maxInput = 0;
    uint64_t maxOutput = 0;
    //set<int64_t> endTimeSet; //using this set of end time to calculate the maximum internal working set
    //printf("size %zu \n", internalAddressLivingVec.size());
    //here calculates the maximum internal working set size
    // for (auto it : internalAddressLivingVec)
    // {
    //     if (it.deathTime > 0)
    //     {
    //         endTimeSet.insert(it.deathTime);
    //         while (it.birthTime > *(endTimeSet.begin()))
    //         {
    //             endTimeSet.erase(endTimeSet.begin());
    //         }
    //         if (endTimeSet.size() > maxinternal)
    //         {
    //             maxinternal = endTimeSet.size();
    //         }
    //     }
    // }
    maxInput = inputMapSize;
    maxOutput = outputAddressIndexSet.size();
    printf("maxInput: %lu \n maxinternal: %lu \n maxOutput: %lu \n", maxInput, maxinternal, maxOutput);
}