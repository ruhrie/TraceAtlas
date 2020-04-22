#include "Kernel.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/User.h>
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

void GetEntrances(std::set<llvm::BasicBlock *> &blocks, std::set<llvm::BasicBlock *> &Entrances);

void GetExits(std::set<llvm::BasicBlock *> &blocks, std::map<int, llvm::BasicBlock *> &ExitTarget);

std::vector<llvm::Value *> GetExternalValues(std::set<llvm::BasicBlock *> &blocks, llvm::ValueToValueMapTy &VMap, std::vector<llvm::Value *> &KernelImports);

void BuildKernel(llvm::Function *KernelFunction, std::set<llvm::BasicBlock *> &blocks, std::set<llvm::BasicBlock *> &Conditional, std::set<llvm::BasicBlock *> &Entrances, llvm::BasicBlock *Exception, llvm::BasicBlock *Exit, std::map<llvm::BasicBlock *, int> &ExitMap, llvm::ValueToValueMapTy &VMap, llvm::BasicBlock *Init);

void ApplyMetadata(llvm::Function *KernelFunction, std::set<llvm::BasicBlock *> &Conditional, std::string &Name, std::map<llvm::Value *, llvm::GlobalObject *> &GlobalMap);

void CopyOperand(llvm::User *inst, llvm::ValueToValueMapTy &VMap);

void RemapOperands(llvm::User *op, llvm::Instruction *inst, llvm::ValueToValueMapTy &VMap);

void Remap(llvm::ValueToValueMapTy &VMap, llvm::Function *KernelFunction);

void InlineFunctions(llvm::Function *KernelFunction, std::set<int64_t> &blocks, llvm::BasicBlock *Init, llvm::BasicBlock *Exception, llvm::BasicBlock *Exit);

void CopyGlobals(llvm::Function *KernelFunction, llvm::ValueToValueMapTy &VMap);

void Repipe(llvm::Function *KernelFunction, llvm::BasicBlock *Exit, std::map<llvm::BasicBlock *, llvm::BasicBlock *> ExitBlockMap);

void ExportFunctionSignatures(llvm::Function *KernelFunction);

void BuildInit(llvm::Function *KernelFunction, llvm::ValueToValueMapTy &VMap, llvm::BasicBlock *Init, llvm::BasicBlock *Exception, std::set<llvm::BasicBlock *> &Entrances);

void BuildExit(llvm::ValueToValueMapTy &VMap, llvm::BasicBlock *Exit, llvm::BasicBlock *Exception, std::map<llvm::BasicBlock *, int> &ExitMap);

void RemapNestedKernels(llvm::Function *KernelFunction, llvm::ValueToValueMapTy &VMap, std::map<llvm::Argument *, llvm::Value *> &ArgumentMap);

void RemapExports(llvm::Function *KernelFunction, llvm::ValueToValueMapTy &VMap, llvm::BasicBlock *Init, std::vector<llvm::Value *> &KernelExports);

void PatchPhis(llvm::Function *KernelFunction);
