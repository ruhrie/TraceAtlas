// from Josh

#include "Passes/SplitKernExitEnter.h"
#include <llvm/Pass.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace DashTracer::Passes {

    bool SplitKernExitEnter::runOnFunction(Function &F) {
        Instruction *exitCall, *entranceCall;
        bool isModified = false;

        for (auto BB = F.begin(); BB != F.end();)
        {
            exitCall = nullptr;
            entranceCall = nullptr;

            //outs() << "Processing a new basic block\n";
            if (BB->size() == 1 || BB->size() == 2) {
                BB++;
                continue;
            }
            for (auto ii = BB->begin(); ii != BB->end(); ii++) {
                Instruction *inst = &*ii;
                if (auto *CI = dyn_cast<CallInst>(inst)) {
                    Function *fun = CI->getCalledFunction();
                    /* The called function can be null in the event of an indirect call https://stackoverflow.com/a/11687221 */
                    if (fun) {
                        if (CI->getCalledFunction()->getName() == "KernelExit") {
                            //outs() << "Found a Kernel Exit\n";
                            exitCall = CI;
                        }
                        if (CI->getCalledFunction()->getName() == "KernelEnter") {
                            //outs() << "Found a Kernel Entrance\n";
                            entranceCall = CI;
                        }
                    }
                }
                if (exitCall != nullptr || entranceCall != nullptr) {
                // if (entranceCall != nullptr) {
                    //outs() << "Splitting a basic block!\n";
                    BasicBlock* newBB = BB->splitBasicBlock(ii);
                    if (newBB->getInstList().size() > 1) {
                        newBB->splitBasicBlock(newBB->front().getNextNode());
                    }
                    isModified = true;
                    break;
                }
            }
            if (exitCall == nullptr && entranceCall == nullptr) {
                // if (entranceCall == nullptr) {
                BB++;
            }
        }

        return isModified;
    }
    char SplitKernExitEnter::ID = 0;
    static RegisterPass<SplitKernExitEnter> Z("SplitKernExitEnter", "Looks for BBs containing both a KernExit and KernEnter and splits them into separate blocks", false, false);
} // namespace DashTracer::Passes