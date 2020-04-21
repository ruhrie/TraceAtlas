#include "Kernel.h"
#include <llvm/IR/BasicBlock.h>
#include <set>
#include <string>

/// @brief  Maps a basic block ID to a kernel object.
///
/// When making decisions about termination instructions, its important to know
/// which basic blocks are valid and which are not
extern std::map<int64_t, Kernel *> KernelMap;

/// @brief  Maps kernel functions to their objects
///
/// When making embedded function calls, it is necessary
/// to get the object in which that embedded function belongs.
extern std::map<llvm::Function *, Kernel *> KfMap;

void GetEntrances(std::set<llvm::BasicBlock *> &blocks, std::set<llvm::BasicBlock*>& Entrances);

void GetExits(std::set<llvm::BasicBlock *> &blocks, std::map<int, llvm::BasicBlock *>& ExitTarget);

std::vector<llvm::Value *> GetExternalValues(llvm::Function* KernelFunction, std::set<llvm::BasicBlock *> &blocks, llvm::ValueToValueMapTy& VMap, std::vector<llvm::Value* >& KernelImports);

void BuildKernel(llvm::Function* KernelFunction, std::set<llvm::BasicBlock *> &blocks, std::set<llvm::BasicBlock *>& Conditional, std::set<llvm::BasicBlock*>& Entrances, llvm::BasicBlock* Exception, llvm::BasicBlock* Exit, std::map<llvm::BasicBlock *, int>& ExitMap, llvm::ValueToValueMapTy VMap, llvm::BasicBlock* Init);

void ApplyMetadata(std::set<llvm::BasicBlock *>& Conditional, std::string& Name, std::map<llvm::Value *, llvm::GlobalObject *> &GlobalMap);

