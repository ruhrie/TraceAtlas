#include "tik/TikKernel.h"
#include "AtlasUtil/Exceptions.h"
#include <nlohmann/json.hpp>

using namespace llvm;
using namespace std;

namespace TraceAtlas::tik
{
    TikKernel::TikKernel(Function *kernFunc)
    {
        KernelFunction = kernFunc;
        Name = KernelFunction->getName();
        // TODO: Conditional init

        // get the metadata and parse it into entrance, exit, values
        //     turn metadata into string
        //     parse json and populate Entrances, Exits,
        MDNode *meta = KernelFunction->getMetadata("Boundaries");
        std::string metaString;
        if (auto mstring = dyn_cast<MDString>(meta->getOperand(0)))
        {
            metaString = mstring->getString();
        }
        else
        {
            AtlasException("Could not convert metadata into string.");
        }
        nlohmann::json js = nlohmann::json::parse(metaString);

        /* Needs to be changed to accomodate for new structure members
        for (auto &i : js["Entrances"])
        {
            Entrances.insert((int64_t)i);
        }*/
        std::set<BasicBlock *> blocks;
        for (auto BB = KernelFunction->begin(); BB != KernelFunction->end(); BB++)
        {
            auto block = cast<BasicBlock>(BB);
            blocks.insert(block);
        }
        // initialize Init, Exit, Exception, KernelImports, KernelExports
        for (auto block : blocks)
        {
            // Special class members
            if (block->getName() == "Init")
            {
                Init = block;
            }
            else if (block->getName() == "Exit")
            {
                Exit = block;
            }
            else if (block->getName() == "Exception")
            {
                Exception = block;
            }
            else
            {
                continue;
            }
            /*
            // initialize KernelImports, KernelExports
            for (BasicBlock::iterator BI = block->begin(), BE = block->end(); BI != BE; ++BI)
            {
                auto *inst = cast<Instruction>(BI);
                //start by getting all the inputs
                //they will be composed of the operands whose input is not defined in one of the parent blocks
                uint32_t numOps = inst->getNumOperands();
                for (uint32_t i = 0; i < numOps; i++)
                {
                    Value *op = inst->getOperand(i);
                    if (auto *operand = dyn_cast<Instruction>(op))
                    {
                        BasicBlock *parentBlock = operand->getParent();
                        if (std::find(blocks.begin(), blocks.end(), parentBlock) == blocks.end())
                        {
                            if (find(KernelImports.begin(), KernelImports.end(), operand) == KernelImports.end())
                            {
                                KernelImports.push_back(operand);
                            }
                        }
                    }
                    else if (auto *ar = dyn_cast<Argument>(op))
                    {
                        if (auto *ci = dyn_cast<CallInst>(inst))
                        {
                            if (KfMap.find(ci->getCalledFunction()) != KfMap.end())
                            {
                                auto subKernel = KfMap[ci->getCalledFunction()];
                                for (auto sExtVal : subKernel->KernelImports)
                                {
                                    //these are the arguments for the function call in order
                                    //we now can check if they are in our vmap, if so they aren't external
                                    //if not they are and should be mapped as is appropriate
                                    if (find(KernelImports.begin(), KernelImports.end(), sExtVal) == KernelImports.end())
                                    {
                                        KernelImports.push_back(sExtVal);
                                    }
                                }
                            }
                        }
                        else
                        {
                            if (find(KernelImports.begin(), KernelImports.end(), ar) == KernelImports.end())
                            {
                                KernelImports.push_back(ar);
                            }
                        }
                    }
                }

                //then get all the exports
                //this is composed of all the instructions whose use extends beyond the current blocks
                for (auto user : inst->users())
                {
                    if (auto i = dyn_cast<Instruction>(user))
                    {
                        auto p = i->getParent();
                        if (blocks.find(p) == blocks.end())
                        {
                            //the use is external therefore it should be a kernel export
                            KernelExports.push_back(inst);
                            break;
                        }
                    }
                    else
                    {
                        throw AtlasException("Non-instruction user detected");
                    }
                }
            }*/
        }
        /*// initialize ArgumentMap
        uint64_t i;
        for (i = 0; i < KernelImports.size(); i++)
        {
            auto *a = cast<Argument>(KernelFunction->arg_begin() + 1 + i);
            a->setName("i" + to_string(i));
            ArgumentMap[a] = KernelImports[i];
        }
        uint64_t j;
        for (j = 0; j < KernelExports.size(); j++)
        {
            auto *a = cast<Argument>(KernelFunction->arg_begin() + 1 + i + j);
            a->setName("e" + to_string(j));
            ArgumentMap[a] = KernelExports[j];
        }*/
    }
} // namespace TraceAtlas::tik