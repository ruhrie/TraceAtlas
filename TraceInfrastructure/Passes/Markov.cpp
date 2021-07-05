#include "Passes/Markov.h"
#include "AtlasUtil/Annotate.h"
#include "Passes/Annotate.h"
#include "Passes/CommandArgs.h"
#include "Passes/Functions.h"
#include "Passes/MarkovIO.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>
#include <string>
#include <vector>

using namespace llvm;

namespace DashTracer::Passes
{
    bool Markov::runOnFunction(Function &F)
    {
        for (auto fi = F.begin(); fi != F.end(); fi++)
        {
            auto *BB = cast<BasicBlock>(fi);
            auto firstInsertion = BB->getFirstInsertionPt();
            auto *firstInst = cast<Instruction>(firstInsertion);
            auto lastInsertion = BB->getTerminator();
            IRBuilder<> firstBuilder(firstInst);

            // look for call insts in the block. After each callinst, we need to inject a call to MarkovIncrement to record the returnInst in the graph
            // do this before we inject our own CallBases
            std::vector<CallBase *> blockCalls;
            for (auto bi = BB->begin(); bi != BB->end(); bi++)
            {
                if (auto CI = dyn_cast<CallBase>(bi))
                {
                    blockCalls.push_back(CI);
                }
            }

            // skip this if we are in the first block of main
            if (!((F.getName() == "main") && (fi == F.begin())))
            {
                int64_t id = GetBlockID(BB);
                Value *idValue = ConstantInt::get(Type::getInt64Ty(BB->getContext()), (uint64_t)id);
                std::vector<Value *> args;
                args.push_back(idValue);
                auto call = firstBuilder.CreateCall(MarkovIncrement, args);
                call->setDebugLoc(NULL);
            }

            auto exitCall = firstBuilder.CreateCall(MarkovExit);
            exitCall->moveBefore(lastInsertion);
            exitCall->setDebugLoc(NULL);

            // two things to check
            // first, see if this is the first block of main, and if it is, insert MarkovInit to the first instruction of the block
            // second, see if there is a return from main here, and if it is, insert MarkovDestroy and move it right before the ReturnInst
            if (F.getName() == "main")
            {
                // MarkovInit
                if (fi == F.begin())
                {
                    std::vector<Value *> args;
                    // get blockCount and make it a value in the LLVM Module
                    firstInsertion = BB->getFirstInsertionPt();
                    firstInst = cast<Instruction>(firstInsertion);
                    IRBuilder<> initBuilder(firstInst);
                    uint64_t blockCount = GetBlockCount(F.getParent());
                    Value *countValue = ConstantInt::get(Type::getInt64Ty(BB->getContext()), blockCount);
                    args.push_back(countValue);
                    // get the BBID and make it a value in the LLVM Module
                    int64_t id = GetBlockID(BB);
                    Value *blockID = ConstantInt::get(Type::getInt64Ty(BB->getContext()), (uint64_t)id);
                    args.push_back(blockID);
                    auto call = initBuilder.CreateCall(MarkovInit, args);
                    call->setDebugLoc(NULL);
                }
                // MarkovDestroy
                // Place this before any return from main
                else if (auto retInst = dyn_cast<ReturnInst>(fi->getTerminator()))
                {
                    auto endInsertion = BB->getTerminator();
                    auto *lastInst = cast<Instruction>(endInsertion);
                    IRBuilder<> lastBuilder(lastInst);

                    auto call = lastBuilder.CreateCall(MarkovDestroy);
                    call->moveBefore(lastInst);
                    call->setDebugLoc(NULL);
                }
                else if (auto resumeInst = dyn_cast<ResumeInst>(fi->getTerminator()))
                {
                    auto endInsertion = BB->getTerminator();
                    auto *lastInst = cast<Instruction>(endInsertion);
                    IRBuilder<> lastBuilder(lastInst);

                    auto call = lastBuilder.CreateCall(MarkovDestroy);
                    call->moveBefore(lastInst);
                    call->setDebugLoc(NULL);
                }
                else if (auto unreachableInst = dyn_cast<UnreachableInst>(fi->getTerminator()))
                {
                    auto endInsertion = BB->getTerminator();
                    auto *lastInst = cast<Instruction>(endInsertion);
                    IRBuilder<> lastBuilder(lastInst);

                    auto call = lastBuilder.CreateCall(MarkovDestroy);
                    call->moveBefore(lastInst);
                    call->setDebugLoc(NULL);
                }
            }
            // we also have to look for the exit() function from libc
            for (auto bi = fi->begin(); bi != fi->end(); bi++)
            {
                if (auto call = dyn_cast<CallBase>(bi))
                {
                    if (call->getCalledFunction())
                    {
                        if (call->getCalledFunction()->getName() == "exit")
                        {
                            IRBuilder<> destroyInserter(call);
                            auto insert = destroyInserter.CreateCall(MarkovDestroy);
                            insert->moveBefore(call);
                            call->setDebugLoc(NULL);
                        }
                    }
                }
            }
        }
        return true;
    }

    bool Markov::doInitialization(Module &M)
    {
        MarkovInit = cast<Function>(M.getOrInsertFunction("MarkovInit", Type::getVoidTy(M.getContext()), Type::getInt64Ty(M.getContext()), Type::getInt64Ty(M.getContext())).getCallee());
        MarkovDestroy = cast<Function>(M.getOrInsertFunction("MarkovDestroy", Type::getVoidTy(M.getContext())).getCallee());
        MarkovIncrement = cast<Function>(M.getOrInsertFunction("MarkovIncrement", Type::getVoidTy(M.getContext()), Type::getInt64Ty(M.getContext())).getCallee());
        MarkovExit = cast<Function>(M.getOrInsertFunction("MarkovExit", Type::getVoidTy(M.getContext())).getCallee());
        return false;
    }

    void Markov::getAnalysisUsage(AnalysisUsage &AU) const
    {
        AU.addRequired<DashTracer::Passes::EncodedAnnotate>();
        AU.addRequired<DashTracer::Passes::MarkovIO>();
    }

    char Markov::ID = 0;
    static RegisterPass<Markov> Y("Markov", "Adds Markov Dumping to the binary", true, false);
} // namespace DashTracer::Passes