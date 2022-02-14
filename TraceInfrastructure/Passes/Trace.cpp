#include "Passes/Trace.h"
#include "AtlasUtil/Annotate.h"
#include "Passes/Annotate.h"
#include "Passes/CommandArgs.h"
#include "Passes/Functions.h"
#include "Passes/TraceIO.h"
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
    bool EncodedTrace::runOnFunction(Function &F)
    {   
        for (auto fi = F.begin(); fi != F.end(); fi++)
        {
            auto BB = cast<BasicBlock>(fi);
            auto firstInsertion = BB->getFirstInsertionPt();
            auto *firstInst = cast<Instruction>(firstInsertion);
            Value *trueConst = ConstantInt::get(Type::getInt1Ty(BB->getContext()), 1);
            Value *falseConst = ConstantInt::get(Type::getInt1Ty(BB->getContext()), 0);

            IRBuilder<> firstBuilder(firstInst);
            int64_t id = GetBlockID(BB);
            Value *idValue = ConstantInt::get(Type::getInt64Ty(BB->getContext()), (uint64_t)id);
            std::vector<Value *> args;
            args.push_back(idValue);
            args.push_back(trueConst);
            firstBuilder.CreateCall(BB_ID, args);
            args.pop_back();
            args.push_back(falseConst);
            // errs() << "BB-ID:" << id << "\n";
            for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI)
            {
                auto *CI = dyn_cast<Instruction>(BI);
                // errs() << *CI << "\n";
                if (DumpLoads)
                {
                    if (auto *load = dyn_cast<LoadInst>(CI))
                    {
                        IRBuilder<> builder(load);
                        Value *addr = load->getPointerOperand();
                        auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(BB->getContext()), 0), true);
                        Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(BB->getContext()));
                        builder.CreateCall(LoadDump, cast);
                    }
                }
                if (DumpStores)
                {
                    if (auto *store = dyn_cast<StoreInst>(CI))
                    {
                        IRBuilder<> builder(store);
                        Value *addr = store->getPointerOperand();
                        auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(BB->getContext()), 0), true);
                        Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(BB->getContext()));
                        builder.CreateCall(StoreDump, cast);
                    }
                }

                if (MemCpyInst *MCI = dyn_cast<MemCpyInst>(CI))
                {
                    std::vector<Value *> values;
                    // destination
                    Value *op0 = MCI->getOperand(0);
                    //source
                    Value *op1 = MCI->getOperand(1);
                    // len
                    Value *op2 = MCI->getOperand(2); 

                    IRBuilder<> builder(MCI);
                    
                    auto castCode = CastInst::getCastOpcode(op0, true, PointerType::get(Type::getInt8PtrTy(BB->getContext()), 0), true);
                    Value *op0cast = builder.CreateCast(castCode, op0, Type::getInt8PtrTy(BB->getContext()));
                    values.push_back(op0cast);
                    
                    castCode = CastInst::getCastOpcode(op1, true, PointerType::get(Type::getInt8PtrTy(BB->getContext()), 0), true);
                    Value *op1cast = builder.CreateCast(castCode, op1, Type::getInt8PtrTy(BB->getContext()));
                    values.push_back(op1cast);

                    castCode = CastInst::getCastOpcode(op2, true, PointerType::get(Type::getInt8PtrTy(BB->getContext()), 0), true);
                    Value *op2cast = builder.CreateCast(castCode, op2, Type::getInt8PtrTy(BB->getContext()));
                    values.push_back(op2cast);
                    
                    
                    auto ref = ArrayRef<Value *>(values);
                    builder.CreateCall(MemCpyDump, ref);

                    // errs()<<"memcpy"
                    // <<"op0:"<<*op0<<"\n"
                    // <<"op1:"<<*op1<<"\n"
                    // <<"op2:"<<*op2<<"\n";          
                   
                }

                // if (BranchInst  *br = dyn_cast<BranchInst>(CI))
                // {
                    
                //     if(br->isConditional())
                //     {
                //         errs()<<"branch:"<<*br<<"\n";
                //         IRBuilder<> builder(br);
                //         errs()<<"conditional\n";
                //         builder.CreateCall(CondBranch);
                //     }
            
                // }
            }
            Instruction *preTerm = BB->getTerminator();
            IRBuilder endBuilder(preTerm);
            endBuilder.CreateCall(BB_ID, args);
        }
        return true;
    }

    bool EncodedTrace::doInitialization(Module &M)
    {
        BB_ID = cast<Function>(M.getOrInsertFunction("BB_ID_Dump", Type::getVoidTy(M.getContext()), Type::getInt64Ty(M.getContext()), Type::getInt1Ty(M.getContext())).getCallee());
        LoadDump = cast<Function>(M.getOrInsertFunction("LoadDump", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8)).getCallee());
        StoreDump = cast<Function>(M.getOrInsertFunction("StoreDump", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8)).getCallee());
        //input types?
        MemCpyDump = cast<Function>(M.getOrInsertFunction("MemCpyDump", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8),Type::getIntNPtrTy(M.getContext(), 8),Type::getIntNPtrTy(M.getContext(), 8)).getCallee());
        // CondBranch = cast<Function>(M.getOrInsertFunction("CondBranch", Type::getVoidTy(M.getContext())).getCallee());
        return false;
    }

    void EncodedTrace::getAnalysisUsage(AnalysisUsage &AU) const
    {
        AU.addRequired<DashTracer::Passes::EncodedAnnotate>();
        AU.addRequired<DashTracer::Passes::TraceIO>();
        AU.setPreservesCFG();
    }

    char EncodedTrace::ID = 0;
    static RegisterPass<EncodedTrace> Y("EncodedTrace", "Adds encoded tracing to the binary", true, false);
} // namespace DashTracer::Passes