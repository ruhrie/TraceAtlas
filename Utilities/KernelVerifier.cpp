
#include "AtlasUtil/Format.h"
#include "tik/Util.h"
#include <fstream>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <nlohmann/json.hpp>
#include <set>

using namespace std;
using namespace llvm;

cl::opt<string> KernelFile(cl::Positional, cl::desc("Specify input kernel json filename"), cl::value_desc("kernel filename"));
cl::opt<string> BitcodeFile("b", cl::desc("Specify input bitcode filename"), cl::value_desc("bitcode filename"));
cl::opt<string> OutputFile("o", cl::desc("Specify output json filename"), cl::value_desc("json filename"));
cl::opt<bool> Preformat("pf", llvm::cl::desc("Bitcode is preformatted"), llvm::cl::value_desc("Bitcode is preformatted"));

int main(int argc, char *argv[])
{
    cl::ParseCommandLineOptions(argc, argv);
    LLVMContext context;
    SMDiagnostic smerror;
    unique_ptr<Module> sourceBitcode = parseIRFile(BitcodeFile, smerror, context);
    //annotate it with the same algorithm used in the tracer
    if (!Preformat)
    {
        Format(sourceBitcode.get());
    }

    ifstream inputJson(KernelFile);
    nlohmann::json j;
    inputJson >> j;
    inputJson.close();

    map<string, set<int64_t>> kernels;
    map<int64_t, BasicBlock *> blockMap;

    for (auto &[k, l] : j["Kernels"].items())
    {
        string index = k;
        nlohmann::json kernel = l["Blocks"];
        kernels[index] = kernel.get<set<int64_t>>();
    }
    set<int64_t> ValidBlocks;
    ValidBlocks = j["ValidBlocks"].get<set<int64_t>>();

    //build the blockMap
    for (auto &mi : *sourceBitcode)
    {
        for (auto fi = mi.begin(); fi != mi.end(); fi++)
        {
            auto *bb = cast<BasicBlock>(fi);
            int64_t id = GetBlockID(bb);
            blockMap[id] = bb;
        }
    }

    /*
    Checks to be done
    * Every block can reach itself somehow
    * Get kernel entrance number
    * Get kernel exit number
    * Get conditional number
    */
    map<string, map<string, int>> resultMap;
    for (const auto &kernel : kernels)
    {
        //start by checking that every block can reach itself
        bool allReachable = true;
        for (auto block : kernel.second)
        {
            //we need to see if this block can ever reach itself
            BasicBlock *base = blockMap[block];
            if (!TraceAtlas::tik::IsSelfReachable(base, kernel.second))
            {
                allReachable = false;
            }
        }
        if (allReachable)
        {
            resultMap[kernel.first]["Valid"] = 1;
        }
        else
        {
            resultMap[kernel.first]["Valid"] = 0;
        }

        set<BasicBlock *> llvmBlocks;
        for (auto block : kernel.second)
        {
            llvmBlocks.insert(blockMap[block]);
        }

        //now get the entrance count
        auto ent = TraceAtlas::tik::GetEntrances(llvmBlocks);
        resultMap[kernel.first]["Entrances"] = ent.size();

        //now get exit count
        auto ex = TraceAtlas::tik::GetExits(llvmBlocks);
        resultMap[kernel.first]["Exits"] = ex.size();

        auto cond = TraceAtlas::tik::GetConditionals(llvmBlocks, kernel.second);
        resultMap[kernel.first]["Conditionals"] = ex.size();

        bool hasEpilogue = TraceAtlas::tik::HasEpilogue(llvmBlocks, kernel.second);
        if (hasEpilogue)
        {
            resultMap[kernel.first]["Epilogue"] = 1;
        }
        else
        {
            resultMap[kernel.first]["Epilogue"] = 0;
        }
    }

    nlohmann::json finalJson = resultMap;
    ofstream oStream(OutputFile);
    oStream << finalJson;
    oStream.close();
    return 0;
}