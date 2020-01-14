#include <llvm/IR/Function.h>
#include <map>
#include <string>

std::map<std::string, int> GetRatios(llvm::Function *F);
std::map<std::string,std:: map<std::string, int>> GetCrossProductTypePerOp(llvm::Function *F);