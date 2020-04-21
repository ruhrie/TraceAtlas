#pragma once
#include "Kernel.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <map>
#include <set>



/// Global pointer to the module of tik representations.
extern llvm::Module *TikModule;

//extern std::set<int64_t> ValidBlocks;
