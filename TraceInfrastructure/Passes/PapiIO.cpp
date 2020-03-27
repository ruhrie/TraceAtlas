
#include "Passes/PapiIO.h"
#include "Passes/Annotate.h"
#include "Passes/CommandArgs.h"
#include "Passes/PapiExport.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/Pass.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <zlib.h>

using namespace llvm;

namespace DashTracer
{
    namespace Passes
    {
        Function *papiInitiate;
        Function *papiTerminate;
        auto PapiIO::runOnModule(Module &M) -> bool
        {
            appendToGlobalCtors(M, papiInitiate, 0);
            appendToGlobalDtors(M, papiTerminate, 0);
            return true;
        }

        void PapiIO::getAnalysisUsage(AnalysisUsage &AU) const
        {
            AU.setPreservesAll();
        }
        auto PapiIO::doInitialization(Module &M) -> bool
        {
            papiTerminate = cast<Function>(M.getOrInsertFunction("TerminatePapi", Type::getVoidTy(M.getContext())).getCallee());
            papiInitiate = cast<Function>(M.getOrInsertFunction("InitializePapi", Type::getVoidTy(M.getContext())).getCallee());
            return true;
        }
    } // namespace Passes
    char Passes::PapiIO::ID = 0;
    static RegisterPass<Passes::PapiIO> TraceIO("PapiIO", "Adds function calls to open/close papi instrumentation", true, false);

} // namespace DashTracer
