#include "tik/Kernel.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Print.h"
#include "tik/Exceptions.h"
#include "tik/InlineStruct.h"
#include "tik/Metadata.h"
#include "tik/TikHeader.h"
#include "tik/Util.h"
#include "tik/tik.h"
#include <algorithm>
#include <iostream>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <queue>
#include <spdlog/spdlog.h>

using namespace llvm;
using namespace std;

static int KernelUID = 0;

set<string> reservedNames;

Kernel::Kernel(std::vector<int> basicBlocks, Module *M, string name)
{
    MemoryRead = NULL;
    MemoryWrite = NULL;
    Init = NULL;
    Exit = NULL;
    string Name = "";
    if (name.empty())
    {
        Name = "Kernel_" + to_string(KernelUID++);
    }
    else if (name.front() >= '0' && name.front() <= '9')
    {
        Name = "K" + name;
    }
    else
    {
        Name = name;
    }
    if (reservedNames.find(Name) != reservedNames.end())
    {
        throw TikException("Kernel Error: Kernel names must be unique!");
    }
    reservedNames.insert(Name);

    set<BasicBlock *> blocks;
    for (Module::iterator F = M->begin(), E = M->end(); F != E; ++F)
    {
        for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
        {
            BasicBlock *b = cast<BasicBlock>(BB);
            int64_t id = GetBlockID(b);
            if (id != -1)
            {
                if (find(basicBlocks.begin(), basicBlocks.end(), id) != basicBlocks.end())
                {
                    blocks.insert(b);
                }
            }
        }
    }

    try
    {
        //this is a recursion check, just so we can enumerate issues
        for (auto block : blocks)
        {
            Function *f = block->getParent();
            for (auto bi = block->begin(); bi != block->end(); bi++)
            {
                if (CallBase *cb = dyn_cast<CallBase>(bi))
                {
                    if (cb->getCalledFunction() == f)
                    {
                        throw TikException("Tik Error: Recursion is unimplemented")
                    }
                }
            }
        }

        //SplitBlocks(blocks);

        GetEntrances(blocks);
        GetExits(blocks);
        GetConditional(blocks);
        GetExternalValues(blocks);

        //we now have all the information we need

        //start by making the correct function
        std::vector<llvm::Type *> inputArgs;
        inputArgs.push_back(Type::getInt8Ty(TikModule->getContext()));
        for (auto inst : ExternalValues)
        {
            inputArgs.push_back(inst->getType());
        }
        FunctionType *funcType = FunctionType::get(Type::getInt8Ty(TikModule->getContext()), inputArgs, false);
        KernelFunction = Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, Name, TikModule);
        for (int i = 0; i < ExternalValues.size(); i++)
        {
            Argument *a = cast<Argument>(KernelFunction->arg_begin() + 1 + i);
            VMap[ExternalValues[i]] = a;
            ArgumentMap[a] = ExternalValues[i];
        }

        //create the artificial blocks
        Init = BasicBlock::Create(TikModule->getContext(), "Init", KernelFunction);
        Exit = BasicBlock::Create(TikModule->getContext(), "Exit", KernelFunction);

        //copy the appropriate blocks
        BuildKernel(blocks);

        BuildExit();

        CopyGlobals();

        //remap and repipe
        Remap();
        //might be fused
        Repipe();

        // replace external function calls with tik declarations
        ExportFunctionSignatures();

        //handle the memory operations
        GetMemoryFunctions();

        UpdateMemory();

        BuildInit();

        RemapNestedKernels();

        //apply metadata
        ApplyMetadata();

        //we will also do some minor cleanup
        for (Function::iterator fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
        {
            for (BasicBlock::iterator bi = fi->begin(); bi != fi->end(); bi++)
            {
                for (auto usr : bi->users())
                {
                    if (auto inst = dyn_cast<Instruction>(usr))
                    {
                        if (inst->getParent() == NULL)
                        {
                        }
                    }
                }
            }
        }

        //and set a flag that we succeeded
        Valid = true;
    }
    catch (TikException &e)
    {
        std::cerr << "Failed to convert kernel to tik"
                  << "\n";
        std::cerr << e.what() << '\n';
        Cleanup();
    }
}

nlohmann::json Kernel::GetJson()
{
    nlohmann::json j;
    vector<string> args;
    for (Argument *i = KernelFunction->arg_begin(); i < KernelFunction->arg_end(); i++)
    {
        args.push_back(GetString(i));
    }
    if (args.size() != 0)
    {
        j["Inputs"] = args;
    }
    if (Init != NULL)
    {
        j["Init"] = GetStrings(Init);
    }
    if (Body.size() != 0)
    {
        for (auto b : Body)
        {
            j["Body"].push_back(GetStrings(b));
        }
    }
    if (Termination.size() != 0)
    {
        for (auto b : Termination)
        {
            j["Termination"].push_back(GetStrings(b));
        }
    }
    if (Exit != NULL)
    {
        j["Exit"] = GetStrings(Exit);
    }
    if (MemoryRead != NULL)
    {
        j["MemoryRead"] = GetStrings(MemoryRead);
    }
    if (MemoryWrite != NULL)
    {
        j["MemoryWrite"] = GetStrings(MemoryWrite);
    }
    if (!Conditional.empty())
    {
        for (auto cond : Conditional)
        {
            j["Conditional"].push_back(GetStrings(cond));
        }
    }
    return j;
}

void Kernel::Cleanup()
{
    if (KernelFunction)
    {
        KernelFunction->removeFromParent();
    }
    if (MemoryRead)
    {
        MemoryRead->removeFromParent();
    }
    if (MemoryWrite)
    {
        MemoryWrite->removeFromParent();
    }
    for (auto g : GlobalMap)
    {
        g.second->removeFromParent();
    }
}

