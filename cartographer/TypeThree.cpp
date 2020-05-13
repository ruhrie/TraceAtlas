#include "TypeThree.h"
#include "AtlasUtil/Annotate.h"
#include "cartographer.h"
#include <indicators/progress_bar.hpp>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Instructions.h>

using namespace std;
using namespace llvm;
namespace TypeThree
{
    std::set<std::set<int64_t>> Process(const std::set<std::set<int64_t>> &type25Kernels)
    {
        indicators::ProgressBar bar;
        if (!noBar)
        {
            bar.set_option(indicators::option::PrefixText{"Detecting type 3 kernels"});
            bar.set_option(indicators::option::ShowElapsedTime{true});
            bar.set_option(indicators::option::ShowRemainingTime{true});
            bar.set_option(indicators::option::BarWidth{50});
        }

        uint64_t total = type25Kernels.size();
        int status = 0;

        set<set<int64_t>> result;
        for (auto kernel : type25Kernels)
        {
            bool change = true;
            while (change)
            {
                change = false;
                for (auto block : kernel)
                {
                    BasicBlock *base = blockMap[block];
                    Function *F = base->getParent();
                    Instruction *term = base->getTerminator();
                    bool preFound = false;
                    bool sucFound = false;
                    if (base == &F->getEntryBlock())
                    {
                        for (auto user : F->users())
                        {
                            if (auto *cb = dyn_cast<CallBase>(user))
                            {
                                BasicBlock *par = cb->getParent();
                                int64_t id = GetBlockID(par);
                                if (kernel.find(id) != kernel.end())
                                {
                                    preFound = true;
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        //check if there is a valid predecessor
                        //aka mandate that everything has to be a part of the loop
                        for (auto pred : predecessors(base))
                        {
                            int64_t id = GetBlockID(pred);
                            if (kernel.find(id) != kernel.end())
                            {
                                preFound = true;
                                break;
                            }
                        }
                    }

                    if (isa<ReturnInst>(term)) //check if a ret
                    {
                        for (auto user : F->users())
                        {
                            if (auto *cb = dyn_cast<CallBase>(user))
                            {
                                BasicBlock *par = cb->getParent();
                                int64_t id = GetBlockID(par);
                                if (kernel.find(id) != kernel.end())
                                {
                                    sucFound = true;
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        //check if there is a valid successor
                        //aka mandate that everything has to be a part of the loop
                        for (auto suc : successors(base))
                        {
                            int64_t id = GetBlockID(suc);
                            if (kernel.find(id) != kernel.end())
                            {
                                sucFound = true;
                                break;
                            }
                        }
                    }

                    if (!preFound || !sucFound)
                    {
                        kernel.erase(block);
                        change = true;
                        break;
                    }
                }
            }
            result.insert(kernel);
            status++;
        }

        if (!noBar && !bar.is_completed())
        {
            bar.set_option(indicators::option::PostfixText{"Kernel " + to_string(status) + "/" + to_string(total)});
            bar.set_progress(100);
            bar.mark_as_completed();
        }
        return result;
    }
} // namespace TypeThree
