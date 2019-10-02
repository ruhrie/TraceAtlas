
#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include "Passes/CommandArgs.h"
#include "Passes/TraceIO.h"
#include "Passes/Trace.h"
#include "Passes/Annotate.h"
#include "Passes/Functions.h"

using namespace llvm;

namespace DashTracer
{
	namespace Passes
	{
		bool TraceIO::runOnModule(Module& M)
		{
			appendToGlobalCtors(M, openFunc, 0);
			appendToGlobalDtors(M, closeFunc, 0);
			return true;
		}

		void TraceIO::getAnalysisUsage(AnalysisUsage& AU) const
		{
			AU.setPreservesAll();
		}
		bool TraceIO::doInitialization(Module& M)
		{
#ifdef LLVM_9
			openFunc = M.getOrInsertFunction("OpenFile", Type::getVoidTy(M.getContext()));
			closeFunc = M.getOrInsertFunction("CloseFile", Type::getVoidTy(M.getContext()));
#elif defined LLVM_8
			openFunc = dyn_cast<Function>(M.getOrInsertFunction("OpenFile", Type::getVoidTy(M.getContext())));
			closeFunc = dyn_cast<Function>(M.getOrInsertFunction("CloseFile", Type::getVoidTy(M.getContext())));
#endif
			return true;
		}
	} // namespace Passes
	char Passes::TraceIO::ID = 0;
	static RegisterPass<Passes::TraceIO> TraceIO("TraceIO", "Adds function calls to open/close trace files", true, false);
} // namespace DashTracer
