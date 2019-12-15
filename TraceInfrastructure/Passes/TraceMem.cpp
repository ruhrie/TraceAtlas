#include "Passes/TraceMem.h"
#include "Passes/Annotate.h"
#include "Passes/CommandArgs.h"
#include "Passes/Functions.h"
#include "Passes/TraceMemIO.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>
#include <string>

using namespace llvm;

namespace DashTracer
{

    namespace Passes
    {
        bool EncodedTraceMem::runOnBasicBlock(BasicBlock &BB)
        {
            auto firstInsertion = BB.getFirstInsertionPt();
            Instruction *firstInst = cast<Instruction>(firstInsertion);

            IRBuilder<> firstBuilder(firstInst);
            std::string name = BB.getName();
            uint64_t id = std::stoul(name.substr(7));
            Value *idValue = ConstantInt::get(Type::getInt64Ty(BB.getContext()), id);
            firstBuilder.CreateCall(BB_ID, idValue);
            
            for (BasicBlock::iterator BI = BB.begin(), BE = BB.end(); BI != BE; ++BI)
            {
                bool done = false;
                Instruction *CI = dyn_cast<Instruction>(BI);
                if (!done)
                {
                    std::vector<Value *> values;
                    if (LoadInst *load = dyn_cast<LoadInst>(CI))
                    {
                        IRBuilder<> builder(load);
                        Value *addr = load->getPointerOperand();
                        Value *MemValue = load->getOperand(0);
                        
                        Type *tyaddr = addr->getType();                       
                        Type *tyaddrContain = tyaddr->getContainedType(0);                       
                        int tyid =  tyaddrContain->getTypeID();
                        int sizeSig = tyid;
                        if(tyaddr ->isIntegerTy())
                        {
                            int bit =  cast<IntegerType>(tyaddr)->getBitWidth();
                            sizeSig = bit;
                        }
                        
                        auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(BB.getContext()), 0), true);
                        Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(BB.getContext()));
                        values.push_back(cast);

                        ConstantInt *sizeSigVal = ConstantInt::get(llvm::Type::getInt8Ty(BB.getContext()), sizeSig);
                        values.push_back(sizeSigVal);
                        ArrayRef<Value *> ref = ArrayRef<Value *>(values);    
                        builder.CreateCall(LoadDumpValue, ref);
                        done = true;
                    }

                    if (StoreInst *store = dyn_cast<StoreInst>(CI))
                    {
                        IRBuilder<> builder(store);
                        Value *addr = store->getPointerOperand();
                        Value *MemValue = store->getOperand(0);

                        Type *tyaddr = addr->getType();                       
                        Type *tyaddrContain = tyaddr->getContainedType(0);                       
                        int tyid =  tyaddrContain->getTypeID();
                        int sizeSig = tyid;
                        if(tyaddr ->isIntegerTy())
                        {
                            int bit =  cast<IntegerType>(tyaddr)->getBitWidth();
                            sizeSig = bit;
                        }
                        
                        auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(BB.getContext()), 0), true);
                        Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(BB.getContext()));
                        values.push_back(cast);

                        ConstantInt *sizeSigVal = ConstantInt::get(llvm::Type::getInt8Ty(BB.getContext()), sizeSig);
                        values.push_back(sizeSigVal);
                        ArrayRef<Value *> ref = ArrayRef<Value *>(values);    
                        builder.CreateCall(StoreDumpValue, ref);
                        done = true;
                    }
                }
            }

            return true;
        }

        bool EncodedTraceMem::doInitialization(Module &M)
        {
            BB_ID = dyn_cast<Function>(M.getOrInsertFunction("BB_ID_Dump", Type::getVoidTy(M.getContext()), Type::getInt64Ty(M.getContext())));
            LoadDump = dyn_cast<Function>(M.getOrInsertFunction("LoadDump", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8)));
            LoadDumpValue = dyn_cast<Function>(M.getOrInsertFunction("LoadDumpValue", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8),Type::getInt8Ty(M.getContext()) ));
            StoreDump = dyn_cast<Function>(M.getOrInsertFunction("StoreDump", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8)));
            StoreDumpValue = dyn_cast<Function>(M.getOrInsertFunction("StoreDumpValue", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8),Type::getInt8Ty(M.getContext()) ));
            
            return false;
        }

        void EncodedTraceMem::getAnalysisUsage(AnalysisUsage &AU) const
        {
            AU.addRequired<DashTracer::Passes::EncodedAnnotate>();
            AU.addRequired<DashTracer::Passes::TraceMemIO>();
            AU.setPreservesCFG();
        }
        char EncodedTraceMem::ID = 1;
        static RegisterPass<EncodedTraceMem> Y("EncodedTraceMem", "Adds encoded tracing memory value to the binary", true, false);
    } // namespace Passes
} // namespace DashTracer