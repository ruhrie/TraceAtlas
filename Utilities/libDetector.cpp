#include "AtlasUtil/Annotate.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
using namespace std;
using namespace llvm;

cl::opt<string> JsonFile("k", cl::desc("Specify input kernel json filename"), cl::value_desc("kernel filename"));
cl::opt<string> InputFile("i", cl::desc("Specify input bitcode filename"), cl::value_desc("bitcode filename"));
cl::opt<string> OutputFile("o", cl::desc("Specify output json filename"), cl::value_desc("json filename"));

auto main(int argc, char *argv[]) -> int
{
    cl::ParseCommandLineOptions(argc, argv);
    ifstream inputJson(JsonFile);
    nlohmann::json j;
    inputJson >> j;
    inputJson.close();

    map<string, vector<int>> kernels;

    for (auto &[key, value] : j.items())
    {
        string index = key;

        nlohmann::json kernel;

        if (!value[0].empty() && value[0].is_array())
        {
            //embedded layout
            kernel = value[0];
        }
        else
        {
            kernel = value;
        }

        kernels[index] = kernel.get<vector<int>>();
    }

    //load the llvm file
    LLVMContext context;
    SMDiagnostic smerror;
    unique_ptr<Module> sourceBitcode = parseIRFile(InputFile, smerror, context);
    //annotate it with the same algorithm used in the tracer
    Annotate(sourceBitcode.get());
    map<string, set<string>> kernelParents;
    for (Module::iterator F = sourceBitcode->begin(), E = sourceBitcode->end(); F != E; ++F)
    {
        auto *f = cast<Function>(F);
        for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
        {
            auto *b = cast<BasicBlock>(BB);
            int64_t id = GetBlockID(b);
            for (const auto &kernel : kernels)
            {
                auto blocks = kernel.second;
                if (find(blocks.begin(), blocks.end(), id) != blocks.end())
                {
                    if (MDNode *N = f->getMetadata("libs"))
                    {
                        string parent = cast<MDString>(N->getOperand(0))->getString();
                        kernelParents[kernel.first].insert(parent);
                    }
                }
            }
        }
    }

    nlohmann::json finalJson = kernelParents;
    ofstream oStream(OutputFile);
    oStream << finalJson;
    oStream.close();
    return 0;
}