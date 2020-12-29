#pragma once
#include <llvm/Pass.h>
using namespace llvm;

namespace DashTracer
{
    namespace Passes
    {
        /// <summary>
        /// The TraceIO pass inserts file IO instructions to the source bitcode.
        /// </summary>
        struct MarkovIO : public ModulePass
        {
            static char ID;
            MarkovIO() : ModulePass(ID) {}
            bool runOnModule(Module &M) override;
            void getAnalysisUsage(AnalysisUsage &AU) const override;
        };
    } // namespace Passes
} // namespace DashTracer
