#pragma once
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/Pass.h>

using namespace llvm;
using namespace std;
namespace DashTracer
{
    namespace Passes
    {
        struct EncodedAnnotate : public ModulePass
        {
            map<int64_t, BasicBlock *> BBidToPtr;
            static char ID;
            EncodedAnnotate() : ModulePass(ID) {}
            bool runOnModule(Module &M) override;
            void getAnalysisUsage(AnalysisUsage &AU) const override;
            map<int64_t, BasicBlock *> &getIDmap(){ return BBidToPtr; }
        };

    } // namespace Passes
} // namespace DashTracer
