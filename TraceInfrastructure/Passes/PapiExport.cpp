#include "Passes/PapiExport.h"
#include "Passes/Annotate.h"
#include "Passes/CommandArgs.h"
#include "Passes/PapiIO.h"
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

        std::vector<uint64_t> kernelBlock;
        Function *certOn;
        Function *certOff;

        bool PapiExport::runOnFunction(Function &F)
        {
            std::string functionName = F.getName();
            for (Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
            {
                BasicBlock *block = cast<BasicBlock>(BB);
                std::vector<Instruction *> toRemove;
                std::string blockName = block->getName();
                if (blockName.rfind("papi_", 0) == 0)
                {
                    break;
                }
                uint64_t blockId = std::stoul(blockName.substr(7));
                bool local = std::find(kernelBlock.begin(), kernelBlock.end(), blockId) != kernelBlock.end();
                bool contInt = false;
                bool contExt = false;
                for (BasicBlock *pred : predecessors(block))
                {
                    std::string subName = pred->getName();
                    uint64_t subId = std::stoul(subName.substr(7));
                    if (std::find(kernelBlock.begin(), kernelBlock.end(), subId) != kernelBlock.end())
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
                        borderBuilder.CreateCall(certOn);
                    }
                    else
                    {
                        borderBuilder.CreateCall(certOff);
                    }
                }
                else
                {
                    if (local & contExt)
                    {
                        borderBuilder.CreateCall(certOn);
                    }
                    else if (!local & contInt)
                    {
                        borderBuilder.CreateCall(certOff);
                    }
                }

                for (BasicBlock::iterator BI = block->begin(), BE = block->end(); BI != BE; ++BI)
                {
                    if (CallInst *callInst = dyn_cast<CallInst>(BI))
                    {
                        Function *calledFunc = callInst->getCalledFunction();
                        if (calledFunc != certOn && calledFunc != certOff)
                        {
                            IRBuilder<> callBuilder(callInst);
                            if (local)
                            {
                                callBuilder.CreateCall(certOn);
                                BI++;
                            }
                            else
                            {
                                callBuilder.CreateCall(certOff);
                                BI++;
                            }
                        }
                    }
                }
            }
            return true;
        } // namespace Passes

        bool PapiExport::doInitialization(Module &M)
        {
            certOn = dyn_cast<Function>(M.getOrInsertFunction("CertifyPapiOn", Type::getVoidTy(M.getContext())));
            certOff = dyn_cast<Function>(M.getOrInsertFunction("CertifyPapiOff", Type::getVoidTy(M.getContext())));

            kernelBlock.clear();
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

                                        kernelBlock.push_back(resultInt);
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
                                    kernelBlock.push_back(resultInt);
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

        void PapiExport::getAnalysisUsage(AnalysisUsage &AU) const
        {
            AU.addRequired<DashTracer::Passes::EncodedAnnotate>();
            AU.addRequired<DashTracer::Passes::PapiIO>();
        }

        char PapiExport::ID = 0;
        static RegisterPass<PapiExport> Y("PapiExport", "Adds papi instrumentation calls to the binary", false, false);
    } // namespace Passes
} // namespace DashTracer