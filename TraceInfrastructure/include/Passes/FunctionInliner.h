#pragma once
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/Pass.h>

using namespace llvm;

namespace DashTracer
{
    namespace Passes
    {
        struct FunctionInliner : public FunctionPass
        {
            static char ID;
            FunctionInliner() : FunctionPass(ID) {}
            bool runOnFunction(Function &F) override;
        };

    } // namespace Passes
} // namespace DashTracer