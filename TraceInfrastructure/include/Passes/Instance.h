#pragma once
#include <llvm/Pass.h>

using namespace llvm;

#pragma clang diagnostic ignored "-Woverloaded-virtual"

namespace DashTracer
{
    namespace Passes
    {
        struct Instance : public FunctionPass
        {
            static char ID;
            Instance() : FunctionPass(ID) {}
            bool runOnFunction(Function &F) override;
            void getAnalysisUsage(AnalysisUsage &AU) const override;
            bool doInitialization(Module &M) override;
        };
    } // namespace Passes
} // namespace DashTracer
