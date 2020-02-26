#pragma once
#include "Kernel.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <map>
#include <memory>

/// @brief  Maps a basic block ID to a kernel object.
///
/// When making decisions about termination instructions, its important to know
/// which basic blocks are valid and which are not
extern std::map<int, std::shared_ptr<Kernel>> KernelMap;

/// Global pointer to the module of tik representations.
extern llvm::Module *TikModule;

/// @brief  Maps kernel functions to their objects
///
/// When making embedded function calls, it is necessary
/// to get the object in which that embedded function belongs.
extern std::map<llvm::Function *, std::shared_ptr<Kernel>> KfMap;
