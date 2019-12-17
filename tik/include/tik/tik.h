#pragma once
#include "Kernel.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <map>


extern std::map<int, Kernel *> KernelMap;
extern llvm::Module *TikModule;
extern std::map<llvm::Function *, Kernel*> KfMap;
