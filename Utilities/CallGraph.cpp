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
#include "llvm/Analysis/CallGraph.h"
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <iostream>

using namespace llvm;
using namespace std;

cl::opt<std::string> InputFilename("i", cl::desc("Specify input bitcode"), cl::value_desc("bitcode filename"), cl::Required);
cl::opt<std::string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("output filename"));
cl::opt<std::string> KernelFilename("k", cl::desc("Specify kernel json"), cl::value_desc("kernel filename"), cl::Required);

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    LLVMContext context;
    SMDiagnostic smerror;
    std::unique_ptr<Module> SourceBitcode = parseIRFile(InputFilename, smerror, context);

    // Annotate its bitcodes and values
    CleanModule(SourceBitcode.get());
    Format(SourceBitcode.get());

    map<int64_t, BasicBlock *> IDToBlock;
    map<int64_t, Value *> IDToValue;
    InitializeIDMaps(SourceBitcode.get(), IDToBlock, IDToValue);

    // Call graph, doesn't include function pointers
    CallGraph CG(*(SourceBitcode.get()));

    // Add function pointers
    for( auto& f : *SourceBitcode )
    {
        for( auto bb = f.begin(); bb != f.end(); bb++ )
        {
            for( auto it = bb->begin(); it != bb->end(); it++ )
            {
                if( auto CI = dyn_cast<CallInst>(it) )
                {
                    auto callee = CI->getCalledFunction();
                    if( callee == nullptr )
                    {
                        auto v  = CI->getCalledValue();
                        auto sv = v->stripPointerCasts();
                        string fname = sv->getName();
                        cout << "Indirect function call name is " << fname << endl;
                    }
                }
            }
        }
    }
    for( const auto& node : CG )
    {
        if( node.first == nullptr )
        {
            // null nodes represent theoretical entries in the call graph, see CallGraphNode class reference
            continue;
        }
        string fname = node.first->getName();
        cout << fname << endl;
        //for( auto item = node.second->begin(); item != node.second->end(); item++ )
        for( unsigned int i = 0; i < node.second->size(); i++ )
        {
            auto calledFunc = (*node.second)[i]->getFunction();
            if( calledFunc != nullptr )
            {
                string calledFName = calledFunc->getName();
                cout << calledFName << endl;
            }
        }
    }
    return 0;
}