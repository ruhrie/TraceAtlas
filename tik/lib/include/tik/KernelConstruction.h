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

void GetEntrances(Kernel *kernel, std::set<llvm::BasicBlock *> &blocks);

void GetExits(Kernel *kernel, std::set<llvm::BasicBlock *> &blocks);

void GetExternalValues(Kernel *kernel, llvm::ValueToValueMapTy &VMap, std::set<llvm::BasicBlock *> &blocks);

void BuildKernel(Kernel *kernel, llvm::ValueToValueMapTy &VMap, std::set<llvm::BasicBlock *> &blocks);

void ApplyMetadata(Kernel *kernel, std::string &Name, std::map<llvm::Value *, llvm::GlobalObject *> &GlobalMap);

void CopyOperand(llvm::User *inst, llvm::ValueToValueMapTy &VMap);

void RemapOperands(llvm::User *op, llvm::Instruction *inst, llvm::ValueToValueMapTy &VMap);

void Remap(Kernel *kernel, llvm::ValueToValueMapTy &VMap);

void InlineFunctions(Kernel *kernel, std::set<int64_t> &blocks);

void CopyGlobals(Kernel *kernel, llvm::ValueToValueMapTy &VMap);

void Repipe(Kernel *kernel);

void ExportFunctionSignatures(Kernel *kernel);

void BuildInit(Kernel *kernel, llvm::ValueToValueMapTy &VMap);

void BuildExit(Kernel *kernel, llvm::ValueToValueMapTy &VMap);

void RemapNestedKernels(Kernel *kernel, llvm::ValueToValueMapTy &VMap);

void RemapExports(Kernel *kernel, llvm::ValueToValueMapTy &VMap);

void PatchPhis(Kernel *kernel);
