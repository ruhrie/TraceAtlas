#include <llvm/IR/Function.h>
#include "Passes/Functions.h"
using namespace llvm;

namespace DashTracer
{
    namespace Passes
    {
#ifdef LLVM_9
		FunctionCallee openFunc;
		FunctionCallee closeFunc;
        FunctionCallee BB_ID;
		FunctionCallee StoreDump;
		FunctionCallee LoadDump;
#elif defined LLVM_8
		Function* openFunc;
		Function* closeFunc;
        Function* BB_ID;
		Function* StoreDump;
		Function* LoadDump;
        Function* fullFunc;
        Function* fullAddrFunc;
#endif
    }
}