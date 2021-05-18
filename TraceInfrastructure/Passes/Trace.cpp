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
    cl::opt<bool> SkipAnnotation("sa", llvm::cl::desc("Skip annotation pass"), llvm::cl::value_desc("Skip the annotation pass due to a more complex build flow"));
    bool EncodedTrace::runOnFunction(Function &F)
    {
        for (auto fi = F.begin(); fi != F.end(); fi++)
        {
            auto *BB = cast<BasicBlock>(fi);
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
            for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI)
            {
                auto *CI = dyn_cast<Instruction>(BI);
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
            }
            Instruction *preTerm = BB->getTerminator();
            IRBuilder<> endBuilder(preTerm);
            endBuilder.CreateCall(BB_ID, args);
        }
        return true;
    }

    bool EncodedTrace::doInitialization(Module &M)
    {
        BB_ID = cast<Function>(M.getOrInsertFunction("TraceAtlasBB_ID_Dump", Type::getVoidTy(M.getContext()), Type::getInt64Ty(M.getContext()), Type::getInt1Ty(M.getContext())).getCallee());
        LoadDump = cast<Function>(M.getOrInsertFunction("TraceAtlasLoadDump", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8)).getCallee());
        StoreDump = cast<Function>(M.getOrInsertFunction("TraceAtlasStoreDump", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8)).getCallee());
        return false;
    }

    void EncodedTrace::getAnalysisUsage(AnalysisUsage &AU) const
    {
        if (!SkipAnnotation)
        {
            AU.addRequired<DashTracer::Passes::EncodedAnnotate>();
        }
        AU.addRequired<DashTracer::Passes::TraceIO>();
    }

    char EncodedTrace::ID = 0;
    static RegisterPass<EncodedTrace> Y("EncodedTrace", "Adds encoded tracing to the binary", true, false);
} // namespace DashTracer::Passes