
#include "Passes/TraceIO.h"
#include "Passes/Annotate.h"
#include "Passes/CommandArgs.h"
#include "Passes/Functions.h"
#include "Passes/Trace.h"
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
        bool TraceIO::runOnModule(Module &M)
        {
            appendToGlobalCtors(M, openFunc, 0);
            appendToGlobalDtors(M, closeFunc, 0);
            return true;
        }

        void TraceIO::getAnalysisUsage(AnalysisUsage &AU) const
        {
            AU.setPreservesAll();
        }
        bool TraceIO::doInitialization(Module &M)
        {
            openFunc = cast<Function>(M.getOrInsertFunction("OpenFile", Type::getVoidTy(M.getContext())).getCallee());
            closeFunc = cast<Function>(M.getOrInsertFunction("CloseFile", Type::getVoidTy(M.getContext())).getCallee());
            return true;
        }
    } // namespace Passes
    char Passes::TraceIO::ID = 0;
    static RegisterPass<Passes::TraceIO> TraceIO("TraceIO", "Adds function calls to open/close trace files", true, false);
} // namespace DashTracer
