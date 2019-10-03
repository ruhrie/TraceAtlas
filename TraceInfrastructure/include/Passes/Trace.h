#ifndef TRACE_H
#define TRACE_H
#include <llvm/IR/BasicBlock.h>
#include <llvm/Pass.h>

using namespace llvm;

#pragma clang diagnostic ignored "-Woverloaded-virtual"

namespace DashTracer
{
    namespace Passes
    {
        /// <summary>
        /// The Trace pass is responsible for injecting the tracing code and calling the other passes.
        /// </summary>
        struct Trace : public BasicBlockPass
        {
            static char ID;
            Trace() : BasicBlockPass(ID) {}
            bool runOnBasicBlock(BasicBlock &BB) override;
            void getAnalysisUsage(AnalysisUsage &AU) const override;
            bool doInitialization(Module &M) override;
        };

        struct EncodedTrace : public BasicBlockPass
        {
            static char ID;
            EncodedTrace() : BasicBlockPass(ID) {}
            bool runOnBasicBlock(BasicBlock &BB) override;
            void getAnalysisUsage(AnalysisUsage &AU) const override;
            bool doInitialization(Module &M) override;
        };
    } // namespace Passes
} // namespace DashTracer
#endif