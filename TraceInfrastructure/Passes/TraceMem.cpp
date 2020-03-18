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
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using namespace llvm;
using namespace std;

namespace DashTracer
{

    namespace Passes
    {
        std::vector<uint64_t> kernelBlockValue;

        bool EncodedTraceMemory::runOnFunction(Function &F)
        {

            for (Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
            {
                BasicBlock *block = cast<BasicBlock>(BB);
                auto dl = block->getModule()->getDataLayout();
                int64_t blockId = GetBlockID(block);
                if (std::find(kernelBlockValue.begin(), kernelBlockValue.end(), blockId) != kernelBlockValue.end())
                {
                    for (BasicBlock::iterator BI = block->begin(), BE = block->end(); BI != BE; ++BI)
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
                                Type *tyaddr = addr->getType();
                                Type *tyaddrContain = tyaddr->getContainedType(0);
                                int sizeSig = dl.getTypeAllocSize(tyaddrContain);
                                auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(block->getContext()), 0), true);
                                Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(block->getContext()));
                                values.push_back(cast);
                                ConstantInt *sizeSigVal = ConstantInt::get(llvm::Type::getInt8Ty(block->getContext()), sizeSig);
                                values.push_back(sizeSigVal);
                                ArrayRef<Value *> ref = ArrayRef<Value *>(values);
                                builder.CreateCall(DumpLoadAddrValue, ref);
                                done = true;
                            }

                            if (StoreInst *store = dyn_cast<StoreInst>(CI))
                            {
                                IRBuilder<> builder(store);
                                Value *addr = store->getPointerOperand();
                                Type *tyaddr = addr->getType();
                                Type *tyaddrContain = tyaddr->getContainedType(0);
                                int sizeSig = dl.getTypeAllocSize(tyaddrContain);
                                auto castCode = CastInst::getCastOpcode(addr, true, PointerType::get(Type::getInt8PtrTy(block->getContext()), 0), true);
                                Value *cast = builder.CreateCast(castCode, addr, Type::getInt8PtrTy(block->getContext()));
                                values.push_back(cast);
                                ConstantInt *sizeSigVal = ConstantInt::get(llvm::Type::getInt8Ty(block->getContext()), sizeSig);
                                values.push_back(sizeSigVal);
                                ArrayRef<Value *> ref = ArrayRef<Value *>(values);
                                builder.CreateCall(DumpStoreAddrValue, ref);
                                done = true;
                            }
                        }
                    }
                }
            }
            return true;
        }

        bool EncodedTraceMemory::doInitialization(Module &M)
        {
            DumpLoadAddrValue = cast<Function>(M.getOrInsertFunction("DumpLoadAddrValue", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8), Type::getInt8Ty(M.getContext())).getCallee());
            DumpStoreAddrValue = cast<Function>(M.getOrInsertFunction("DumpStoreAddrValue", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8), Type::getInt8Ty(M.getContext())).getCallee());

            kernelBlockValue.clear();
            nlohmann::json j;
            std::ifstream inputStream(KernelFilename);
            inputStream >> j;
            inputStream.close();
            for (auto &[key, value] : j.items())
            {
                string index = key;
                if (stoi(index) == KernelIndex)
                {
                    nlohmann::json kernel;
                    if (!value[0].empty() && value[0].is_array())
                    {
                        //embedded layout
                        kernel = value[0];
                    }
                    else
                    {
                        kernel = value;
                    }
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
    } // namespace Passes
} // namespace DashTracer