Kernel::~Kernel()
{
}

void Kernel::ExportFunctionSignatures()
{
    for (auto bi = KernelFunction->begin(); bi != KernelFunction->end(); bi++)
    {
        for (auto inst = bi->begin(); inst != bi->end(); inst++)
        {
            if (CallBase *callInst = dyn_cast<CallBase>(inst))
            {
                Function *f = callInst->getCalledFunction();
                if (f != MemoryRead && f != MemoryWrite)
                {
                    llvm::Function *funcDec = llvm::Function::Create(callInst->getCalledFunction()->getFunctionType(), GlobalValue::LinkageTypes::ExternalLinkage, callInst->getCalledFunction()->getName(), TikModule);
                    funcDec->setAttributes(callInst->getCalledFunction()->getAttributes());
                    callInst->setCalledFunction(funcDec);
                }
            }
        }
    }
}

void Kernel::UpdateMemory()
{
    std::set<llvm::Value *> coveredGlobals;
    std::set<llvm::StoreInst *> newStores;
    IRBuilder<> initBuilder(Init);
    for (int i = 0; i < ExternalValues.size(); i++)
    {
        PrintVal(GlobalMap[VMap[ExternalValues[i]]]);
        PrintVal(ExternalValues[i]);
        if (GlobalMap.find(VMap[ExternalValues[i]]) != GlobalMap.end())
        {
            if (GlobalMap[VMap[ExternalValues[i]]] == NULL)
            {
                throw TikException("Tik Error: External Value not found in GlobalMap.");
            }
            coveredGlobals.insert(GlobalMap[VMap[ExternalValues[i]]]);
            auto b = initBuilder.CreateStore(KernelFunction->arg_begin() + i + 1, GlobalMap[VMap[ExternalValues[i]]]);
            MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikSynthetic::Store))));
            b->setMetadata("TikSynthetic", tikNode);
            newStores.insert(b);
        }
    }

    for (Function::iterator bb = KernelFunction->begin(), be = KernelFunction->end(); bb != be; bb++)
    {
        for (BasicBlock::iterator BI = bb->begin(), BE = bb->end(); BI != BE; ++BI)
        {
            Instruction *inst = cast<Instruction>(BI);
            for (auto pair : GlobalMap)
            {
                if (pair.first == inst)
                {
                    if (coveredGlobals.find(pair.second) == coveredGlobals.end())
                    {
                        IRBuilder<> builder(inst->getNextNode());
                        Constant *constant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0);
                        auto a = builder.CreateGEP(inst->getType(), pair.second, constant);
                        auto b = builder.CreateStore(inst, a);
                        MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikSynthetic::Store))));
                        b->setMetadata("TikSynthetic", tikNode);
                    }
                }
            }
        }
    }
}

void Kernel::Remap()
{
    for (Function::iterator BB = KernelFunction->begin(), E = KernelFunction->end(); BB != E; ++BB)
    {
        for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI)
        {
            Instruction *inst = cast<Instruction>(BI);
            RemapInstruction(inst, VMap, llvm::RF_None);
        }
    }
}

void Kernel::RemapNestedKernels()
{
    // look through the body for pointer references
    // every time we see a global reference who is not written to in MemRead, not stored to in Init, store to it in body

    // every time we use a global pointer in the body that is not in MemWrite, store to it

    // loop->body or loop->exit (conditional)
    // body->loop (unconditional)

    // Now find all calls to the embedded kernel functions in the body, if any, and change their arguments to the new ones
    std::map<Argument *, Value *> embeddedCallArgs;
    for (auto b = KernelFunction->begin(); b != KernelFunction->end(); b++)
    {
        for (BasicBlock::iterator i = b->begin(), BE = b->end(); i != BE; ++i)
        {
            if (CallInst *callInst = dyn_cast<CallInst>(i))
            {
                Function *funcCal = callInst->getCalledFunction();
                llvm::Function *funcName = TikModule->getFunction(funcCal->getName());
                if (!funcName)
                {
                    // we have a non-kernel function call
                }
                else if (funcName != MemoryRead && funcName != MemoryWrite) // must be a kernel function call
                {
                    bool found = false;
                    auto calledFunc = callInst->getCalledFunction();
                    auto subK = KfMap[calledFunc];
                    if (subK)
                    {
                        for (auto sarg = calledFunc->arg_begin(); sarg < calledFunc->arg_end(); sarg++)
                        {
                            for (auto b = KernelFunction->begin(); b != KernelFunction->end(); b++)
                            {
                                for (BasicBlock::iterator j = b->begin(), BE2 = b->end(); j != BE2; ++j)
                                {
                                    if (subK->ArgumentMap[sarg] == cast<Instruction>(j))
                                    {
                                        found = true;
                                        embeddedCallArgs[sarg] = cast<Instruction>(j);
                                    }
                                }
                            }
                        }
                        for (auto sarg = calledFunc->arg_begin(); sarg < calledFunc->arg_end(); sarg++)
                        {
                            for (auto arg = KernelFunction->arg_begin(); arg < KernelFunction->arg_end(); arg++)
                            {
                                if (subK->ArgumentMap[sarg] == ArgumentMap[arg])
                                {
                                    embeddedCallArgs[sarg] = arg;
                                }
                            }
                        }
                        auto limit = callInst->getNumArgOperands();
                        for (int k = 0; k < limit; k++)
                        {
                            Value *op = callInst->getArgOperand(k);
                            if (Argument *arg = dyn_cast<Argument>(op))
                            {
                                auto asdf = embeddedCallArgs[arg];
                                callInst->setArgOperand(k, asdf);
                            }
                            else if (Constant *c = dyn_cast<Constant>(op))
                            {
                                //we don't have to do anything so ignore
                            }
                            else
                            {
                                throw TikException("Tik Error: Unexpected value passed to function");
                            }
                        }
                    }
                }
            }
        }
    }
}

