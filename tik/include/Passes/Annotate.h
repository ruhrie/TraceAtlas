#pragma once
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/Pass.h>

using namespace llvm;

namespace DashTracer
{
    namespace Passes
    {
        struct EncodedAnnotate : public ModulePass
        {
            static char ID;
            EncodedAnnotate() : ModulePass(ID) {}
            bool runOnModule(Module &M) override;
            void getAnalysisUsage(AnalysisUsage &AU) const override;
        };

    } // namespace Passes
} // namespace DashTracer
