#pragma once
#include <llvm/IR/Function.h>

using namespace llvm;

namespace DashTracer
{
    namespace Passes
    {
        extern Function *openFunc;
        extern Function *closeFunc;
        extern Function *BB_ID;
        extern Function *StoreDump;
        extern Function *DumpStoreValue;
        extern Function *LoadDump;
        extern Function *DumpLoadValue;
        extern Function *fullFunc;
        extern Function *fullAddrFunc;
        extern Function *MarkovOpen;
        extern Function *MarkovClose;
        extern Function *MarkovInit;
        extern Function *MarkovDestroy;
        extern Function *MarkovIncrement;
        extern Function *MarkovExit;
        extern Function *InstanceInit;
        extern Function *InstanceDestroy;
        extern Function *InstanceIncrement;
        extern Function *InstanceIncrement;
        extern Function *InstanceIncrement;
        extern Function *MemInstructionDump;
        extern Function *MemProfInitialization;
        extern Function *MemProfDestroy;
    } // namespace Passes
} // namespace DashTracer