void Kernel::BuildInit()
{
    IRBuilder<> initBuilder(Init);
    auto initSwitch = initBuilder.CreateSwitch(KernelFunction->arg_begin(), Exit, Entrances.size());
    int i = 0;
    for (auto ent : Entrances)
    {
        initSwitch->addCase(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), i), cast<BasicBlock>(VMap[ent]));
        i++;
    }
}

void Kernel::GetConditional(std::set<llvm::BasicBlock *> &blocks)
{
    set<BasicBlock *> conditions;

    //start by marking all blocks that have conditions
    for (auto block : blocks)
    {
        auto term = block->getTerminator();
        int sucCount = term->getNumSuccessors();
        if (sucCount > 1)
        {
            conditions.insert(block);
        }
    }

    //now that we have these conditions we will go a depth search to see if it is a successor of itself
    set<BasicBlock *> validConditions;
    map<BasicBlock *, set<BasicBlock *>> exitDict;
    map<BasicBlock *, set<BasicBlock *>> recurseDict;
    for (auto cond : conditions)
    {
        bool exRecurses = false;
        bool exExit = false;

        set<BasicBlock *> exitPaths;
        set<BasicBlock *> recursePaths;

        //ideally one of them will recurse and one of them will exit
        for (auto suc : successors(cond))
        {
            queue<BasicBlock *> toProcess;
            set<BasicBlock *> checked;
            toProcess.push(suc);
            checked.insert(suc);
            checked.insert(cond);
            bool recurses = false;
            bool exit = false;
            while (!toProcess.empty())
            {
                BasicBlock *processing = toProcess.front();
                toProcess.pop();
                auto term = processing->getTerminator();
                if (term->getNumSuccessors() == 0)
                {
                    exit = true;
                }
                for (auto succ : successors(processing))
                {
                    if (succ == cond)
                    {
                        recurses = true;
                    }
                    if (blocks.find(succ) == blocks.end())
                    {
                        exit = true;
                    }
                    if (checked.find(succ) == checked.end() && blocks.find(succ) != blocks.end())
                    {
                        toProcess.push(succ);
                        checked.insert(succ);
                    }
                }
            }
            if (recurses && exit)
            {
                //this did both which implies that it can't be the condition
                continue;
            }
            if (recurses)
            {
                exRecurses = true;
                //this branch is the body branch
                recursePaths.insert(suc);
            }
            if (exit)
            {
                exExit = true;
                //and this one exits
                exitPaths.insert(suc);
            }
        }

        if (exExit && exRecurses)
        {
            validConditions.insert(cond);
            recurseDict[cond] = recursePaths;
            exitDict[cond] = exitPaths;
        }
    }

    for (auto b : validConditions)
    {
        Conditional.insert(b);

        auto exitPaths = exitDict[b];
        auto recPaths = recurseDict[b];
        //now process the body
        {
            queue<BasicBlock *> processing;
            set<BasicBlock *> visited;
            for (auto block : recPaths)
            {
                processing.push(block);
                visited.insert(block);
            }
            for (auto c : validConditions)
            {
                visited.insert(c);
            }
            while (!processing.empty())
            {
                auto a = processing.front();
                processing.pop();
                visited.insert(a);
                if (find(Body.begin(), Body.end(), a) == Body.end())
                {
                    if (blocks.find(a) != blocks.end())
                    {
                        Body.insert(a);
                    }
                }
                for (auto suc : successors(a))
                {
                    if (validConditions.find(suc) == validConditions.end())
                    {
                        if (visited.find(suc) == visited.end())
                        {
                            processing.push(suc);
                        }
                    }
                }
            }
        }
        /*
        //and the terminus
        {
            queue<BasicBlock *> processing;
            set<BasicBlock *> visited;
            for (auto block : exitPaths)
            {
                processing.push(block);
                visited.insert(block);
            }
            while (!processing.empty())
            {
                auto a = processing.front();
                processing.pop();
                visited.insert(a);
                if (find(Termination.begin(), Termination.end(), a) == Termination.end())
                {
                    if (blocks.find(a) != blocks.end())
                    {
                        //throw TikException("Tik Error: Detected terminus block");
                        Termination.insert(a);
                    }
                }
                for (auto suc : successors(a))
                {
                    if (validConditions.find(suc) == validConditions.end())
                    {
                        if (visited.find(suc) == visited.end())
                        {
                            processing.push(suc);
                        }
                    }
                }
            }
        }
        */
    }

    for (auto block : blocks)
    {
        if (Body.find(block) == Body.end())
        {
            Termination.insert(block);
        }
    }
}

