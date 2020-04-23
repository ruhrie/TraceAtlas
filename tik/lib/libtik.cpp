#include "tik/libtik.h"
#include "tik/Kernel.h"
#include <map>

llvm::Module *TikModule;
std::map<int64_t, Kernel *> KernelMap;
std::map<llvm::Function *, Kernel *> KfMap;