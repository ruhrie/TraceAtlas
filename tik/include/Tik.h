#pragma once
#include "Kernel.h"
#include <llvm/IR/Module.h>
#include <map>

extern std::map<int, Kernel *> KernelMap;
llvm::Module *TikModule;