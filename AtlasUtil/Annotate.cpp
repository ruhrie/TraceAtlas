#include "AtlasUtil/Annotate.h"
using namespace llvm;

void Annotate(llvm::Module *M)
{
    uint64_t index = 0;
    for (auto mi = M->begin(); mi != M->end(); mi++)
    {
        Function *F = cast<Function>(mi);
        Annotate(F, index);
    }
}
void Annotate(llvm::Function *F, uint64_t &startingIndex)
{
    for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
    {
        SetBlockID(cast<BasicBlock>(BB), startingIndex++);
    }
}

int64_t GetBlockID(llvm::BasicBlock *BB)
{
    int result = -1;
    Instruction *first = cast<Instruction>(BB->getFirstInsertionPt());
    if (MDNode *node = first->getMetadata("BlockID"))
    {
        auto ci = cast<ConstantInt>(cast<ConstantAsMetadata>(node->getOperand(0))->getValue());
        result = ci->getSExtValue();
    }
    return result;
}

void SetBlockID(llvm::BasicBlock *BB, uint64_t i)
{
    MDNode *idNode = MDNode::get(BB->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(BB->getContext()), i++)));
    BB->getFirstInsertionPt()->setMetadata("BlockID", idNode);
}