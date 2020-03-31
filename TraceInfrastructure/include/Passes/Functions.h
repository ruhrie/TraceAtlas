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
        extern Function *StoreValue;
        extern Function *LoadDump;
        extern Function *LoadValue;
        extern Function *fullFunc;
        extern Function *fullAddrFunc;
    } // namespace Passes
} // namespace DashTracer

#endif