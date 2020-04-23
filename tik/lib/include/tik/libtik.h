#include "tik/Kernel.h"
#include <llvm/IR/Module.h>
#include <map>

extern llvm::Module *TikModule;
extern std::map<llvm::Function *, std::shared_ptr<Kernel>> KfMap;
extern std::map<int64_t, std::shared_ptr<Kernel>> KernelMap;
