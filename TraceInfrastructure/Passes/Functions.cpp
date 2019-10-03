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
        Function *LoadDump;
        Function *fullFunc;
        Function *fullAddrFunc;
    } // namespace Passes
} // namespace DashTracer