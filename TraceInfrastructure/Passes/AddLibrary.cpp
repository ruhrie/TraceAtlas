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
    bool AddLibrary::runOnFunction(Function &F)
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

    bool AddLibrary::doInitialization(Module &M)
    {
        libName = MDNode::get(M.getContext(), MDString::get(M.getContext(), LibraryName));
        return false;
    }

    char AddLibrary::ID = 0;
    static RegisterPass<AddLibrary> X("AddLibrary", "Labels functions with the library name", true, true);
} // namespace DashTracer::Passes