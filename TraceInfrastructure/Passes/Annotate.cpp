#include "Passes/Annotate.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace DashTracer
{
    void Annotate(Function *F)
    {
        int basicBlock = 0;
        int line = 0;
        uint64_t function = F->getGUID();
        MDNode *P = MDNode::get(F->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt64Ty(F->getContext()), function)));
        for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
        {
            MDNode *N = MDNode::get(F->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt32Ty(F->getContext()), basicBlock)));
            for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI)
            {
                Instruction *CI = dyn_cast<Instruction>(BI);
                MDNode *O = MDNode::get(F->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt32Ty(F->getContext()), line)));
                CI->setMetadata("Block.UID", N);
                CI->setMetadata("Line.UID", O);
                CI->setMetadata("Function.UID", P);
                line++;
            }
            basicBlock++;
        }
    }

    void Annotate(Module *M)
    {
        int function = 0;
        int basicBlock = 0;
        int line = 0;
        MDNode *P = MDNode::get(M->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt64Ty(M->getContext()), function)));
        for (Module::iterator F = M->begin(), E = M->end(); F != E; ++F)
        {
            for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
            {
                MDNode *N = MDNode::get(M->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt32Ty(M->getContext()), basicBlock)));
                for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI)
                {
                    Instruction *CI = dyn_cast<Instruction>(BI);
                    MDNode *O = MDNode::get(M->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt32Ty(M->getContext()), line)));
                    CI->setMetadata("Block.UID", N);
                    CI->setMetadata("Line.UID", O);
                    CI->setMetadata("Function.UID", P);
                    line++;
                }
                basicBlock++;
            }
            function++;
        }
    }

    namespace Passes
    {
        static uint64_t UID = 0;
        bool Annotate::runOnFunction(Function &F)
        {
            DashTracer::Annotate(&F);
            return false;
        }
        void Annotate::getAnalysisUsage(AnalysisUsage &AU) const
        {
            AU.setPreservesAll();
        }

        struct MAnnotate : public ModulePass
        {
            static char ID;
            MAnnotate() : ModulePass(ID) {}

            bool runOnModule(Module &M) override
            {
                DashTracer::Annotate(&M);
                return false;
            }
            void getAnalysisUsage(AnalysisUsage &AU) const override
            {
                AU.setPreservesAll();
            }
        };

        bool EncodedAnnotate::runOnModule(Module &M)
        {
            for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F)
            {
                for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
                {
                    BB->setName("BB_UID_" + std::to_string(UID++));
                }
            }
            return true;
        }

        void EncodedAnnotate::getAnalysisUsage(AnalysisUsage &AU) const
        {
            AU.setPreservesAll();
        }

        char Annotate::ID = 0;
        char MAnnotate::ID = 1;
        char EncodedAnnotate::ID = 2;
        static RegisterPass<Annotate> X("Annotate", "Annotate an IR", true, true);
        static RegisterPass<MAnnotate> Y("MAnnotate", "Annotate an IR module", false, false);
        static RegisterPass<EncodedAnnotate> Z("EncodedAnnotate", "Renames the basic blocks in the module", true, false);
    } // namespace Passes
} // namespace DashTracer