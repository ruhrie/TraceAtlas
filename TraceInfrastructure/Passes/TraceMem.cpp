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
#include "llvm/IR/DataLayout.h"
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace llvm;

namespace DashTracer
{

    namespace Passes
    {
        std::vector<uint64_t> kernelBlockForValue;
        
        bool EncodedTraceMem::runOnFunction(Function &F)
        {
            
            for (Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
            {
                BasicBlock *block = cast<BasicBlock>(BB);
                std::string blockName = block->getName();
                auto dl = block-> getModule()->getDataLayout();
                uint64_t blockId = std::stoul(blockName.substr(7));
                if (std::find(kernelBlockForValue.begin(), kernelBlockForValue.end(), blockId) != kernelBlockForValue.end())
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
                                builder.CreateCall(DumpLoadValue, ref);
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
                                builder.CreateCall(DumpStoreValue, ref);
                                done = true;
                            }
                        }
                    }
                }
            }
            return true;
        }

        bool EncodedTraceMem::doInitialization(Module &M)
        {   
            DumpLoadValue = dyn_cast<Function>(M.getOrInsertFunction("DumpLoadValue", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8),Type::getInt8Ty(M.getContext()) ));
            DumpStoreValue = dyn_cast<Function>(M.getOrInsertFunction("DumpStoreValue", Type::getVoidTy(M.getContext()), Type::getIntNPtrTy(M.getContext(), 8),Type::getInt8Ty(M.getContext()) ));
            
            kernelBlockForValue.clear();
            std::ifstream inputStream(KernelFilename);
            if (inputStream.is_open())
            {
                std::string data = "";
                std::string line;
                while (std::getline(inputStream, line))
                {
                    data += line;
                }
                data.erase(std::remove(data.begin(), data.end(), '\n'), data.end());
                data.erase(std::remove(data.begin(), data.end(), ' '), data.end());
                data.erase(std::remove(data.begin(), data.end(), '\t'), data.end());
                std::vector<std::string> kernels;
                std::string temp = "";
                for (int i = 1; i < data.length() - 1; i++)
                {
                    if (data[i - 1] == ']' && data[i] == ',')
                    {
                        kernels.push_back(temp);
                        temp = "";
                    }
                    else
                    {
                        temp += data[i];
                    }
                }
                kernels.push_back(temp);
                bool found = false;
                for (int i = 0; i < kernels.size(); i++)
                {
                    if (found)
                    {
                        break;
                    }
                    std::string kern = kernels[i];
                    for (int i = 1; i < kern.length(); i++)
                    {
                        if (kern[i] == '"')
                        {
                            uint64_t index = std::stoul(kern.substr(1, i - 1));
                            if (index == KernelIndex)
                            {
                                std::string kernString = kern.substr(i + 3, kern.length() - i - 4);
                                std::string intString = "";
                                for (int j = 0; j < kernString.length(); j++)
                                {
                                    if (kernString[j] == ',')
                                    {
                                        uint64_t resultInt;
                                        if (intString.rfind("0X", 0) == 0 || intString.rfind("0x", 0) == 0)
                                        {
                                            resultInt = std::stoul(intString.substr(1, intString.size() - 2), nullptr, 16);
                                        }
                                        else
                                        {
                                            resultInt = std::stoul(intString);
                                        }

                                        kernelBlockForValue.push_back(resultInt);
                                        intString = "";
                                    }
                                    else
                                    {
                                        intString += kernString[j];
                                    }
                                }
                                if (intString.length() != 0)
                                {
                                    uint64_t resultInt;
                                    if (intString.rfind("0X", 0) == 0 || intString.rfind("0x", 0) == 0)
                                    {
                                        resultInt = std::stoul(intString.substr(1, intString.size() - 2), nullptr, 16);
                                    }
                                    else
                                    {
                                        resultInt = std::stoul(intString);
                                    }
                                    kernelBlockForValue.push_back(resultInt);
                                }
                                found = true;
                            }
                            break;
                        }
                    }
                }

                inputStream.close();
            }

            else
            {
                std::cout << "Failed to open kernel file. Will not trace events\n";
            }

            return false;
        }

        void EncodedTraceMem::getAnalysisUsage(AnalysisUsage &AU) const
        {
            AU.addRequired<DashTracer::Passes::EncodedAnnotate>();
            AU.addRequired<DashTracer::Passes::TraceMemIO>();
            AU.setPreservesCFG();
        }
        char EncodedTraceMem::ID = 1;
        static RegisterPass<EncodedTraceMem> Y("EncodedTraceMem", "Adds encoded tracing memory value in the kernel to the binary", true, false);
    } // namespace Passes
} // namespace DashTracer