#include "AtlasUtil/Print.h"
#include "tik/Exceptions.h"
#include "tik/Kernel.h"
#include "tik/LoopGrammars.h"
#include <iostream>
#include <llvm/IR/Instructions.h>
using namespace llvm;
using namespace std;

void Kernel::GetKernelLabels()
{
    //LoopGrammar lg = LoopGrammar::None;
    if (Conditional.size() != 1)
    {
        throw TikException("Expected a single conditoinal");
    }
    for (auto cond : Conditional)
    {

        auto term = cond->getTerminator();
        Value *condition;
        //bool shouldTrue = false; //we should continue if true
        if (auto c = dyn_cast<BranchInst>(term))
        {
            condition = c->getCondition();
            if (c->getSuccessor(1) == Exit)
            {
                //shouldTrue = true;
            }
        }
        else
        {
            throw TikException("Unrecognized conditional terminator");
        }

        if (ICmpInst *cmp = dyn_cast<ICmpInst>(condition))
        {
            auto left = cmp->getOperand(0);
            auto right = cmp->getOperand(1);
            //if either of these are constant we know this is a dynamic limit
            bool leftConst = isa<Constant>(left);
            bool rightConst = isa<Constant>(right);
            assert(!(leftConst && rightConst));
            if (leftConst || rightConst)
            {
                //now is it dynamic or fixed
                Value *nonConst;
                if (leftConst)
                {
                    nonConst = right;
                }
                else
                {
                    nonConst = left;
                }

                if (LoadInst *li = dyn_cast<LoadInst>(nonConst))
                {
                    //this load should be formatted by us so we look for the index
                    //auto call = cast<CallInst>(cast<IntToPtrInst>(li->getPointerOperand())->getOperand(0));
                }
                else
                {
                    throw TikException("Expected a load for non constant value");
                }
                //now we check what the non constant value is
                //lg = LoopGrammar::Fixed;
            }
            else
            {
                //lg = LoopGrammar::Internal;
            }
        }
        else
        {
            //if this is a load to a value we don't write, it is external. Otherwise it is internal
            //not implemented right now
            //lg = LoopGrammar::Internal;
            throw TikException("Unrecognized condition parameter");
        }
    }

    //we now have the loop grammar identified
}