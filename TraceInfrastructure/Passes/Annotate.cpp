#include "Passes/Annotate.h"
#include "AtlasUtil/Annotate.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace DashTracer::Passes
{
    auto EncodedAnnotate::runOnModule(Module &M) -> bool
    {
        Annotate(&M);
        return true;
    }

    void EncodedAnnotate::getAnalysisUsage(AnalysisUsage &AU) const
    {
        AU.setPreservesAll();
    }
    char EncodedAnnotate::ID = 0;
    static RegisterPass<EncodedAnnotate> Z("EncodedAnnotate", "Renames the basic blocks in the module", true, false);
} // namespace DashTracer::Passes