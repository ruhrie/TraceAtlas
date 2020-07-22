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
#include <string>

using namespace std;
using namespace llvm;
using namespace WorkingSet;
map<int64_t, set<uint64_t>> kernelMap;
map<int64_t,vector<uint64_t>> BBMemInstSize;

set<int64_t> ValidBlock;
llvm::cl::opt<string> inputTrace("i", llvm::cl::desc("Specify the input trace filename"), llvm::cl::value_desc("trace filename"));
cl::opt<std::string> KernelFilename("k", cl::desc("Specify kernel json"), cl::value_desc("kernel filename"), cl::Required);
llvm::cl::opt<string> bitcodeFile("b", llvm::cl::desc("Specify bitcode name"), llvm::cl::value_desc("bitcode filename"), llvm::cl::Required);
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
        for (auto it :kernel.get<set<int64_t>>())
        {
            kernelMap[it].insert(index);
        }
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
                    }
                    else if(auto *inst = dyn_cast<StoreInst>(bi))
                    {
                        auto *type = inst->getPointerOperand()->getType()->getContainedType(0);
                        uint64_t dataSize = dl.getTypeAllocSize(type);
                        //errs()<< *inst<<"\n";
                        //BBMemInstSize[id]
                        BBMemInstSize[id].push_back(dataSize);
                    }
                }              
            }
        }
    }

    ProcessTrace(inputTrace, &WorkingSet::Process, "working set analysis", false);
    //ProcessTrace(inputTrace, &WorkingSet::ProcessBlock, "working set analysis", false);

    
    //here calculates the maximum internal working set size
    map<uint64_t,vector<pair<int64_t,uint64_t>>> internalTimeStampMap;
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

                // for dynamic size printing
                pair<int64_t,uint64_t> timePoint;
                timePoint.first = it.birthTime;
                timePoint.second = endTimeSet.size();
                internalTimeStampMap[itout.first].push_back(timePoint);
                
                if (endTimeSet.size() > maxinternal)
                {
                    maxinternal = endTimeSet.size();
                }
            }
        }
        if (maxinternalfiring[itout.first]>maxinternal)
        {
            maxinternal = maxinternalfiring[itout.first];
        }
        maxInput = itout.second.inputMapSize;
        maxOutput = itout.second.outputAddressIndexSet.size();
        printf("maxInput: %lu \n maxinternal: %lu \n maxOutput: %lu \n", maxInput, maxinternal, maxOutput);
    }
    string   fileName;
    for (auto &it: internalTimeStampMap)
    {
        fileName = string("kernel") + to_string(it.first)+string(".txt");
        ofstream mycout(fileName.c_str());
        for(auto &itiner: it.second)
        {
            mycout<< itiner.first<< "   " <<itiner.second<<endl;
        } 
    }
}