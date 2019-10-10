#include "Tik.h"
#include <llvm/Support/CommandLine.h>
#include <string>
#include <set>
#include <fstream>
#include <iostream>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IRReader/IRReader.h>
#include <json.hpp>

using namespace std;
using namespace llvm;

enum Filetype
{
    LLVM,
    DPDA
};

std::map<int, Kernel*> KernelMap;

cl::opt<string> JsonFile("j", cl::desc("Specify input json filename"), cl::value_desc("json filename"));
cl::opt<string> KernelFile("o", cl::desc("Specify output kernel filename"), cl::value_desc("kernel filename"));
cl::opt<string> InputFile(cl::Positional, cl::Required, cl::desc("<input file>"));
cl::opt<Filetype> InputType("t", cl::desc("Choose input file type"),
                            cl::values(
                                clEnumVal(LLVM, "LLVM IR"),
                                clEnumVal(DPDA, "DPDA DSL")),
                            cl::init(LLVM));

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

    map<string, vector<string>> childParentMapping;

    for (auto element : kernels)
    {
        for (auto comparison : kernels)
        {
            if (element.first != comparison.first)
            {
                if (element.second < comparison.second)
                {
                    vector<int> i;
                    set_intersection(element.second.begin(), element.second.end(), comparison.second.begin(), comparison.second.end(), back_inserter(i));
                    if (i == comparison.second)
                    {
                        childParentMapping[element.first].push_back(comparison.first);
                    }
                }
            }
        }
    }

    //load the llvm file
    LLVMContext context;
    SMDiagnostic smerror;
    unique_ptr<Module> sourceBitcode = parseIRFile(InputFile, smerror, context);
    //annotate it with the same algorithm used in the tracer
    static uint64_t UID = 0;
    for (Module::iterator F = sourceBitcode->begin(), E = sourceBitcode->end(); F != E; ++F)
    {
        for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
        {
            BB->setName("BB_UID_" + std::to_string(UID++));
        }
    }			

    TikModule = new Module(InputFile, context);

    //we now process all kernels who have no children and then remove them as we go

    std::vector<Kernel*> results;

    bool change = true;
    while(change)
    {
        change = false;
        for(auto kernel : kernels)
        {
            if(childParentMapping.find(kernel.first) == childParentMapping.end())
            {
                //this kernel has no unexplained parents
                Kernel *kern = new Kernel(kernel.second, sourceBitcode.get());
                //so we remove its blocks from all parents
                vector<string> toRemove;
                for(auto child : childParentMapping)
                {
                    auto loc = find(child.second.begin(), child.second.end(), kernel.first);
                    if (loc != child.second.end())
                    {
                        child.second.erase(loc);
                        if(child.second.size() == 0)
                        {
                            toRemove.push_back(child.first);
                        }
                    }
                }
                //if necessary remove the entry from the map
                for(auto r : toRemove)
                {
                    auto it = childParentMapping.find(r);
                    childParentMapping.erase(it);
                }
                //publish our result
                results.push_back(kern);
                change = true;
                for(auto block : kernel.second)
                {
                    KernelMap[block] = kern;
                }
                //and remove it from kernels
                auto it = find(kernels.begin(), kernels.end(), kernel);
                kernels.erase(it);
                //and restart the iterator to ensure cohesion
                break;
            }            
        }
    }

    nlohmann::json finalJson;
    for(Kernel* kern : results)
    {
        finalJson["Kernels"][kern->Name] = kern->GetJson();
    }

    ofstream oStream(KernelFile);
    oStream << finalJson;
    oStream.close();
    return 0;
}