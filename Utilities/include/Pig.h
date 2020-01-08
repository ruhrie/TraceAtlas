#include <llvm/IR/Function.h>
#include <map>
#include <string>

std::map<std::string, int> GetRatios(llvm::Function *F);
int GetCrossProduct(llvm::Function *F);