void Kernel::BuildKernel(set<BasicBlock *> &blocks)
{
    for (auto block : blocks)
    {
        int64_t id = GetBlockID(block);
        if (KernelMap.find(id) != KernelMap.end())
        {
            //this belongs to a subkernel
            auto nestedKernel = KernelMap[id];
            if (nestedKernel->Entrances.find(block) == nestedKernel->Entrances.end())
            {
                //we need to make a unique block for each entrance (there is currently only one)
                int i = 0;
                for (auto ent : nestedKernel->Entrances)
                {
                    std::vector<llvm::Value *> inargs;
                    for (auto ai = nestedKernel->KernelFunction->arg_begin(); ai < nestedKernel->KernelFunction->arg_end(); ai++)
                    {
                        if (ai == nestedKernel->KernelFunction->arg_begin())
                        {
                            inargs.push_back(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), i));
                        }
                        else
                        {
                            inargs.push_back(cast<Value>(ai));
                        }
                    }
                    BasicBlock *intermediateBlock = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
                    IRBuilder<> intBuilder(intermediateBlock);
                    auto cc = intBuilder.CreateCall(nestedKernel->KernelFunction, inargs);
                    auto sw = intBuilder.CreateSwitch(cc, Exit, nestedKernel->ExitTarget.size());
                    for (auto pair : nestedKernel->ExitTarget)
                    {

                        if (blocks.find(pair.second) != blocks.end())
                        {
                            sw->addCase(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), pair.first), pair.second);
                        }
                        else
                        {
                            //exits both kernels simultaneously
                            //we create a temp block and remab the exit so the phi has a value
                            //then remap in the dictionary for the final mapping
                            //note that we do not change the ExitTarget map so we still go to the right place
                            BasicBlock *newBlock = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
                            IRBuilder<> newBlockBuilder(newBlock);
                            newBlockBuilder.CreateBr(Exit);
                            sw->addCase(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), pair.first), newBlock);
                            int index = ExitMap[pair.second];
                            ExitMap.erase(pair.second);
                            ExitMap[newBlock] = index;
                        }
                    }
                    VMap[block] = intermediateBlock;
                    if (Body.find(block) != Body.end())
                    {
                        Body.insert(intermediateBlock);
                        Body.erase(intermediateBlock);
                    }
                    else if (Termination.find(block) != Termination.end())
                    {
                        Termination.insert(intermediateBlock);
                        Termination.erase(intermediateBlock);
                    }
                    else
                    {
                        throw TikException("Tik Error: Block not assigned to Body or Terminus");
                    }
                    i++;
                }
            }
        }
        else
        {
            auto cb = CloneBasicBlock(block, VMap, "", KernelFunction);
            if (Conditional.find(block) != Conditional.end())
            {
                Conditional.erase(block);
                Conditional.insert(cb);
            }
            vector<CallInst *> toInline;
            for (auto bi = cb->begin(); bi != cb->end(); bi++)
            {
                if (CallInst *ci = dyn_cast<CallInst>(bi))
                {
                    toInline.push_back(ci);
                }
            }
            BasicBlock *working = cb;
            for (auto ci : toInline)
            {
                if (ci->isIndirectCall())
                {
                    throw TikException("Tik Error: Indirect calls aren't supported")
                }
                else
                {
                    auto calledFunc = ci->getCalledFunction();
                    if (!calledFunc)
                    {
                        throw TikException("Tik Error: Call inst was null")
                    }

                    if (!calledFunc->empty())
                    {
                        //we need to do a check here to see if we already inlined it
                        InlineStruct currentStruct;
                        for (auto inl : InlinedFunctions)
                        {
                            if (inl.CalledFunction == calledFunc)
                            {
                                currentStruct = inl;
                                break;
                            }
                        }
                        if (currentStruct.CalledFunction == NULL)
                        {
                            //needs to be inlined
                            currentStruct.CalledFunction = calledFunc;
                            BasicBlock *suffix = working->splitBasicBlock(ci);
                            Body.insert(suffix);
                            //first create the phi block which is the entry point
                            BasicBlock *phiBlock = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
                            Body.insert(phiBlock);
                            IRBuilder<> phiBuilder(phiBlock);
                            //first phi we need is the number of exit paths
                            vector<BasicBlock *> funcUses;
                            for (auto user : calledFunc->users())
                            {
                                if (CallInst *callUse = dyn_cast<CallInst>(user))
                                {
                                    BasicBlock *parent = callUse->getParent();
                                    if (find(Body.begin(), Body.end(), parent) != Body.end())
                                    {
                                        funcUses.push_back(parent);
                                    }
                                }
                                else
                                {
                                    throw TikException("Tik Error: Only expected callInst");
                                }
                            }
                            //now that we know that we can create the phi for where to branch to
                            auto branchPhi = phiBuilder.CreatePHI(Type::getInt8Ty(TikModule->getContext()), funcUses.size());
                            int i = 0;
                            for (auto func : funcUses) //and populate it with the entry for this call at least
                            {
                                branchPhi->addIncoming(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), i++), func);
                            }
                            //then we do the same for ever argument
                            int argIndex = 0;
                            for (auto ai = calledFunc->arg_begin(); ai != calledFunc->arg_end(); ai++)
                            {
                                Argument *arg = cast<Argument>(ai);
                                Value *passedValue = ci->getOperand(argIndex++);
                                auto argPhi = phiBuilder.CreatePHI(arg->getType(), funcUses.size()); //create a phi for the arg
                                argPhi->addIncoming(passedValue, cb);                                //and give it a value for the current call instruction
                                VMap[arg] = argPhi;
                                currentStruct.ArgNodes.push_back(argPhi);
                            }
                            phiBuilder.CreateBr(&calledFunc->getEntryBlock()); //after this we can finally branch into the function

                            //we also need a block at the end to gather the return values
                            auto returnBlock = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
                            Body.insert(returnBlock);
                            int returnCount = 0; //just like before we need a count
                            map<BasicBlock *, Value *> returnMap;
                            for (auto fi = calledFunc->begin(); fi != calledFunc->end(); fi++)
                            {
                                BasicBlock *fBasicBlock = cast<BasicBlock>(fi);
                                if (VMap.find(fBasicBlock) != VMap.end())
                                {
                                    fBasicBlock = cast<BasicBlock>(VMap[fBasicBlock]);
                                }
                                if (ReturnInst *ri = dyn_cast<ReturnInst>(fBasicBlock->getTerminator()))
                                {
                                    returnMap[fBasicBlock] = ri->getReturnValue();
                                    ri->eraseFromParent(); //we also remove the return here because we don't need it
                                    returnCount++;
                                    IRBuilder<> fIteratorBuilder(fBasicBlock);
                                    fIteratorBuilder.CreateBr(returnBlock);
                                }
                            }

                            //with the count we create the phi nodes iff the return type isn't void
                            IRBuilder<> returnBuilder(returnBlock);
                            if (calledFunc->getReturnType() != Type::getVoidTy(TikModule->getContext()))
                            {
                                auto returnPhi = returnBuilder.CreatePHI(calledFunc->getReturnType(), returnCount);
                                for (auto pair : returnMap)
                                {
                                    returnPhi->addIncoming(pair.second, pair.first);
                                }
                                ci->replaceAllUsesWith(returnPhi);
                            }
                            //finally we use the first phi we created to determine where we should return to
                            auto branchSwitch = returnBuilder.CreateSwitch(branchPhi, suffix, funcUses.size());
                            branchSwitch->addCase(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), 0), suffix);
                            currentStruct.SwitchInstruction = branchSwitch;

                            //and redirect the first block
                            BranchInst *priorBranch = cast<BranchInst>(cb->getTerminator());
                            priorBranch->setSuccessor(0, phiBlock);
                            ci->eraseFromParent();

                            InlinedFunctions.push_back(currentStruct); //finally add it to the already inlined functions
                            working = suffix;
                        }
                        else
                        {
                            //we already inlined this one and need to add the appropriate entries to teh argnodes and the switch instruction
                            throw TikException("Tik Error: Not Implemented");
                        }
                    }
                }
            }
            if (Body.find(block) != Body.end())
            {
                Body.erase(block);
                Body.insert(cb);
            }
            else if (Termination.find(block) != Termination.end())
            {
                Termination.erase(block);
                Termination.insert(cb);
            }
            else
            {
                throw TikException("Tik Error: block not in Body or Termination");
            }
            VMap[block] = cb;

            //fix the phis
            int rescheduled = 0; //the number of blocks we rescheduled
            for (auto bi = cb->begin(); bi != cb->end(); bi++)
            {
                if (PHINode *p = dyn_cast<PHINode>(bi))
                {
                    for (auto pred : p->blocks())
                    {
                        if (blocks.find(pred) == blocks.end())
                        {
                            //we have an invalid predecessor, replace with init
                            p->replaceIncomingBlockWith(pred, Init);
                            rescheduled++;
                        }
                    }
                }
                else
                {
                    break;
                }
            }
            if (rescheduled > 1)
            {
                spdlog::warn("Rescheduled more than one phi predecessor"); //basically this is a band aid. Needs some more help
            }
        }
    }
}

