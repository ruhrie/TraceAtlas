#include "Passes/MemProfile.h"
#include "AtlasUtil/Annotate.h"
#include "Passes/Annotate.h"
#include "Passes/CommandArgs.h"
#include "Passes/Functions.h"
#include "llvm/IR/DataLayout.h"
#include <fstream>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using namespace llvm;
using namespace std;

namespace DashTracer::Passes
{


    bool MemProfile::runOnFunction(Function &F)
    {

        for (Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
        {
            auto *block = cast<BasicBlock>(BB);
            auto dl = block->getModule()->getDataLayout();
            int64_t blockId = GetBlockID(block);

            for (BasicBlock::iterator BI = block->begin(), BE = block->end(); BI != BE; ++BI)
            {

                auto *CI = dyn_cast<Instruction>(BI);
                std::vector<Value *> values;
                if (auto *load = dyn_cast<LoadInst>(CI))
                {
                    IRBuilder<> builder(load);
                    Value *addr = load->getPointerOperand();
                    // auto *type = load->getType()->getContainedType(0);
                    uint64_t dataSize = 0; //dl.getTypeAllocSize(type);
                    
                    // addr
                    auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(block->getContext()), 0), true);                    
                    Value *addrCast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(block->getContext()));
                    values.push_back(addrCast);
                    //bb id
                    Value *blockID = ConstantInt::get(Type::getInt64Ty(BB->getContext()), (uint64_t)blockId);
                    values.push_back(blockID);
                    // data size
                    Value *dataSizeValue = ConstantInt::get(Type::getInt64Ty(BB->getContext()), dataSize);
                    values.push_back(dataSizeValue);
                    // type 0 = load, 1 = store
                    uint64_t OPtype = 0;
                    Value *OPtypeValue = ConstantInt::get(Type::getInt64Ty(BB->getContext()), OPtype);
                    values.push_back(OPtypeValue);

                    auto ref = ArrayRef<Value *>(values);
                    builder.CreateCall(MemInstructionDump, ref);
                }

                if (auto *store = dyn_cast<StoreInst>(CI))
                {
                    IRBuilder<> builder(store);
                    Value *addr = store->getPointerOperand();
                    // auto *type = store->getType()->getContainedType(0);
                    uint64_t dataSize = 0; //dl.getTypeAllocSize(type);
                    // addr
                    auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(block->getContext()), 0), true);                    
                    Value *addrCast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(block->getContext()));
                    values.push_back(addrCast);
                    //bb id
                    Value *blockID = ConstantInt::get(Type::getInt64Ty(BB->getContext()), (uint64_t)blockId);
                    values.push_back(blockID);
                    // data size
                    Value *dataSizeValue = ConstantInt::get(Type::getInt64Ty(BB->getContext()), dataSize);
                    values.push_back(dataSizeValue);
                    // type 0 = load, 1 = store
                    uint64_t OPtype = 1;
                    Value *OPtypeValue = ConstantInt::get(Type::getInt64Ty(BB->getContext()), OPtype);
                    values.push_back(OPtypeValue);

                    auto ref = ArrayRef<Value *>(values);
                    builder.CreateCall(MemInstructionDump, ref);
                }
            }
            
        }
        return true;
    }

    bool MemProfile::doInitialization(Module &M)
    {
        MemInstructionDump = cast<Function>(M.getOrInsertFunction("MemInstructionDump", Type::getVoidTy(M.getContext()),Type::getIntNPtrTy(M.getContext(), 8), Type::getInt64Ty(M.getContext()),Type::getInt64Ty(M.getContext()),Type::getInt64Ty(M.getContext())).getCallee());
        return false;
    }

    void MemProfile::getAnalysisUsage(AnalysisUsage &AU) const
    {
        AU.addRequired<DashTracer::Passes::EncodedAnnotate>();

    }
    char MemProfile::ID = 1;
    static RegisterPass<MemProfile> Y("MemProfile", "memory profiler", true, false);
} // namespace DashTracer::Passes