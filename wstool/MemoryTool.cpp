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

using namespace std;
using namespace llvm;
using namespace WorkingSet;
llvm::cl::opt<string> inputTrace("i", llvm::cl::desc("Specify the input trace filename"), llvm::cl::value_desc("trace filename"));

int main(int argc, char **argv)
{

    cl::ParseCommandLineOptions(argc, argv);
    ProcessTrace(inputTrace, &WorkingSet::Process, "working set analysis", false);

    //store max size of input output internal working set
    uint64_t maxInput = 0;
    uint64_t maxOutput = 0;
    uint64_t maxinternal = 0;
    set<uint64_t> outputSet; //using this set of output addresses in case of duplicated
    set<int64_t> endTimeSet; //using this set of end time to calculate the maximum internal working set
    // struct of internal working set
    for (auto it : internalAddressLivingVec)
    {
        //if the deathTime is "-1", it's an output address
        if (it.deathTime == -1 && outputSet.find(it.address) == outputSet.end())
        {
            outputSet.insert(it.address);
        }
        endTimeSet.insert(it.deathTime);
        if (it.brithTime > *(endTimeSet.begin()))
        {
            endTimeSet.erase(endTimeSet.begin());
        }
        if (endTimeSet.size() > maxinternal)
        {
            maxinternal = endTimeSet.size();
        }
    }
    maxInput = inputMapSize;
    maxOutput = outputSet.size();
    printf("maxInput: %lu \n maxinternal: %lu \n maxOutput: %lu \n", maxInput, maxinternal, maxOutput);
}