void Kernel::GetExternalValues(set<BasicBlock *> &blocks)
{
    for (auto bb : blocks)
    {
        for (BasicBlock::iterator BI = bb->begin(), BE = bb->end(); BI != BE; ++BI)
        {
            Instruction *inst = cast<Instruction>(BI);
            int numOps = inst->getNumOperands();
            for (int i = 0; i < numOps; i++)
            {
                Value *op = inst->getOperand(i);
                if (Instruction *operand = dyn_cast<Instruction>(op))
                {
                    BasicBlock *parentBlock = operand->getParent();
                    if (std::find(blocks.begin(), blocks.end(), parentBlock) == blocks.end())
                    {
                        if (find(ExternalValues.begin(), ExternalValues.end(), operand) == ExternalValues.end())
                        {
                            ExternalValues.push_back(operand);
                        }
                    }
                }
                else if (Argument *ar = dyn_cast<Argument>(op))
                {
                    if (CallInst *ci = dyn_cast<CallInst>(inst))
                    {
                        if (KfMap.find(ci->getCalledFunction()) != KfMap.end())
                        {
                            auto subKernel = KfMap[ci->getCalledFunction()];
                            for (auto sExtVal : subKernel->ExternalValues)
                            {
                                //these are the arguments for the function call in order
                                //we now can check if they are in our vmap, if so they aren't external
                                //if not they are and should be mapped as is appropriate
                                Value *v = VMap[sExtVal];
                                if (!v)
                                {
                                    if (find(ExternalValues.begin(), ExternalValues.end(), sExtVal) == ExternalValues.end())
                                    {
                                        ExternalValues.push_back(sExtVal);
                                    }
                                }
                                else //this is a mapped value, therefore it is in the kernel locally
                                {
                                }
                            }
                        }
                        else
                        {
                            throw TikException("Tik Error: Not Implemented");
                        }
                    }
                    else
                    {
                        if (find(ExternalValues.begin(), ExternalValues.end(), ar) == ExternalValues.end())
                        {
                            ExternalValues.push_back(ar);
                        }

                        bool external = false;
                        for (auto user : ar->getParent()->users())
                        {
                            if (auto a = dyn_cast<Instruction>(user))
                            {
                                if (blocks.find(a->getParent()) == blocks.end())
                                {
                                    external = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void Kernel::GetMemoryFunctions()
{
    // first, get all the pointer operands of each load and store in Kernel::Body
    /** new for loop grabbing all inputs of child kernels */

    set<LoadInst *> loadInst;
    set<StoreInst *> storeInst;
    for (auto ki = KernelFunction->begin(); ki != KernelFunction->end(); ki++)
    {
        auto bb = cast<BasicBlock>(ki);
        for (BasicBlock::iterator BI = bb->begin(), BE = bb->end(); BI != BE; ++BI)
        {
            Instruction *inst = cast<Instruction>(BI);
            if (LoadInst *newInst = dyn_cast<LoadInst>(inst))
            {
                loadInst.insert(newInst);
            }
            else if (StoreInst *newInst = dyn_cast<StoreInst>(inst))
            {
                storeInst.insert(newInst);
            }
        }
    }
    set<Value *> loadValues;
    set<Value *> storeValues;
    for (LoadInst *load : loadInst)
    {
        Value *loadVal = load->getPointerOperand();
        loadValues.insert(loadVal);
    }
    for (StoreInst *store : storeInst)
    {
        Value *storeVal = store->getPointerOperand();
        storeValues.insert(storeVal);
    }

    // create MemoryRead, MemoryWrite functions
    FunctionType *funcType = FunctionType::get(Type::getInt64Ty(TikModule->getContext()), Type::getInt64Ty(TikModule->getContext()), false);
    MemoryRead = Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, "MemoryRead", TikModule);
    MemoryWrite = Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, "MemoryWrite", TikModule);

    int i = 0;
    BasicBlock *loadBlock = BasicBlock::Create(TikModule->getContext(), "entry", MemoryRead);
    IRBuilder<> loadBuilder(loadBlock);
    Value *priorValue = NULL;

    // Add ExternalValues to the global map

    // MemoryRead
    map<Value *, Value *> loadMap;
    for (Value *lVal : loadValues)
    {
        // since MemoryRead is a function, its pointers need to be globally scoped so it and the Kernel function can use them
        if (GlobalMap.find(lVal) == GlobalMap.end())
        {
            llvm::Constant *globalInt = ConstantPointerNull::get(cast<PointerType>(lVal->getType()));
            llvm::GlobalVariable *g = new GlobalVariable(*TikModule, globalInt->getType(), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, globalInt);
            GlobalMap[lVal] = g;
        }
        VMap[lVal] = GlobalMap[lVal];
        Constant *constant = ConstantInt::get(Type::getInt64Ty(TikModule->getContext()), 0);
        auto a = loadBuilder.CreateGEP(lVal->getType(), GlobalMap[lVal], constant);
        auto b = loadBuilder.CreateLoad(a);
        Instruction *converted = cast<Instruction>(loadBuilder.CreatePtrToInt(b, Type::getInt64Ty(TikModule->getContext())));
        Constant *indexConstant = ConstantInt::get(Type::getInt64Ty(TikModule->getContext()), i++);
        loadMap[lVal] = indexConstant;
        if (priorValue == NULL)
        {
            priorValue = converted;
        }
        else
        {
            ICmpInst *cmpInst = cast<ICmpInst>(loadBuilder.CreateICmpEQ(MemoryRead->arg_begin(), indexConstant));
            SelectInst *sInst = cast<SelectInst>(loadBuilder.CreateSelect(cmpInst, converted, priorValue));
            priorValue = sInst;
        }
    }
    if (!priorValue)
    {
        spdlog::warn("Empty kernel read encountered");
        loadBuilder.CreateRet(ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0));
    }
    else
    {
        loadBuilder.CreateRet(priorValue);
    }

    // MemoryWrite
    i = 0;
    BasicBlock *storeBlock = BasicBlock::Create(TikModule->getContext(), "entry", MemoryWrite);
    IRBuilder<> storeBuilder(storeBlock);
    map<Value *, Value *> storeMap;
    priorValue = NULL;
    for (Value *sVal : storeValues)
    {
        // since MemoryWrite is a function, its pointers need to be globally scoped so it and the Kernel function can use them
        if (GlobalMap.find(sVal) == GlobalMap.end())
        {
            llvm::Constant *globalInt = ConstantPointerNull::get(cast<PointerType>(sVal->getType()));
            llvm::GlobalVariable *g = new GlobalVariable(*TikModule, globalInt->getType(), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, globalInt);
            GlobalMap[sVal] = g;
        }
        if (GlobalMap.find(sVal) == GlobalMap.end())
        {
            continue;
        }
        Constant *constant = ConstantInt::get(Type::getInt64Ty(TikModule->getContext()), 0);
        auto a = storeBuilder.CreateGEP(sVal->getType(), GlobalMap[sVal], constant);
        auto b = storeBuilder.CreateLoad(a);
        Instruction *converted = cast<Instruction>(storeBuilder.CreatePtrToInt(b, Type::getInt64Ty(TikModule->getContext())));
        Constant *indexConstant = ConstantInt::get(Type::getInt64Ty(TikModule->getContext()), i++);
        if (priorValue == NULL)
        {
            priorValue = converted;
            storeMap[sVal] = indexConstant;
        }
        else
        {
            ICmpInst *cmpInst = cast<ICmpInst>(storeBuilder.CreateICmpEQ(MemoryWrite->arg_begin(), indexConstant));
            SelectInst *sInst = cast<SelectInst>(storeBuilder.CreateSelect(cmpInst, converted, priorValue));
            priorValue = sInst;
            storeMap[sVal] = indexConstant;
        }
    }
    if (!priorValue)
    {
        spdlog::warn("Empty kernel write encountered");
        storeBuilder.CreateRet(ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0));
    }
    else
    {
        storeBuilder.CreateRet(priorValue);
    }

    // find instructions in body block not belonging to parent kernel
    vector<Instruction *> toRemove;
    for (auto bb = KernelFunction->begin(); bb != KernelFunction->end(); bb++)
    {
        for (BasicBlock::iterator BI = bb->begin(), BE = bb->end(); BI != BE; ++BI)
        {
            Instruction *inst = cast<Instruction>(BI);
            IRBuilder<> builder(inst);
            if (LoadInst *newInst = dyn_cast<LoadInst>(inst))
            {
                auto readAddr = loadMap[newInst->getPointerOperand()];
                if (readAddr == NULL)
                {
                    throw TikException("Tik Error: Missing address for load");
                }
                auto memCall = builder.CreateCall(MemoryRead, readAddr);
                auto casted = cast<Instruction>(builder.CreateIntToPtr(memCall, newInst->getPointerOperand()->getType()));
                MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikSynthetic::Cast))));
                casted->setMetadata("TikSynthetic", tikNode);
                auto newLoad = builder.CreateLoad(casted);
                newInst->replaceAllUsesWith(newLoad);
                toRemove.push_back(newInst);
            }
            else if (StoreInst *newInst = dyn_cast<StoreInst>(inst))
            {
                auto writeAddr = storeMap[newInst->getPointerOperand()];
                if (writeAddr == NULL)
                {
                    throw TikException("Tik Error: Missing address for store");
                }
                auto memCall = builder.CreateCall(MemoryWrite, writeAddr);
                auto casted = cast<Instruction>(builder.CreateIntToPtr(memCall, newInst->getPointerOperand()->getType()));
                MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikSynthetic::Cast))));
                casted->setMetadata("TikSynthetic", tikNode);
                auto newStore = builder.CreateStore(newInst->getValueOperand(), casted); //storee);
                toRemove.push_back(newInst);
            }
        }
    }

    // remove the instructions that don't belong
    for (auto inst : toRemove)
    {
        inst->eraseFromParent();
    }
}

