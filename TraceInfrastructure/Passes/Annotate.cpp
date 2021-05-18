#include "Passes/Annotate.h"
#include "AtlasUtil/Format.h"
#include <iostream>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace DashTracer::Passes
{
    cl::opt<uint64_t> TraceAtlasStartIndex("tai", llvm::cl::desc("Initial block index"), llvm::cl::value_desc("Initial block index"));
    cl::opt<uint64_t> TraceAtlasStartValueIndex("tavi", llvm::cl::desc("Initial value index"), llvm::cl::value_desc("Initial value index"));
    bool EncodedAnnotate::runOnModule(Module &M)
    {
        if (TraceAtlasStartIndex.getNumOccurrences() != 0)
        {
            TraceAtlasIndex = TraceAtlasStartIndex;
        }
        if (TraceAtlasStartValueIndex.getNumOccurrences() != 0)
        {
            TraceAtlasValueIndex = TraceAtlasStartValueIndex;
        }
        Format(&M);
        if (TraceAtlasStartIndex.getNumOccurrences() != 0)
        {
            std::cout << "Ending TraceAtlas block index: " << TraceAtlasIndex << std::endl;
        }
        if (TraceAtlasStartValueIndex.getNumOccurrences() != 0)
        {
            std::cout << "Ending TraceAtlas value index: " << TraceAtlasValueIndex << std::endl;
        }
        return true;
    }

    void EncodedAnnotate::getAnalysisUsage(AnalysisUsage &AU) const
    {
        AU.setPreservesAll();
    }
    char EncodedAnnotate::ID = 0;
    static RegisterPass<EncodedAnnotate> Z("EncodedAnnotate", "Renames the basic blocks in the module", true, false);
} // namespace DashTracer::Passes