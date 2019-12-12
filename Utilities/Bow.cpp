#include <llvm/IRReader/IRReader.h>
#include <fstream>
#include <iostream>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/CommandLine.h>
#include <string>
#include <llvm/IR/Module.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>
using namespace std;
using namespace llvm;


cl::opt<string> JsonFile("j", cl::desc("Specify input json filename"), cl::value_desc("json filename"));
cl::opt<string> NameFile("o", cl::desc("Specify output name filename"), cl::value_desc("name filename"));
cl::opt<string> InputFile(cl::Positional, cl::Required, cl::desc("<input file>"));

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
    if (sourceBitcode == NULL)
    {
        std::cerr << "Failed to load bitcode file\n";
        return -1;
    }
    //annotate it with the same algorithm used in the tracer
    static uint64_t UID = 0;
    for (Module::iterator F = sourceBitcode->begin(), E = sourceBitcode->end(); F != E; ++F)
    {
        for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
        {
            BB->setName("BB_UID_" + std::to_string(UID++));
        }
    }	
    map<string, set<string>> kernelParents;
    for (Module::iterator F = sourceBitcode->begin(), E = sourceBitcode->end(); F != E; ++F)
    {
        Function *f = cast<Function>(F);
        string functionName = f->getName();
        for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
        {
            BasicBlock *b = cast<BasicBlock>(BB);
            string blockName = b->getName();
            uint64_t id = std::stoul(blockName.substr(7));
            for(auto kernel : kernels)
            {
                auto blocks = kernel.second;
                if(find(blocks.begin(), blocks.end(), id) != blocks.end())
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