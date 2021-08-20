#include "Passes/TraceMem.h"
#include "AtlasUtil/Annotate.h"
#include "Passes/Annotate.h"
#include "Passes/CommandArgs.h"
#include "Passes/Functions.h"
#include "Passes/TraceMemIO.h"
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
                    Type *tyaddr = addr->getType();
                    Type *tyaddrContain = tyaddr->getContainedType(0);
                    uint64_t sizeSig = dl.getTypeAllocSize(tyaddrContain);
                    auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(block->getContext()), 0), true);
                    Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(block->getContext()));
                    builder.CreateCall(LoadDump, cast);
                    values.push_back(cast);
                    ConstantInt *sizeSigVal = ConstantInt::get(llvm::Type::getInt8Ty(block->getContext()), sizeSig);
                    values.push_back(sizeSigVal);
                    auto ref = ArrayRef<Value *>(values);
                    builder.CreateCall(DumpLoadValue, ref);
                }

                if (auto *store = dyn_cast<StoreInst>(CI))
                {
                    IRBuilder<> builder(store);
                    Value *addr = store->getPointerOperand();
                    Type *tyaddr = addr->getType();
                    Type *tyaddrContain = tyaddr->getContainedType(0);
                    uint64_t sizeSig = dl.getTypeAllocSize(tyaddrContain);
                    auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(block->getContext()), 0), true);
                    Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(block->getContext()));
                    builder.CreateCall(StoreDump, cast);
                    values.push_back(cast);
                    ConstantInt *sizeSigVal = ConstantInt::get(llvm::Type::getInt8Ty(block->getContext()), sizeSig);
                    values.push_back(sizeSigVal);
                    auto ref = ArrayRef<Value *>(values);
                    builder.CreateCall(DumpStoreValue, ref);
                }
            }
            
        }
        return true;
    }

    bool MemProfile::doInitialization(Module &M)
    {
        DumpLoadValue = cast<Function>(M.getOrInsertFunction("TraceAtlasDumpLoadValue", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8), Type::getInt8Ty(M.getContext())).getCallee());
        DumpStoreValue = cast<Function>(M.getOrInsertFunction("TraceAtlasDumpStoreValue", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8), Type::getInt8Ty(M.getContext())).getCallee());
        LoadDump = cast<Function>(M.getOrInsertFunction("TraceAtlasLoadDump", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8)).getCallee());
        StoreDump = cast<Function>(M.getOrInsertFunction("TraceAtlasStoreDump", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8)).getCallee());
        return false;
    }

    void MemProfile::getAnalysisUsage(AnalysisUsage &AU) const
    {
        AU.addRequired<DashTracer::Passes::EncodedAnnotate>();
        AU.addRequired<DashTracer::Passes::MemProfileIO>();
    }
    char MemProfile::ID = 1;
    static RegisterPass<MemProfile> Y("MemProfile", "memory profiler", true, false);
} // namespace DashTracer::Passes