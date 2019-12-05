#include "Passes/Trace.h"
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

using namespace llvm;

namespace DashTracer
{

    namespace Passes
    {
        bool Trace::runOnBasicBlock(BasicBlock &BB)
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
                        auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(BB.getContext()), 0), true);
                        Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(BB.getContext()));
                        values.push_back(cast);
                        ArrayRef<Value *> ref = ArrayRef<Value *>(values);
                        builder.CreateCall(fullAddrFunc, ref);
                    }
                    else if (StoreInst *store = dyn_cast<StoreInst>(CI))
                    {
                        Value *addr = store->getPointerOperand();
                        auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(BB.getContext()), 0), true);
                        Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(BB.getContext()));
                        values.push_back(cast);
                        ArrayRef<Value *> ref = ArrayRef<Value *>(values);
                        builder.CreateCall(fullAddrFunc, ref);
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
        void Trace::getAnalysisUsage(AnalysisUsage &AU) const
        {
            AU.addRequired<DashTracer::Passes::Annotate>();
            AU.addRequired<DashTracer::Passes::TraceIO>();
            AU.setPreservesCFG();
        }

        bool Trace::doInitialization(Module &M)
        {
            fullFunc = cast<Function>(M.getOrInsertFunction("Write", Type::getVoidTy(M.getContext()), Type::getInt8PtrTy(M.getContext()), Type::getInt32Ty(M.getContext()), Type::getInt32Ty(M.getContext()), Type::getInt64Ty(M.getContext())).getCallee());
            fullAddrFunc = cast<Function>(M.getOrInsertFunction("WriteAddress", Type::getVoidTy(M.getContext()), Type::getInt8PtrTy(M.getContext()), Type::getInt32Ty(M.getContext()), Type::getInt32Ty(M.getContext()), Type::getInt64Ty(M.getContext()), Type::getInt8PtrTy(M.getContext())).getCallee());
            return false;
        }
        bool EncodedTrace::runOnBasicBlock(BasicBlock &BB)
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
                        auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(BB.getContext()), 0), true);
                        Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(BB.getContext()));
                        builder.CreateCall(LoadDump, cast);
                        done = true;
                    }
                }
                if (DumpStores && !done)
                {
                    if (StoreInst *store = dyn_cast<StoreInst>(CI))
                    {
                        IRBuilder<> builder(store);
                        Value *addr = store->getPointerOperand();
                        auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(BB.getContext()), 0), true);
                        Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(BB.getContext()));
                        builder.CreateCall(StoreDump, cast);
                        done = true;
                    }
                }
            }

            return true;
        }

        bool EncodedTrace::doInitialization(Module &M)
        {
            BB_ID = cast<Function>(M.getOrInsertFunction("BB_ID_Dump", Type::getVoidTy(M.getContext()), Type::getInt64Ty(M.getContext())).getCallee());
            LoadDump = cast<Function>(M.getOrInsertFunction("LoadDump", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8)).getCallee());
            StoreDump = cast<Function>(M.getOrInsertFunction("StoreDump", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8)).getCallee());
            return false;
        }

        void EncodedTrace::getAnalysisUsage(AnalysisUsage &AU) const
        {
            AU.addRequired<DashTracer::Passes::EncodedAnnotate>();
            AU.addRequired<DashTracer::Passes::TraceIO>();
            AU.setPreservesCFG();
        }

        char Trace::ID = 0;
        char EncodedTrace::ID = 1;
        static RegisterPass<Trace> X("Trace", "Adds tracing to the binary", true, false);
        static RegisterPass<EncodedTrace> Y("EncodedTrace", "Adds encoded tracing to the binary", true, false);
    } // namespace Passes
} // namespace DashTracer