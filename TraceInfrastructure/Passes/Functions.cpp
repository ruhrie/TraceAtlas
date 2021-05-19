#include "Passes/Functions.h"
#include <llvm/IR/Function.h>
using namespace llvm;

namespace DashTracer::Passes
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
    Function *MarkovOpen;
    Function *MarkovClose;
    Function *MarkovInit;
    Function *MarkovDestroy;
    Function *MarkovIncrement;
    Function *MarkovExit;
} // namespace DashTracer::Passes