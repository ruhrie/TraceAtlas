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
        Function *DumpStoreAddrValue;
        Function *LoadDump;
        Function *DumpLoadAddrValue;
        Function *fullFunc;
        Function *fullAddrFunc;
    } // namespace Passes
} // namespace DashTracer