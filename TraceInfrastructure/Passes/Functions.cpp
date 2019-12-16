#include "Passes/Functions.h"
#include <llvm/IR/Function.h>
using namespace llvm;

namespace DashTracer
{
    namespace Passes
    {
        Function *openFunc;
        Function *closeFunc;
        Function *BB_ID;
        Function *StoreDump;
        Function *DumpStoreValue;
        Function *LoadDump;
        Function *DumpLoadValue;
        Function *fullFunc;
        Function *fullAddrFunc;
        Function *fullAddrValueFunc;    
    } // namespace Passes
} // namespace DashTracer