void Kernel::BuildExit()
{
    IRBuilder<> exitBuilder(Exit);
    auto phi = exitBuilder.CreatePHI(Type::getInt8Ty(TikModule->getContext()), ExitMap.size() + 1);
    for (auto pair : ExitMap)
    {
        auto v = VMap[pair.first];
        phi->addIncoming(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), pair.second), cast<BasicBlock>(VMap[pair.first]));
    }
    phi->addIncoming(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), -1), Init);
    exitBuilder.CreateRet(phi);
}

//if the result is one entry long it is a value. Otherwise its a list of instructions
vector<Value *> Kernel::BuildReturnTree(BasicBlock *bb, vector<BasicBlock *> blocks)
{
    vector<Value *> result;
    Instruction *term = bb->getTerminator();
    if (ReturnInst *retInst = dyn_cast<ReturnInst>(term))
    {
        //so the block is a return, just return the value
        result.push_back(retInst->getReturnValue());
    }
    else if (BranchInst *brInst = dyn_cast<BranchInst>(term))
    {
        //we have a branch, if it is unconditional recurse on the target
        //otherwise recurse on all targets and build a select between the results
        //we assume the last entry in the vector is the result to be selected, so be careful on the order of insertion
        if (brInst->isConditional())
        {
            //we have a conditional so select between return vals
            //still need to check if successors are valid though
            Value *cond = brInst->getCondition();
            auto suc0 = brInst->getSuccessor(0);
            auto suc1 = brInst->getSuccessor(1);
            bool valid0 = find(blocks.begin(), blocks.end(), suc0) != blocks.end();
            bool valid1 = find(blocks.begin(), blocks.end(), suc1) != blocks.end();
            if (!(valid0 || valid1))
            {
                throw TikException("Tik Error: Branch instruction with no valid successors reached");
            }
            Value *c0 = NULL;
            Value *c1 = NULL;
            if (valid0) //if path 0 is valid we examine it
            {
                auto sub0 = BuildReturnTree(suc0, blocks);
                if (sub0.size() != 1)
                {
                    //we can just copy them
                    //otherwise this is a single value and should just be referenced
                    result.insert(result.end(), sub0.begin(), sub0.end());
                }
                c0 = sub0.back();
            }
            if (valid1) //if path 1 is valid we examine it
            {
                auto sub1 = BuildReturnTree(brInst->getSuccessor(1), blocks);
                if (sub1.size() != 1)
                {
                    //we can just copy them
                    //otherwise this is a single value and should just be referenced
                    result.insert(result.end(), sub1.begin(), sub1.end());
                }
                c1 = sub1.back();
            }
            if (valid0 && valid1) //if they are both valid we need to select between them
            {
                SelectInst *sInst = SelectInst::Create(cond, c0, c1);
                result.push_back(sInst);
            }
        }
        else
        {
            //we are not conditional so just recursef
            auto sub = BuildReturnTree(brInst->getSuccessor(0), blocks);
            result.insert(result.end(), sub.begin(), sub.end());
        }
    }
    else
    {
        throw TikException("Tik Error: Not Implemented");
    }
    if (result.size() == 0)
    {
        throw TikException("Tik Error: Return instruction tree must have at least one result");
    }
    return result;
}

