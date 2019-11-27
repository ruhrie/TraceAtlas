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
        bool TraceMem::runOnBasicBlock(BasicBlock &BB)
        {
            for (BasicBlock::iterator BI = BB.begin(), BE = BB.end(); BI != BE; ++BI)
            {
                //start by extracting the UIDs
                bool lineE = false;
                bool blockE = false;
                bool functionE = false;
                Instruction *CI = dyn_cast<Instruction>(BI);
                ConstantInt *line;
                ConstantInt *block;
                ConstantInt *function;
                MDNode *lineMD = CI->getMetadata("Line.UID");
                MDNode *blockMD = CI->getMetadata("Block.UID");
                MDNode *functionMD = CI->getMetadata("Function.UID");
                if (lineMD)
                {
                    lineE = true;
                    line = mdconst::dyn_extract<ConstantInt>(lineMD->getOperand(0));
                }
                if (blockMD)
                {
                    blockE = true;
                    block = mdconst::dyn_extract<ConstantInt>(blockMD->getOperand(0));
                }
                if (functionMD)
                {
                    functionE = true;
                    function = mdconst::dyn_extract<ConstantInt>(functionMD->getOperand(0));
                }
                //convert the instruction to a string
                Instruction *baseInst;
                if (isa<PHINode>(CI) || isa<LandingPadInst>(CI))
                {
                    baseInst = cast<Instruction>(cast<BasicBlock>(BB).getFirstInsertionPt()); //may insert out of order, fortunately not important to us
                }
                else
                {
                    baseInst = CI;
                }
                IRBuilder<> builder(dyn_cast<Instruction>(baseInst));
                std::string str;
                llvm::raw_string_ostream rso(str);
                CI->print(rso);
                Value *strPtr = builder.CreateGlobalStringPtr(str.c_str());
                //prepare the arguments
                if (blockE && lineE && functionE)
                {
                    std::vector<Value *> values;
                    values.push_back(strPtr);
                    values.push_back(line);
                    values.push_back(block);
                    values.push_back(function);
                    //if there is a load or a store also append the address and call fullAddrFunc
                    if (LoadInst *load = dyn_cast<LoadInst>(CI))
                    {
                        Value *addr = load->getPointerOperand();
                        Value *MemValue = load->getOperand(0);
                       
                        // errs()<<"src value:";
                        // errs()<< *addr << '\n';
                        // errs()<<"src type:";
                        // errs()<< *(addr->getType()) << '\n';
                        // errs()<<"dst type PointerType:";
                        // errs()<< *(PointerType::get(Type::getInt8PtrTy(BB.getContext()), 0)) << '\n';
                        // errs()<<"dst type no PointerType:";
                        // errs()<< *(Type::getInt8PtrTy(BB.getContext())) << '\n';

                        auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(BB.getContext()), 0), true);
                        Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(BB.getContext()));
                        values.push_back(cast);
                        // errs()<<"address cast Value:";
                        // errs()<< *cast << '\n';
                        // errs()<<"castAble:";
                        // errs()<< CastInst::isCastable (MemValue->getType(), Type::getInt8Ty(BB.getContext())) << '\n';
                        castCode = CastInst::getCastOpcode(MemValue, true, Type::getInt8Ty(BB.getContext()), true);                      
                        cast = builder.CreateCast(castCode, MemValue, Type::getInt8Ty(BB.getContext()));
                        // errs()<<"MemValue cast Value:";
                        // errs()<< *cast << '\n';
                        values.push_back(cast);

                        ArrayRef<Value *> ref = ArrayRef<Value *>(values);
                        builder.CreateCall(fullAddrValueFunc, ref);
                    }
                    else if (StoreInst *store = dyn_cast<StoreInst>(CI))
                    {
                        Value *addr = store->getPointerOperand();
                        Value *MemValue = store->getOperand(0);
    
                        //  errs()<<"store:";
                        //  errs()<< *store << '\n';
                        //  errs()<<"store addr:";
                        //  errs()<< *addr << '\n';                
                        //  errs()<<"store MemValue:";
                        //  errs()<< *MemValue << '\n';                        

                        auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(BB.getContext()), 0), true);
                        Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(BB.getContext()));
                        values.push_back(cast);

                        castCode = CastInst::getCastOpcode(MemValue, true, Type::getInt8Ty(BB.getContext()), true);                      
                        cast = builder.CreateCast(castCode, MemValue, Type::getInt8Ty(BB.getContext()));
                        //  errs()<<"MemValue cast Value:";
                        //  errs()<< *cast << '\n';
                        values.push_back(cast);


                        ArrayRef<Value *> ref = ArrayRef<Value *>(values);
                        builder.CreateCall(fullAddrValueFunc, ref);
                    }
                    //if it is a return in main we need to call this one instruction earlier due to file io
                    else if (ReturnInst *ret = dyn_cast<ReturnInst>(CI))
                    {
                        if (BB.getParent()->getName() == "main")
                        {
                            builder.SetInsertPoint(builder.GetInsertPoint()->getPrevNode());
                        }
                        ArrayRef<Value *> ref = ArrayRef<Value *>(values);
                        builder.CreateCall(fullFunc, ref);
                    }
                    //otherwise just insert the call
                    else
                    {
                        ArrayRef<Value *> ref = ArrayRef<Value *>(values);
                        builder.CreateCall(fullFunc, ref);
                    }
                }
            }
            return true;
        }
        void TraceMem::getAnalysisUsage(AnalysisUsage &AU) const
        {
            AU.addRequired<DashTracer::Passes::Annotate>();
            AU.addRequired<DashTracer::Passes::TraceMemIO>();
            AU.setPreservesCFG();
        }

        bool TraceMem::doInitialization(Module &M)
        {
            fullFunc = dyn_cast<Function>(M.getOrInsertFunction("Write", Type::getVoidTy(M.getContext()), Type::getInt8PtrTy(M.getContext()), Type::getInt32Ty(M.getContext()), Type::getInt32Ty(M.getContext()), Type::getInt64Ty(M.getContext())));
            fullAddrFunc = dyn_cast<Function>(M.getOrInsertFunction("WriteAddress", Type::getVoidTy(M.getContext()), Type::getInt8PtrTy(M.getContext()), Type::getInt32Ty(M.getContext()), Type::getInt32Ty(M.getContext()), Type::getInt64Ty(M.getContext()), Type::getInt8PtrTy(M.getContext())));
            fullAddrValueFunc = dyn_cast<Function>(M.getOrInsertFunction("WriteAddressValue", Type::getVoidTy(M.getContext()), Type::getInt8PtrTy(M.getContext()), Type::getInt32Ty(M.getContext()), Type::getInt32Ty(M.getContext()), Type::getInt64Ty(M.getContext()), Type::getInt8PtrTy(M.getContext()),Type::getInt8Ty(M.getContext())));
            return false;
        }
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
                if (DumpLoads && !done)
                {
                    if (LoadInst *load = dyn_cast<LoadInst>(CI))
                    {
                        IRBuilder<> builder(load);
                        Value *addr = load->getPointerOperand();
                        Value *MemValue = load->getOperand(0);
                        

                        auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(BB.getContext()), 0), true);
                        Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(BB.getContext()));
                        builder.CreateCall(LoadDump, cast);
                        castCode = CastInst::getCastOpcode(MemValue, true, Type::getInt8Ty(BB.getContext()), true);                      
                        cast = builder.CreateCast(castCode, MemValue, Type::getInt8Ty(BB.getContext()));
                        builder.CreateCall(LoadDumpValue, cast);
                        done = true;
                    }
                }
                if (DumpStores && !done)
                {
                    if (StoreInst *store = dyn_cast<StoreInst>(CI))
                    {
                        IRBuilder<> builder(store);
                        Value *addr = store->getPointerOperand();
                        Value *MemValue = store->getOperand(0);

                        auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(BB.getContext()), 0), true);
                        Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(BB.getContext()));
                        builder.CreateCall(StoreDump, cast);

                        castCode = CastInst::getCastOpcode(MemValue, true, Type::getInt8Ty(BB.getContext()), true);                      
                        cast = builder.CreateCast(castCode, MemValue, Type::getInt8Ty(BB.getContext()));
                        builder.CreateCall(StoreDumpValue, cast);
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
            LoadDumpValue = dyn_cast<Function>(M.getOrInsertFunction("LoadDumpValue", Type::getVoidTy(M.getContext()), Type::getInt8Ty(M.getContext())));
            StoreDump = dyn_cast<Function>(M.getOrInsertFunction("StoreDump", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8)));
            StoreDumpValue = dyn_cast<Function>(M.getOrInsertFunction("StoreDumpValue", Type::getVoidTy(M.getContext()), Type::getInt8Ty(M.getContext())));
            return false;
        }

        void EncodedTraceMem::getAnalysisUsage(AnalysisUsage &AU) const
        {
            AU.addRequired<DashTracer::Passes::EncodedAnnotate>();
            AU.addRequired<DashTracer::Passes::TraceMemIO>();
            AU.setPreservesCFG();
        }

        char TraceMem::ID = 0;
        char EncodedTraceMem::ID = 1;
        static RegisterPass<TraceMem> X("TraceMem", "Adds tracing mem to the binary", true, false);
        static RegisterPass<EncodedTraceMem> Y("EncodedTraceMem", "Adds encoded tracing mem to the binary", true, false);
    } // namespace Passes
} // namespace DashTracer