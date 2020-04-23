#include "tik/Kernel.h"
#include <llvm/IR/Module.h>
#include <map>

extern llvm::Module *TikModule;
extern std::map<llvm::Function *, Kernel *> KfMap;
extern std::map<int64_t, Kernel *> KernelMap;
