#ifndef FUNCTIONS_H
#define FUNCTIONS_H

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
        extern Function *fullAddrValueFunc;
    } // namespace Passes
} // namespace DashTracer

#endif