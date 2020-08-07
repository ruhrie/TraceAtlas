#pragma once

#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Module.h>

inline void Split(llvm::Module *M)
{
    for (auto &mi : *M)
    {
        for (auto &fi : mi)
        {
            llvm::BasicBlock *BB = llvm::cast<llvm::BasicBlock>(&fi);
            for (auto &bi : *BB)
            {
                llvm::Instruction *i = llvm::cast<llvm::Instruction>(&bi);
                if (!i->isTerminator())
                {
                    if (auto cb = llvm::dyn_cast<llvm::CallBase>(i))
                    {
                        auto nxt = cb->getNextNode();
                        if (!nxt->isTerminator() || llvm::isa<llvm::InvokeInst>(nxt))
                        {
                            if (!llvm::isa<llvm::DbgInfoIntrinsic>(i)) //skip debug intrinsics
                            {
                                //we should split
                                BB->splitBasicBlock(nxt);
                            }
                        }
                    }
                }
            }
        }
    }
}
