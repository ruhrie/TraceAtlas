#pragma once
#include <llvm/IR/BasicBlock.h>
#include <llvm/Pass.h>

using namespace llvm;

namespace DashTracer
{
    namespace Passes
    {

        struct PapiExport : public FunctionPass
        {
            static char ID;
            PapiExport() : FunctionPass(ID) {}
            bool runOnFunction(Function &F) override;
            bool doInitialization(Module &M) override;
            void getAnalysisUsage(AnalysisUsage &AU) const override;
        };

    } // namespace Passes
} // namespace DashTracer
