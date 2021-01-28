#include "llvm/Analysis/CallGraph.h"
#include "AtlasUtil/Format.h"
#include "AtlasUtil/Print.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
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
    map<string, vector<int64_t>> blockCallers;
    for (const auto &bbid : j.items())
    {
        blockCallers[bbid.key()] = j[bbid.key()]["BlockCallers"].get<vector<int64_t>>();
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
    CallGraph CG(*(SourceBitcode.get()));

    // Add function pointers
    for (auto &f : *SourceBitcode)
    {
        for (auto bb = f.begin(); bb != f.end(); bb++)
        {
            for (auto it = bb->begin(); it != bb->end(); it++)
            {
                if (auto CI = dyn_cast<CallInst>(it))
                {
                    auto callee = CI->getCalledFunction();
                    if (callee == nullptr)
                    {
                        // try to find a block caller entry for this function, if it's not there we have to move on
                        auto BBID = GetBlockID(cast<BasicBlock>(bb));
                        if (blockCallers.find(to_string(BBID)) != blockCallers.end())
                        {
                            if (!blockCallers[to_string(BBID)].empty())
                            {
                                auto calleeID = blockCallers[to_string(BBID)].front(); // should only be 1 element long, because of basic block splitting
                                auto calleeBlock = IDToBlock[calleeID];
                                if (calleeBlock != nullptr)
                                {
                                    auto parentNode = CG.getOrInsertFunction(bb->getParent());
                                    auto childNode = CG.getOrInsertFunction(calleeBlock->getParent());
                                    parentNode->addCalledFunction(CI, childNode);
                                    BlockToFPtr[cast<BasicBlock>(bb)] = calleeBlock->getParent();
                                }
                            }
                            else
                            {
                                spdlog::warn("BlockCallers did not contain an entry for the indirect function call in BBID " + to_string(BBID));
                            }
                        }
                        else
                        {
                            spdlog::warn("BlockCallers did not contain an entry for the indirect call in BBID " + to_string(BBID));
                        }
                    }
                }
            }
        }
    }
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