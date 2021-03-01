#include "AtlasUtil/Format.h"
#include "AtlasUtil/Path.h"
#include "AtlasUtil/Print.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/SourceMgr.h"
#include <fstream>
#include <iomanip>
#include <map>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using namespace llvm;
using namespace std;
using json = nlohmann::json;

cl::opt<std::string> InputFilename("i", cl::desc("Specify input bitcode"), cl::value_desc("bitcode filename"), cl::Required);
cl::opt<std::string> BlockInfo("j", cl::desc("Specify BlockInfo json"), cl::value_desc("BlockInfo filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"));

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);

    ifstream inputJson;
    nlohmann::json j;
    try
    {
        inputJson.open(BlockInfo);
        inputJson >> j;
        inputJson.close();
    }
    catch (exception &e)
    {
        spdlog::critical("Couldn't open input json file: " + BlockInfo);
        spdlog::critical(e.what());
        return EXIT_FAILURE;
    }
    map<int64_t, int64_t> blockCallers;
    for (const auto &bbid : j.items())
    {
        auto vec = j[bbid.key()]["BlockCallers"].get<vector<int64_t>>();
        if (vec.size() == 1)
        {
            blockCallers[stol(bbid.key())] = *vec.begin();
        }
        else if (vec.size() > 1)
        {
            throw AtlasException("Found more than one entry in a blockCaller key!");
        }
    }

    LLVMContext context;
    SMDiagnostic smerror;
    std::unique_ptr<Module> SourceBitcode = parseIRFile(InputFilename, smerror, context);
    if (SourceBitcode.get() == nullptr)
    {
        spdlog::critical("Failed to open bitcode file: " + InputFilename);
        return EXIT_FAILURE;
    }

    // Annotate its bitcodes and values
    CleanModule(SourceBitcode.get());
    Format(SourceBitcode.get());

    map<int64_t, BasicBlock *> IDToBlock;
    map<int64_t, Value *> IDToValue;
    map<BasicBlock *, Function *> BlockToFPtr;
    InitializeIDMaps(SourceBitcode.get(), IDToBlock, IDToValue);

    // Call graph, doesn't include function pointers
    auto CG = getCallGraph(SourceBitcode.get(), blockCallers, BlockToFPtr, IDToBlock);

    json outputJson;
    for (const auto &node : CG)
    {
        if (node.first == nullptr)
        {
            // null nodes represent theoretical entries in the call graph, see CallGraphNode class reference
            continue;
        }
        string fname = node.first->getName();
        outputJson[fname] = std::vector<string>();
        for (unsigned int i = 0; i < node.second->size(); i++)
        {
            auto calledFunc = (*node.second)[i]->getFunction();
            if (calledFunc != nullptr)
            {
                string calledFName = calledFunc->getName();
                outputJson[node.first->getName()].push_back(calledFunc->getName());
            }
            // null function calls can still exist, this tool only adds functions to the callgraph
        }
    }
    ofstream oStream(OutputFilename);
    oStream << setw(4) << outputJson;
    oStream.close();
    return 0;
}