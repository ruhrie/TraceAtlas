#include <llvm/IR/Module.h>

void Annotate(llvm::Module *M);
void Annotate(llvm::Function *F, int &startingIndex);