void Kernel::ApplyMetadata()
{
    //first we will clean the current instructions
    for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
    {
        for (auto bi = fi->begin(); bi != fi->end(); bi++)
        {
            Instruction *inst = cast<Instruction>(bi);
            inst->setMetadata("dbg", NULL);
        }
    }

    //annotate the kernel functions
    MDNode *kernelNode = MDNode::get(TikModule->getContext(), MDString::get(TikModule->getContext(), Name));
    KernelFunction->setMetadata("KernelName", kernelNode);
    MemoryRead->setMetadata("KernelName", kernelNode);
    MemoryWrite->setMetadata("KernelName", kernelNode);
    for (auto global : GlobalMap)
    {
        global.second->setMetadata("KernelName", kernelNode);
    }
    //annotate the body
    MDNode *bodyNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikMetadata::Body))));
    for (auto body : Body)
    {
        cast<Instruction>(body->getFirstInsertionPt())->setMetadata("TikMetadata", bodyNode);
    }
    //annotate the terminus
    MDNode *termNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikMetadata::Terminus))));
    for (auto term : Termination)
    {
        cast<Instruction>(term->getFirstInsertionPt())->setMetadata("TikMetadata", termNode);
    }
    //annotate the conditional, has to happen after body since conditional is a part of the body
    MDNode *condNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikMetadata::Conditional))));
    for (auto cond : Conditional)
    {
        cast<Instruction>(cond->getFirstInsertionPt())->setMetadata("TikMetadata", condNode);
    }
}

