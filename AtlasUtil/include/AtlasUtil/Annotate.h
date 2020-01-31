#include <llvm/IR/Constants.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
void Annotate(llvm::Module *M);
void Annotate(llvm::Function *F, uint64_t &startingIndex);
int64_t GetBlockID(llvm::BasicBlock *BB);
void SetBlockID(llvm::BasicBlock *BB, uint64_t i);