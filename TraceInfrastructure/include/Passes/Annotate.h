#ifndef ANNOTATE_H
#define ANNOTATE_H
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/Pass.h>

using namespace llvm;

namespace DashTracer
{
    /// <summary>
    /// Annotates the input bitcode.
    /// </summary>
    void Annotate(Function *F);
} // namespace DashTracer

namespace DashTracer
{
    namespace Passes
    {
        /// <summary>
        /// The Annotate pass inserts UIDs to each instruction and block. It also appends the function GUID.
        /// </summary>
        struct Annotate : public FunctionPass
        {
            static char ID;
            Annotate() : FunctionPass(ID) {}
            bool runOnFunction(Function &F) override;
            void getAnalysisUsage(AnalysisUsage &AU) const override;
        };

        struct EncodedAnnotate : public ModulePass
        {
            static char ID;
            EncodedAnnotate() : ModulePass(ID) {}
            bool runOnModule(Module &M) override;
            void getAnalysisUsage(AnalysisUsage &AU) const override;
        };

    } // namespace Passes
} // namespace DashTracer

#endif