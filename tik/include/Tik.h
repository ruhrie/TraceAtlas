#pragma once
#include "Kernel.h"
#include <llvm/IR/Module.h>
#include <map>

extern std::map<int, Kernel *> KernelMap;
extern llvm::Module *TikModule;
void ResolveFunctionCalls(void);