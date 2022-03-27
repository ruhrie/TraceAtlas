// from Josh

#include "Passes/FunctionInliner.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>

using namespace llvm;

namespace DashTracer::Passes
{

    bool FunctionInliner::runOnFunction(Function &F)
    {
        bool isModified = false;

        //outs() << "Processing a new basic block\n";
        // if (BB->size() == 1 || BB->size() == 2) {
        //     BB++;
        //     continue;
        // }

        while (true)
        {
            Instruction *inlineCall;
            bool detectedInline = false;
            for (auto BB = F.begin(); BB != F.end(); BB++)
            {
                for (auto ii = BB->begin(); ii != BB->end(); ii++)
                {
                    Instruction *inst = &*ii;
                    if (auto *CI = dyn_cast<CallInst>(inst))
                    {

                        Function *fun = CI->getCalledFunction();
                        /* The called function can be null in the event of an indirect call https://stackoverflow.com/a/11687221 */
                        if (fun)
                        {
                            if (CI->getCalledFunction()->getName() == "xcorr")
                            {
                                detectedInline = true;
                                inlineCall = CI;
                                break;
                            }
                        }
                    }
                }
            }

            if (!detectedInline)
            {
                isModified = true;
                return isModified;
            }
            else
            {
                errs() << *inlineCall << "inst \n";
                errs() << "Found xcorr\n";
                InlineFunctionInfo ifi = InlineFunctionInfo(NULL);
                errs() << "Found ifi\n"
                       << &ifi;
                auto *call = dyn_cast<CallInst>(inlineCall);
                InlineFunction(call, ifi);
            }
        }

        isModified = true;
        return isModified;
    }
    char FunctionInliner::ID = 0;
    static RegisterPass<FunctionInliner> Z("FunctionInliner", "Looks for BBs containing both a KernExit and KernEnter and splits them into separate blocks", false, false);
} // namespace DashTracer::Passes