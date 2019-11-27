#include "Passes/NearKernelExport.h"
#include "Passes/Annotate.h"
#include "Passes/CommandArgs.h"
#include "Passes/PapiIO.h"
#include "Passes/TraceMemIO.h"
#include "Passes/Functions.h"
#include "llvm/IR/CFG.h"
#include <fstream>
#include <iostream>
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

using namespace llvm;

namespace DashTracer
{

    namespace Passes
    {

        std::vector<uint64_t> kernelBlockForValue;

        bool NearKernelExport::runOnFunction(Function &F)
        {
            std::string functionName = F.getName();
            for (Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
            {
                BasicBlock *block = cast<BasicBlock>(BB);
                std::string blockName = block->getName();
                
                uint64_t blockId = std::stoul(blockName.substr(7));
                bool local = std::find(kernelBlockForValue.begin(), kernelBlockForValue.end(), blockId) != kernelBlockForValue.end();
                bool contInt = false;
                bool contExt = false;
                for (BasicBlock *pred : predecessors(block))
                {
                    std::string subName = pred->getName();
                    uint64_t subId = std::stoul(subName.substr(7));
                    if (std::find(kernelBlockForValue.begin(), kernelBlockForValue.end(), subId) != kernelBlockForValue.end())
                    {
                        contInt = true;
                    }
                    else
                    {
                        contExt = true;
                    }
                }

                IRBuilder<> borderBuilder(block);
                Instruction *nonPhi = block->getFirstNonPHI();
                while (Instruction *lp = dyn_cast<LandingPadInst>(nonPhi))
                {
                    nonPhi = lp->getNextNode();
                }
                borderBuilder.SetInsertPoint(nonPhi);

                if (&F.getEntryBlock() == block)
                {
                    if (local)
                    {
                        for (BasicBlock::iterator BI = block->begin(), BE = block->end(); BI != BE; ++BI)
                        {
                            bool done = false;
                            Instruction *CI = dyn_cast<Instruction>(BI);
                            if (LoadInst *load = dyn_cast<LoadInst>(CI))
                            {
                                IRBuilder<> builder(load);
                                Value *MemValue = load->getOperand(0);                       
                                auto castCode = CastInst::getCastOpcode(MemValue, true, Type::getInt8Ty(block->getContext()), true);                      
                                Value *cast = builder.CreateCast(castCode, MemValue, Type::getInt8Ty(block->getContext()));
                                builder.CreateCall(LoadDumpValue, cast);
                                done = true;
                            }
                            if (StoreInst *store = dyn_cast<StoreInst>(CI))
                            {
                                IRBuilder<> builder(store);
                                Value *MemValue = store->getOperand(0);
                                auto castCode = CastInst::getCastOpcode(MemValue, true, Type::getInt8Ty(block->getContext()), true);                      
                                Value *cast = builder.CreateCast(castCode, MemValue, Type::getInt8Ty(block->getContext()));
                                builder.CreateCall(StoreDumpValue, cast);
                                done = true;
                            }
                            
                        }
                        
                    }
                }
                else
                {
                    if (local & contExt)
                    {
                        for (BasicBlock::iterator BI = block->begin(), BE = block->end(); BI != BE; ++BI)
                        {
                            bool done = false;
                            Instruction *CI = dyn_cast<Instruction>(BI);
                            if (LoadInst *load = dyn_cast<LoadInst>(CI))
                            {
                                IRBuilder<> builder(load);
                                Value *MemValue = load->getOperand(0);                       
                                auto castCode = CastInst::getCastOpcode(MemValue, true, Type::getInt8Ty(block->getContext()), true);                      
                                Value *cast = builder.CreateCast(castCode, MemValue, Type::getInt8Ty(block->getContext()));
                                builder.CreateCall(LoadDumpValue, cast);
                                done = true;
                            }
                            if (StoreInst *store = dyn_cast<StoreInst>(CI))
                            {
                                IRBuilder<> builder(store);
                                Value *MemValue = store->getOperand(0);
                                auto castCode = CastInst::getCastOpcode(MemValue, true, Type::getInt8Ty(block->getContext()), true);                      
                                Value *cast = builder.CreateCast(castCode, MemValue, Type::getInt8Ty(block->getContext()));
                                builder.CreateCall(StoreDumpValue, cast);
                                done = true;
                            }
                        }
                    
                    }
                }                
            }
            return true;
        } // namespace Passes

        bool NearKernelExport::doInitialization(Module &M)
        {
            LoadDumpValue = dyn_cast<Function>(M.getOrInsertFunction("LoadDumpValue", Type::getVoidTy(M.getContext()), Type::getInt8Ty(M.getContext())));
            StoreDumpValue = dyn_cast<Function>(M.getOrInsertFunction("StoreDumpValue", Type::getVoidTy(M.getContext()), Type::getInt8Ty(M.getContext())));
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

        void NearKernelExport::getAnalysisUsage(AnalysisUsage &AU) const
        {
            AU.addRequired<DashTracer::Passes::EncodedAnnotate>();
            AU.addRequired<DashTracer::Passes::TraceMemIO>();
        }

        char NearKernelExport::ID = 0;
        static RegisterPass<NearKernelExport> Y("NearKernelExport", "Adds memory value saving near kernel boundry calls to the binary", false, false);
    } // namespace Passes
} // namespace DashTracer