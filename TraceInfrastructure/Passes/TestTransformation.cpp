#include "Passes/TestTransformation.h"
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
    // source node end bb -> old target start bb, new target start bb
    map<int64_t,pair<int64_t,int64_t>> BBMapingTransform;
    

    bool TestTrans::runOnFunction(Function &F)
    {
        map<int64_t, BasicBlock *> &BBidToPtr = getAnalysis<DashTracer::Passes::EncodedAnnotate>().getIDmap();
        for (Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
        {
            auto *block = cast<BasicBlock>(BB);
            auto dl = block->getModule()->getDataLayout();
            int64_t blockId = GetBlockID(block);
            if (BBMapingTransform.find(blockId)!=BBMapingTransform.end())
            {
                for (BasicBlock::iterator BI = block->begin(), BE = block->end(); BI != BE; ++BI)
                {
                    auto *CI = dyn_cast<Instruction>(BI);
                    if (auto *branch = dyn_cast<BranchInst>(CI))
                    {
                        errs()<<*branch<<"\n";
                        int64_t num = branch->getNumSuccessors();
                        for (unsigned int i = 0;i< num;i++)
                        {
                            auto succ = branch->getSuccessor(i);
                            if(GetBlockID(succ)==BBMapingTransform[blockId].first)
                            {
                                // change successor into the second
                                auto newBB = BBidToPtr[BBMapingTransform[blockId].second];
                                branch->setSuccessor(i,newBB);
                            }
                        }
                        errs()<<*branch<<"\n";
                    }
                }
            }
        }
        return true;
    }

    bool TestTrans::doInitialization(Module &M)
    {  
        BBMapingTransform.clear();
        nlohmann::json j;
        std::ifstream inputStream(DAGBBIOFile);
        inputStream >> j;
        inputStream.close();
        nlohmann::json mapping = j["BBMapingTransform"];
        BBMapingTransform = mapping.get<map <int64_t,pair<int64_t,int64_t>>>();
        return false;
    }

    void TestTrans::getAnalysisUsage(AnalysisUsage &AU) const
    {
        AU.addRequired<DashTracer::Passes::EncodedAnnotate>();
        AU.setPreservesCFG();
    }
    char TestTrans::ID = 1;
    static RegisterPass<TestTrans> Y("TestTrans", "generate test tranformation", true, false);
} // namespace DashTracer::Passes