
#include "Passes/MarkovIO.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Format.h"
#include "Passes/Annotate.h"
#include "Passes/CommandArgs.h"
#include "Passes/Functions.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
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
        GlobalVariable *tst;
        bool MarkovIO::runOnModule(Module &M)
        {
            uint64_t blockCount = GetBlockCount(&M);
            ConstantInt *i = ConstantInt::get(Type::getInt64Ty(M.getContext()), blockCount);
            tst = new GlobalVariable(M, i->getType(), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, i, "MarkovBlockCount");
            return true;
        }

        void MarkovIO::getAnalysisUsage(AnalysisUsage &AU) const
        {
            AU.setPreservesAll();
        }
    } // namespace Passes
    char Passes::MarkovIO::ID = 0;
    static RegisterPass<Passes::MarkovIO> MarkovIO("MarkovIO", "Adds function calls to open/close markov files", true, false);
} // namespace DashTracer
