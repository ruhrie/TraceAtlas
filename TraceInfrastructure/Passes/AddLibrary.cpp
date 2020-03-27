#include "Passes/AddLibrary.h"
#include "Passes/CommandArgs.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>

using namespace llvm;

namespace DashTracer::Passes
{
    MDNode *libName;
    auto AddLibrary::runOnFunction(Function &F) -> bool
    {
        if (!F.empty())
        {
            F.setMetadata("libs", libName);
        }
        return false;
    }

    void AddLibrary::getAnalysisUsage(AnalysisUsage &AU) const
    {
        AU.setPreservesAll();
    }

    auto AddLibrary::doInitialization(Module &M) -> bool
    {
        libName = MDNode::get(M.getContext(), MDString::get(M.getContext(), LibraryName));
        return false;
    }

    char AddLibrary::ID = 0;
    static RegisterPass<AddLibrary> X("AddLibrary", "Labels functions with the library name", true, true);
} // namespace DashTracer::Passes