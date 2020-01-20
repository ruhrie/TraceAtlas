#include "AtlasUtil/Annotate.h"

using namespace llvm;

void Annotate(llvm::Module *M)
{
    int index = 0;
    for (auto mi = M->begin(); mi != M->end(); mi++)
    {
        Function *F = cast<Function>(mi);
        Annotate(F, index);
    }
}
void Annotate(llvm::Function *F, int &startingIndex)
{
    for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
    {
        BB->setName("BB_UID_" + std::to_string(startingIndex++));
    }
}