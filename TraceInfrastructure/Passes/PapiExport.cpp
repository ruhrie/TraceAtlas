#include "Passes/PapiExport.h"
#include "AtlasUtil/Annotate.h"
#include "Passes/Annotate.h"
#include "Passes/CommandArgs.h"
#include "Passes/PapiIO.h"
#include <fstream>
#include <iostream>
#include <llvm/IR/CFG.h>
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

    std::vector<uint64_t> kernelBlock;
    Function *certOn;
    Function *certOff;

    auto PapiExport::runOnFunction(Function &F) -> bool
    {
        std::string functionName = F.getName();
        for (Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
        {
            auto *block = cast<BasicBlock>(BB);
            std::vector<Instruction *> toRemove;
            auto blockId = GetBlockID(block);
            bool local = std::find(kernelBlock.begin(), kernelBlock.end(), blockId) != kernelBlock.end();
            bool contInt = false;
            bool contExt = false;
            for (BasicBlock *pred : predecessors(block))
            {
                auto subId = GetBlockID(pred);
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
                if (local && contExt)
                {
                    borderBuilder.CreateCall(certOn);
                }
                else if (!local && contInt)
                {
                    borderBuilder.CreateCall(certOff);
                }
            }

            for (BasicBlock::iterator BI = block->begin(), BE = block->end(); BI != BE; ++BI)
            {
                if (auto *callInst = dyn_cast<CallInst>(BI))
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
                            assert(certOff != nullptr);
                            callBuilder.CreateCall(certOff);
                            BI++;
                        }
                    }
                }
            }
        }
        return true;
    } // namespace Passes

    auto PapiExport::doInitialization(Module &M) -> bool
    {
        certOn = cast<Function>(M.getOrInsertFunction("CertifyPapiOn", Type::getVoidTy(M.getContext())).getCallee());
        certOff = cast<Function>(M.getOrInsertFunction("CertifyPapiOff", Type::getVoidTy(M.getContext())).getCallee());

        kernelBlock.clear();
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
                kernelBlock = kernel.get<vector<uint64_t>>();
            }
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
} // namespace DashTracer::Passes