void Kernel::Repipe()
{
    //remap the conditional to the exit
    for (auto ki = KernelFunction->begin(); ki != KernelFunction->end(); ki++)
    {
        auto c = cast<BasicBlock>(ki);
        auto cTerm = c->getTerminator();
        if (!cTerm)
        {
            continue;
        }
        int cSuc = cTerm->getNumSuccessors();
        for (int i = 0; i < cSuc; i++)
        {
            auto suc = cTerm->getSuccessor(i);
            if (suc->getParent() != KernelFunction)
            {
                cTerm->setSuccessor(i, Exit);
            }
        }
    }
}

void Kernel::SplitBlocks(set<BasicBlock *> &blocks)
{
    vector<BasicBlock *> toProcess;
    for (auto block : blocks)
    {
        toProcess.push_back(block);
    }

    while (toProcess.size() != 0)
    {
        BasicBlock *next = toProcess.back();
        toProcess.pop_back();
        for (auto bi = next->begin(); bi != next->end(); bi++)
        {
            if (auto ci = dyn_cast<CallBase>(bi))
            {
                if (!ci->isTerminator())
                {
                    Function *f = ci->getCalledFunction();
                    if (f && !f->empty())
                    {
                        int64_t id = GetBlockID(next);
                        auto spl = next->splitBasicBlock(ci->getNextNode());
                        SetBlockID(spl, id);
                        blocks.insert(spl);
                        toProcess.push_back(spl);
                    }
                }
            }
        }
    }
}

void Kernel::GetEntrances(set<BasicBlock *> &blocks)
{
    for (BasicBlock *block : blocks)
    {
        int id = GetBlockID(block);
        if (KernelMap.find(id) == KernelMap.end())
        {
            for (BasicBlock *pred : predecessors(block))
            {
                if (pred)
                {
                    //strange bug here that leaves a predecessor in place, even if we remove the parent
                    //not sure about the solution
                    if (blocks.find(pred) == blocks.end() && pred->getParent() != NULL)
                    {
                        Entrances.insert(block);
                    }
                }
            }
            //we also check the entry blocks
            Function *parent = block->getParent();
            BasicBlock *entry = &parent->getEntryBlock();
            if (block == entry)
            {
                //potential entrance
                bool extUse = false;
                for (auto user : parent->users())
                {
                    Instruction *ci = cast<Instruction>(user);
                    BasicBlock *parentBlock = ci->getParent();
                    if (blocks.find(parentBlock) == blocks.end())
                    {
                        extUse = true;
                        break;
                    }
                }
                if (extUse)
                {
                    Entrances.insert(block);
                }
            }
        }
    }
    if (Entrances.size() == 0)
    {
        throw TikException("Kernel Exception: tik requires a body entrance");
    }
}

std::string Kernel::GetHeaderDeclaration(void)
{
    std::string headerString = getCType(KernelFunction->getReturnType()) + " ";
    headerString += KernelFunction->getName();
    headerString += "(";
    int i = 0;
    for (auto ai = KernelFunction->arg_begin(); ai < KernelFunction->arg_end(); ai++)
    {
        if (i > 0)
        {
            headerString += ", ";
        }
        headerString += getCType(ai->getType());
        headerString += " arg";
        headerString += std::to_string(i);
        i++;
    }
    headerString += ");\n";
    return headerString;
}

void Kernel::SanityChecks()
{
    for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
    {
        BasicBlock *BB = cast<BasicBlock>(fi);
        int predCount = 0;
        for (auto pred : predecessors(BB))
        {
            predCount++;
        }
        if (predCount == 0)
        {
            if (BB != Init)
            {
                throw TikException("Tik Sanity Failure: No predecessors");
            }
        }
    }
}

void Kernel::GetExits(set<BasicBlock *> &blocks)
{
    int exitId = 0;
    // search for exit basic blocks
    set<BasicBlock *> coveredExits;
    for (auto block : blocks)
    {
        for (auto suc : successors(block))
        {
            if (blocks.find(suc) == blocks.end())
            {
                if (coveredExits.find(suc) == coveredExits.end())
                {
                    ExitTarget[exitId] = suc;
                    coveredExits.insert(suc);
                    ExitMap[block] = exitId++;
                }
            }
        }
    }
    if (exitId == 0)
    {
        throw TikException("Tik Error: tik found no kernel exits")
    }
    if (exitId != 1)
    {
        //removing this is exposing an llvm bug: corrupted double-linked list
        //we just won't support it for the moment
        //throw TikException("Tik Error: tik only supports single exit kernels")
    }
}

void Kernel::CopyGlobals()
{
    for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
    {
        for (auto bi = fi->begin(); bi != fi->end(); bi++)
        {
            Instruction *inst = cast<Instruction>(bi);
            if (!isa<CallBase>(inst))
            {
                for (int j = 0; j < inst->getNumOperands(); j++)
                {
                    Value *v = inst->getOperand(j);
                    if (GlobalVariable *gv = dyn_cast<GlobalVariable>(v))
                    {
                        Module *m = gv->getParent();
                        if (m != TikModule)
                        {
                            //its the wrong module
                            if (VMap.find(gv) == VMap.end())
                            {
                                //and not already in the vmap
                                Constant *initializer = NULL;
                                if (gv->hasInitializer())
                                {
                                    initializer = gv->getInitializer();
                                }
                                GlobalVariable *newVar = new GlobalVariable(*TikModule, gv->getValueType(), gv->isConstant(), gv->getLinkage(), initializer, gv->getName(), NULL, gv->getThreadLocalMode(), gv->getAddressSpace(), gv->isExternallyInitialized());
                                VMap[gv] = newVar;
                            }
                        }
                    }
                    else if (GlobalValue *gv = dyn_cast<GlobalValue>(v))
                    {
                        throw TikException("Tik Error: Non variable global reference");
                    }
                }
            }
        }
    }
}
