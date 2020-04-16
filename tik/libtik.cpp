#include "tik/tik.h"
llvm::Module *TikModule;
std::map<int64_t, Kernel *> KernelMap;
std::map<llvm::Function *, Kernel *> KfMap;