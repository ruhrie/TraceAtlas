#pragma once
#include <llvm/IR/BasicBlock.h>
#include <llvm/Pass.h>

using namespace llvm;

#pragma clang diagnostic ignored "-Woverloaded-virtual"

namespace DashTracer
{
    namespace Passes
    {
        struct EncodedTraceMemory : public FunctionPass
        {
            static char ID;
            EncodedTraceMemory() : FunctionPass(ID) {}
            bool runOnFunction(Function &F) override;
            void getAnalysisUsage(AnalysisUsage &AU) const override;
            bool doInitialization(Module &M) override;
        };
    } // namespace Passes
} // namespace DashTracer
