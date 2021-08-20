#include "Passes/MemProfileIO.h"
#include "Passes/Annotate.h"
#include "Passes/CommandArgs.h"
#include "Passes/Functions.h"
#include "Passes/TraceMem.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/Pass.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

using namespace llvm;

namespace DashTracer
{
    namespace Passes
    {
        bool MemProfileIO::runOnModule(Module &M)
        {
            appendToGlobalCtors(M, openFunc, 0);
            appendToGlobalDtors(M, closeFunc, 0);
            return true;
        }

        void MemProfileIO::getAnalysisUsage(AnalysisUsage &AU) const
        {
            AU.setPreservesAll();
        }
        bool MemProfileIO::doInitialization(Module &M)
        {
            openFunc = cast<Function>(M.getOrInsertFunction("OpenMemProfileFile", Type::getVoidTy(M.getContext())).getCallee());
            closeFunc = cast<Function>(M.getOrInsertFunction("CloseMemProfileFile", Type::getVoidTy(M.getContext())).getCallee());
            return true;
        }
    } // namespace Passes
    char Passes::MemProfileIO::ID = 0;
    static RegisterPass<Passes::MemProfileIO> MemProfileIO("MemProfileIO", "Adds function calls to open/close memory profiler result files", true, false);
} // namespace DashTracer
