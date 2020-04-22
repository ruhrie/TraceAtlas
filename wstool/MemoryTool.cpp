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
    //using this set of output addresses in case of duplicated
    set <int64_t> outputSet;
    // struct of internal working set 
    for (auto it : internalAddressLivingVec)
    {
        //if the deathTime is 3, it's an output address
        if (it.deathTime == -1 && outputSet.find(it.address) == outputSet.end())
        {
            outputSet.insert(it.address);
            maxOutput++;
        }
    }
    //time iteration
    for (int64_t time = 0; time < timing; time++)
    { 
        set<int64_t> aliveInternalAddress;
        // for erasing
        set<int> eraseSet;
        int eraseIndex = 0;
        //internal virtual address iteration for each time
        for (auto it : internalAddressLivingVec)
        {
            //the birth time of addresses is late than time now, break the loop
            if (it.brithTime > time)
            {
                break;
            }
            //  brithTime < t < deathTime, it is a living internal address
            if (it.brithTime <= time && it.deathTime > time)
            {
                aliveInternalAddress.insert(it.address);
            }
            //  t > deathTime, it is a died internal address, erasing this from the vector to speed up
            else if (time > it.deathTime)
            {
                eraseSet.insert(eraseIndex);
            }
            eraseIndex++;
        }
        //erase died addresses from the vector
        for (auto it : eraseSet)
        {
            internalAddressLivingVec.erase(internalAddressLivingVec.begin() + it);
        }
        //max internal size
        if (maxinternal < aliveInternalAddress.size())
        {
            maxinternal = aliveInternalAddress.size();
        }
        // for debug
        if (time % 1000 == 0)
        {
            float ProcessRatio = (float)time / WorkingSet::timing;
            // for temporal debug
            printf("process time :%ld, %ld, %.5f \n", time, WorkingSet::timing, ProcessRatio);
        }
//        timeline.clear();
    }
    maxInput = inputMapSize;
    printf("maxInput: %lu \n maxinternal: %lu \n maxOutput: %lu \n", maxInput, maxinternal, maxOutput);
}