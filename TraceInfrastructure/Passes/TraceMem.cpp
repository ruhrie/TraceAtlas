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

    std::vector<uint64_t> kernelBlockValue;

    bool EncodedTraceMemory::runOnFunction(Function &F)
    {

        for (Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
        {
            auto *block = cast<BasicBlock>(BB);
            auto dl = block->getModule()->getDataLayout();
            int64_t blockId = GetBlockID(block);
            if (std::find(kernelBlockValue.begin(), kernelBlockValue.end(), blockId) != kernelBlockValue.end())
            {
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
                        builder.CreateCall(LoadValue, ref);
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
                        builder.CreateCall(StoreValue, ref);
                    }
                }
            }
        }
        return true;
    }

    bool EncodedTraceMemory::doInitialization(Module &M)
    {
        LoadValue = cast<Function>(M.getOrInsertFunction("LoadValue", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8), Type::getInt8Ty(M.getContext())).getCallee());
        StoreValue = cast<Function>(M.getOrInsertFunction("StoreValue", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8), Type::getInt8Ty(M.getContext())).getCallee());
        LoadDump = cast<Function>(M.getOrInsertFunction("LoadDump", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8)).getCallee());
        StoreDump = cast<Function>(M.getOrInsertFunction("StoreDump", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8)).getCallee());
        kernelBlockValue.clear();
        nlohmann::json j;
        std::ifstream inputStream(KernelFilename);
        inputStream >> j;
        inputStream.close();
        for (auto &[key, value] : j["Kernels"].items())
        {
            string index = key;
            if (stoi(index) == KernelIndex)
            {
                nlohmann::json kernel = value["Blocks"];
                kernelBlockValue = kernel.get<vector<uint64_t>>();
            }
        }

        return false;
    }

    void EncodedTraceMemory::getAnalysisUsage(AnalysisUsage &AU) const
    {
        AU.addRequired<DashTracer::Passes::EncodedAnnotate>();
        AU.addRequired<DashTracer::Passes::TraceMemIO>();
        AU.setPreservesCFG();
    }
    char EncodedTraceMemory::ID = 1;
    static RegisterPass<EncodedTraceMemory> Y("EncodedTraceMemory", "Adds encoded tracing memory value in the kernel to the binary", true, false);
} // namespace DashTracer::Passes