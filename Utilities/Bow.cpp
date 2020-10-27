#include "AtlasUtil/Format.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <nlohmann/json.hpp>
#include <set>
using namespace std;
using namespace llvm;

cl::opt<string> JsonFile("j", cl::desc("Specify input json filename"), cl::value_desc("json filename"));
cl::opt<string> NameFile("o", cl::desc("Specify output name filename"), cl::value_desc("name filename"));
cl::opt<string> InputFile(cl::Positional, cl::Required, cl::desc("<input file>"));
cl::opt<bool> Preformat("pf", llvm::cl::desc("Bitcode is preformatted"), llvm::cl::value_desc("Bitcode is preformatted"));

int main(int argc, char *argv[])
{
    cl::ParseCommandLineOptions(argc, argv);
    std::cout << InputFile << " " << JsonFile << "\n";
    ifstream inputJson(JsonFile);
    nlohmann::json j;
    inputJson >> j;
    inputJson.close();

    map<string, vector<int>> kernels;

    for (auto &[key, value] : j.items())
    {
        string index = key;
        vector<int> kernel = value;
        kernels[index] = kernel;
    }

    //load the llvm file
    LLVMContext context;
    SMDiagnostic smerror;
    unique_ptr<Module> sourceBitcode = parseIRFile(InputFile, smerror, context);
    if (sourceBitcode == nullptr)
    {
        std::cerr << "Failed to load bitcode file\n";
        return -1;
    }
    Module *M = sourceBitcode.get();
    //annotate it with the same algorithm used in the tracer
    if (!Preformat)
    {
        Format(M);
    }
    map<string, set<string>> kernelParents;
    for (Module::iterator F = M->begin(), E = M->end(); F != E; ++F)
    {
        auto *f = cast<Function>(F);
        string functionName = f->getName();
        for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
        {
            auto *b = cast<BasicBlock>(BB);
            int64_t id = GetBlockID(b);
            for (const auto &kernel : kernels)
            {
                auto blocks = kernel.second;
                if (find(blocks.begin(), blocks.end(), id) != blocks.end())
                {
                    kernelParents[kernel.first].insert(functionName);
                }
            }
        }
    }

    nlohmann::json finalJson = kernelParents;
    ofstream oStream(NameFile);
    oStream << finalJson;
    oStream.close();
    return 0;
}