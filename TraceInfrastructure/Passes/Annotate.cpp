#include "Passes/Annotate.h"
#include "AtlasUtil/Annotate.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/raw_ostream.h>
#include <map>

using namespace llvm;

 
namespace DashTracer::Passes
{

    bool EncodedAnnotate::runOnModule(Module &M)
    {
        Annotate(&M);

        for (auto &mi : M)
        {
            for (auto fi = mi.begin(); fi != mi.end(); fi++)
            {
                auto *bb = cast<BasicBlock>(fi);                              
                int64_t id = GetBlockID(bb);
                BBidToPtr[id] = bb;
            }
        } 

        return true;
    }

    void EncodedAnnotate::getAnalysisUsage(AnalysisUsage &AU) const
    {
        AU.setPreservesAll();
    }
    char EncodedAnnotate::ID = 0;
    static RegisterPass<EncodedAnnotate> Z("EncodedAnnotate", "Renames the basic blocks in the module", true, false);
} // namespace DashTracer::Passes