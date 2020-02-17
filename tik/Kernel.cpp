#include "tik/Kernel.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Print.h"
#include "tik/Exceptions.h"
#include "tik/InlineStruct.h"
#include "tik/Metadata.h"
#include "tik/Util.h"
#include "tik/tik.h"
#include <algorithm>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <queue>
using namespace llvm;
using namespace std;

static int KernelUID = 0;

set<string> reservedNames;

Kernel::Kernel(std::vector<int> basicBlocks, Module *M, string name)
{
    MemoryRead = NULL;
    MemoryWrite = NULL;
    Init = NULL;
    Conditional = NULL;
    Exit = NULL;
    if (name.empty())
    {
        Name = "Kernel_" + to_string(KernelUID++);
    }
    else
    {
        if (reservedNames.find(name) != reservedNames.end())
        {
            throw TikException("Kernel names must be unique!");
        }
        Name = name;
    }
    reservedNames.insert(Name);

    FunctionType *mainType = FunctionType::get(Type::getVoidTy(TikModule->getContext()), false);
    KernelFunction = Function::Create(mainType, GlobalValue::LinkageTypes::ExternalLinkage, Name, TikModule);
    Init = BasicBlock::Create(TikModule->getContext(), "Init", KernelFunction);
    Exit = BasicBlock::Create(TikModule->getContext(), "Exit", KernelFunction);

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

    //order should be getConditional (find the condition)
    //then find body/prequel
    //then find epilogue/termination
    //then we can go to exits/init/memory like normal

    //SplitBlocks(blocks);

    GetEntrances(blocks);

    GetConditional(blocks);

    GetPrequel(blocks);

    auto bodyPrequel = GetBodyPrequel(blocks);

    set<BasicBlock *> bblocks;
    for (auto block : blocks)
    {
        if (block != Conditional)
        {
            bblocks.insert(block);
        }
    }

    //for the moment I am going to skip the epilogue/termination and get the prequel stuff working
    //its only necessary for recursion anyway
    BuildBody(bblocks);

    BuildCondition();

    BuildExit();

    Remap();
    //might be fused
    Repipe();

    GetInitInsts();

    GetMemoryFunctions();

    MorphKernelFunction();

    ApplyMetadata();
}

tuple<set<BasicBlock *>, set<BasicBlock *>> Kernel::GetPrePostConditionBlocks(set<BasicBlock *> blocks, set<BasicBlock *> conditionalBlocks)
{
    set<BasicBlock *> exits;
    for (auto block : blocks)
    {
        auto term = block->getTerminator();
        for (int i = 0; i < term->getNumSuccessors(); i++)
        {
            auto suc = term->getSuccessor(i);
            if (find(blocks.begin(), blocks.end(), suc) == blocks.end())
            {
                exits.insert(suc);
            }
        }
        if (isa<ReturnInst>(term))
        {
            Function *f = block->getParent();
            for (auto user : f->users())
            {
                if (auto ui = dyn_cast<CallInst>(user))
                {
                    if (find(blocks.begin(), blocks.end(), ui->getParent()) == blocks.end())
                    {
                        exits.insert(block);
                        break;
                    }
                }
                else if (auto ui = dyn_cast<InvokeInst>(user))
                {
                    if (find(blocks.begin(), blocks.end(), ui->getParent()) == blocks.end())
                    {
                        exits.insert(block);
                        break;
                    }
                }
            }
        }
    }
    assert(exits.size() == 1);
    BasicBlock *currentBlock;

    set<BasicBlock *> pre;
    set<BasicBlock *> post;

    return {pre, post};
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
    if (Prequel.size() != 0)
    {
        for (auto b : Prequel)
        {
            j["Prequel"].push_back(GetStrings(b));
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
    if (Conditional != NULL)
    {
        j["Conditional"] = GetStrings(Conditional);
    }
    return j;
}

Kernel::~Kernel()
{
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

void Kernel::MorphKernelFunction()
{
    // localVMap is the new VMap for the new function
    llvm::ValueToValueMapTy localVMap;

    // capture all the input args our kernel function will need
    std::vector<llvm::Type *> inputArgs;
    for (auto inst : ExternalValues)
    {
        inputArgs.push_back(inst->getType());
    }

    // create our new function with input args and clone our basic blocks into it
    FunctionType *funcType = FunctionType::get(Type::getVoidTy(TikModule->getContext()), inputArgs, false);
    llvm::Function *newFunc = llvm::Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, KernelFunction->getName() + "_tmp", TikModule);
    for (int i = 0; i < ExternalValues.size(); i++)
    {
        ArgumentMap[newFunc->arg_begin() + i] = ExternalValues[i];
    }

    auto newInit = CloneBasicBlock(Init, localVMap, "", newFunc);
    localVMap[Conditional] = newInit;
    Init = newInit;

    vector<BasicBlock *> newBody;
    for (auto b : Body)
    {
        auto cb = CloneBasicBlock(b, localVMap, "", newFunc);
        newBody.push_back(cb);
        localVMap[b] = cb;
    }
    Body = newBody;
    vector<BasicBlock *> newPrequel;
    for (auto b : Prequel)
    {
        auto cb = CloneBasicBlock(b, localVMap, "", newFunc);
        newPrequel.push_back(cb);
        localVMap[b] = cb;
    }
    Prequel = newPrequel;
    vector<BasicBlock *> newEpilogue;
    vector<BasicBlock *> newTermination;
    for (auto b : Termination)
    {
        auto cb = CloneBasicBlock(b, localVMap, "", newFunc);
        newTermination.push_back(cb);
        localVMap[b] = cb;
    }
    Termination = newTermination;
    auto exitCloned = CloneBasicBlock(Exit, localVMap, "", newFunc);
    localVMap[Exit] = exitCloned;
    Exit = exitCloned;
    auto concCloned = CloneBasicBlock(Conditional, localVMap, "", newFunc);
    localVMap[Conditional] = concCloned;
    Conditional = concCloned;
    // remove the old function from the parent but do not erase it
    KernelFunction->removeFromParent();
    newFunc->setName(KernelFunction->getName());
    KernelFunction = newFunc;

    // for each input instruction, store them into one of our global pointers
    // GlobalMap already contains the input arg->global pointer relationships we need
    std::set<llvm::Value *> coveredGlobals;
    std::set<llvm::StoreInst *> newStores;
    IRBuilder<> initBuilder(Init);
    for (int i = 0; i < ExternalValues.size(); i++)
    {
        if (GlobalMap.find(ExternalValues[i]) != GlobalMap.end())
        {
            if (GlobalMap[ExternalValues[i]] == NULL)
            {
                throw TikException("External Value not found in GlobalMap.");
            }
            coveredGlobals.insert(GlobalMap[ExternalValues[i]]);
            auto b = initBuilder.CreateStore(KernelFunction->arg_begin() + i, GlobalMap[ExternalValues[i]]);
            MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikSynthetic::Store))));
            b->setMetadata("TikSynthetic", tikNode);
            newStores.insert(b);
        }
    }

    // look through the body for pointer references
    // every time we see a global reference who is not written to in MemRead, not stored to in Init, store to it in body

    // every time we use a global pointer in the body that is not in MemWrite, store to it
    for (Function::iterator bb = newFunc->begin(), be = newFunc->end(); bb != be; bb++)
    {
        for (BasicBlock::iterator BI = bb->begin(), BE = bb->end(); BI != BE; ++BI)
        {
            Instruction *inst = cast<Instruction>(BI);
            for (auto pair : GlobalMap)
            {
                if (localVMap.find(pair.first) != localVMap.end() && llvm::cast<Instruction>(localVMap[pair.first]) == inst)
                {
                    if (coveredGlobals.find(pair.second) == coveredGlobals.end())
                    {
                        IRBuilder<> builder(inst->getNextNode());
                        Constant *constant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0);
                        auto a = builder.CreateGEP(inst->getType(), GlobalMap[pair.first], constant);
                        auto b = builder.CreateStore(inst, a);
                        MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikSynthetic::Store))));
                        b->setMetadata("TikSynthetic", tikNode);
                    }
                }
            }
        }
    }

    // now create the branches between basic blocks
    // init->loop (unconditional)
    auto a = initBuilder.CreateBr(Conditional);
    // loop->body or loop->exit (conditional)
    // body->loop (unconditional)

    // Now find all calls to the embedded kernel functions in the body, if any, and change their arguments to the new ones
    std::map<Argument *, Value *> embeddedCallArgs;
    for (auto b : Body)
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
                else // must be a kernel function call
                {
                    bool found = false;
                    auto calledFunc = callInst->getCalledFunction();
                    auto subK = KfMap[calledFunc];
                    if (subK)
                    {
                        for (auto sarg = calledFunc->arg_begin(); sarg < calledFunc->arg_end(); sarg++)
                        {
                            for (auto b : Body)
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
                            Argument *arg = cast<Argument>(callInst->getArgOperand(k));
                            auto asdf = embeddedCallArgs[arg];
                            callInst->setArgOperand(k, asdf);
                        }
                    }
                }
            }
        }
    }

    // finally, remap our instructions for the new function
    for (Function::iterator BB = KernelFunction->begin(), E = KernelFunction->end(); BB != E; ++BB)
    {
        for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI)
        {
            Instruction *inst = cast<Instruction>(BI);
            RemapInstruction(inst, localVMap, llvm::RF_None);
        }
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
    for (auto cond : conditions)
    {
        bool exRecurses = false;
        bool exExit = false;
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
                if(term->getNumSuccessors() == 0)
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
            if(recurses && exit)
            {
                //this did both which implies that it can't be the condition
                continue;
            }
            if(recurses)
            {
                exRecurses = true;
            }
            if(exit)
            {
                exExit = true;
            }
        }

        if(exExit && exRecurses)
        {
            validConditions.insert(cond);
        }
    }

    if (validConditions.size() != 1)
    {
        throw TikException("Only supports single condition kernels");
    }

    for (auto b : validConditions)
    {
        Conditional = b;
    }
}

tuple<set<BasicBlock *>, set<BasicBlock *>> Kernel::GetBodyPrequel(set<BasicBlock *> blocks)
{
    set<BasicBlock *> body;
    set<BasicBlock *> prequel;
    set<BasicBlock *> entrances;
    for (BasicBlock *block : blocks)
    {
        for (BasicBlock *pred : predecessors(block))
        {
            if (pred)
            {
                if (blocks.find(pred) == blocks.end() && pred != Conditional)
                {
                    entrances.insert(block);
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
                if (blocks.find(parentBlock) == blocks.end() && parentBlock != Conditional)
                {
                    extUse = true;
                    break;
                }
            }
            if (extUse)
            {
                entrances.insert(block);
            }
        }
    }
    if (entrances.size() != 1)
    {
        throw TikException("Kernel Exception: tik only supports single entrance kernels");
    }

    //now that we have the entrances we can do dijkstras
    Function *parentFunc;
    parentFunc = Conditional->getParent();

    bool isRecursive = false;

    for (auto block : blocks)
    {
        for (auto bi = block->begin(); bi != block->end(); bi++)
        {
            if (auto ci = dyn_cast<CallBase>(bi))
            {
                Function *f = ci->getCalledFunction();
                if (f == parentFunc)
                {
                    isRecursive = true;
                    break;
                }
            }
        }
        if (isRecursive)
        {
            break;
        }
    }

    for (auto ent : entrances)
    {
        EnterTarget = ent;
    }

    return {body, prequel};
}

void Kernel::BuildCondition()
{
    Conditional = CloneBasicBlock(Conditional, VMap, "", KernelFunction);
}
void Kernel::BuildBody(std::set<llvm::BasicBlock *> blocks)
{
    for (auto block : blocks)
    {
        int64_t id = GetBlockID(block);
        if (KernelMap.find(id) != KernelMap.end())
        {
            //this belongs to a subkernel
            Kernel *nestedKernel = KernelMap[id];
            if (nestedKernel->EnterTarget == block)
            {
                //we need to make a unique block for each entrance (there is currently only one)
                std::vector<llvm::Value *> inargs;
                for (auto ai = nestedKernel->KernelFunction->arg_begin(); ai < nestedKernel->KernelFunction->arg_end(); ai++)
                {
                    inargs.push_back(cast<Value>(ai));
                }
                BasicBlock *intermediateBlock = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
                IRBuilder<> intBuilder(intermediateBlock);
                intBuilder.CreateCall(nestedKernel->KernelFunction, inargs);
                intBuilder.CreateBr(cast<BasicBlock>(nestedKernel->ExitTarget[0]));
                VMap[block] = intermediateBlock;
                Body.push_back(intermediateBlock);
            }
        }
        else
        {
            auto cb = CloneBasicBlock(block, VMap, "", KernelFunction);
            vector<CallInst *> toInline;
            for (auto bi = cb->begin(); bi != cb->end(); bi++)
            {
                if (CallInst *ci = dyn_cast<CallInst>(bi))
                {
                    toInline.push_back(ci);
                }
            }
            for (auto ci : toInline)
            {
                auto calledFunc = ci->getCalledFunction();
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
                        BasicBlock *suffix = cb->splitBasicBlock(ci);
                        Body.push_back(suffix);
                        //first create the phi block which is the entry point
                        BasicBlock *phiBlock = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
                        Body.push_back(phiBlock);
                        IRBuilder<> phiBuilder(phiBlock);
                        //first phi we need is the number of exit paths
                        vector<BasicBlock *> funcUses;
                        for (auto user : calledFunc->users())
                        {
                            if (CallInst *callUse = dyn_cast<CallInst>(user))
                            {
                                BasicBlock *parent = callUse->getParent();
                                if (find(blocks.begin(), blocks.end(), parent) != blocks.end())
                                {
                                    funcUses.push_back(parent);
                                }
                            }
                            else
                            {
                                throw TikException("Only expected callInst");
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
                        Body.push_back(returnBlock);
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
                                ri->removeFromParent(); //we also remove the return here because we don't need it
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
                        ci->removeFromParent();

                        InlinedFunctions.push_back(currentStruct); //finally add it to the already inlined functions
                    }
                    else
                    {
                        //we already inlined this one and need to add the appropriate entries to teh argnodes and the switch instruction
                        throw TikException("Not Implemented");
                    }
                }
            }
            Body.push_back(cb);
            VMap[block] = cb;
        }
    }
}
void Kernel::BuildPrequel(std::set<llvm::BasicBlock *> blocks)
{
    for (auto block : blocks)
    {
        int64_t id = GetBlockID(block);
        if (KernelMap.find(id) != KernelMap.end())
        {
            //this belongs to a subkernel
            Kernel *nestedKernel = KernelMap[id];
            if (nestedKernel->EnterTarget == block)
            {
                //we need to make a unique block for each entrance (there is currently only one)
                std::vector<llvm::Value *> inargs;
                for (auto ai = nestedKernel->KernelFunction->arg_begin(); ai < nestedKernel->KernelFunction->arg_end(); ai++)
                {
                    inargs.push_back(cast<Value>(ai));
                }
                BasicBlock *intermediateBlock = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
                IRBuilder<> intBuilder(intermediateBlock);
                intBuilder.CreateCall(nestedKernel->KernelFunction, inargs);
                intBuilder.CreateBr(cast<BasicBlock>(nestedKernel->ExitTarget[0]));
                VMap[block] = intermediateBlock;
                Body.push_back(intermediateBlock);
            }
        }
        else
        {
            auto cb = CloneBasicBlock(block, VMap, "", KernelFunction);
            vector<CallInst *> toInline;
            for (auto bi = cb->begin(); bi != cb->end(); bi++)
            {
                if (CallInst *ci = dyn_cast<CallInst>(bi))
                {
                    toInline.push_back(ci);
                }
            }
            for (auto ci : toInline)
            {
                auto calledFunc = ci->getCalledFunction();
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
                        BasicBlock *suffix = cb->splitBasicBlock(ci);
                        Body.push_back(suffix);
                        //first create the phi block which is the entry point
                        BasicBlock *phiBlock = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
                        Body.push_back(phiBlock);
                        IRBuilder<> phiBuilder(phiBlock);
                        //first phi we need is the number of exit paths
                        vector<BasicBlock *> funcUses;
                        for (auto user : calledFunc->users())
                        {
                            if (CallInst *callUse = dyn_cast<CallInst>(user))
                            {
                                BasicBlock *parent = callUse->getParent();
                                if (find(blocks.begin(), blocks.end(), parent) != blocks.end())
                                {
                                    funcUses.push_back(parent);
                                }
                            }
                            else
                            {
                                throw TikException("Only expected callInst");
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
                        Body.push_back(returnBlock);
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
                                ri->removeFromParent(); //we also remove the return here because we don't need it
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
                        ci->removeFromParent();

                        InlinedFunctions.push_back(currentStruct); //finally add it to the already inlined functions
                    }
                    else
                    {
                        //we already inlined this one and need to add the appropriate entries to teh argnodes and the switch instruction
                        throw TikException("Not Implemented");
                    }
                }
            }
            Body.push_back(cb);
            VMap[block] = cb;
        }
    }
}

void Kernel::GetInitInsts()
{
    for (auto bb : Body)
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
                    if (std::find(Body.begin(), Body.end(), parentBlock) == Body.end())
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
                                if (VMap[sExtVal] == NULL)
                                {
                                    if (find(ExternalValues.begin(), ExternalValues.end(), sExtVal) == ExternalValues.end())
                                    {
                                        ExternalValues.push_back(sExtVal);
                                    }
                                }
                                else
                                {
                                    throw TikException("Unimplemented");
                                }
                            }
                        }
                        else
                        {
                            throw TikException("Unimplemented");
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
    for (auto bb : Body)
    {
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
    for (BasicBlock::iterator BI = Conditional->begin(), BE = Conditional->end(); BI != BE; ++BI)
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
    set<Value *> loadValues;
    set<Value *> storeValues;
    for (LoadInst *load : loadInst)
    {
        Value *loadVal = load->getPointerOperand();
        loadValues.insert(loadVal);
        Type *loadType = loadVal->getType();
    }
    for (StoreInst *store : storeInst)
    {
        Value *storeVal = store->getPointerOperand();
        storeValues.insert(storeVal);
    }

    // create MemoryRead, MemoryWrite functions
    FunctionType *funcType = FunctionType::get(Type::getInt32Ty(TikModule->getContext()), Type::getInt32Ty(TikModule->getContext()), false);
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
        Constant *constant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0);
        auto a = loadBuilder.CreateGEP(lVal->getType(), GlobalMap[lVal], constant);
        auto b = loadBuilder.CreateLoad(a);
        Instruction *converted = cast<Instruction>(loadBuilder.CreatePtrToInt(b, Type::getInt32Ty(TikModule->getContext())));
        Constant *indexConstant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), i++);
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
    Instruction *loadRet = cast<ReturnInst>(loadBuilder.CreateRet(priorValue));

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
        Constant *constant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0);
        auto a = storeBuilder.CreateGEP(sVal->getType(), GlobalMap[sVal], constant);
        auto b = storeBuilder.CreateLoad(a);
        Instruction *converted = cast<Instruction>(storeBuilder.CreatePtrToInt(b, Type::getInt32Ty(TikModule->getContext())));
        Constant *indexConstant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), i++);
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
    Instruction *storeRet = cast<ReturnInst>(storeBuilder.CreateRet(priorValue));

    // find instructions in body block not belonging to parent kernel
    vector<Instruction *> toRemove;
    for (auto bb : Body)
    {
        for (BasicBlock::iterator BI = bb->begin(), BE = bb->end(); BI != BE; ++BI)
        {
            Instruction *inst = cast<Instruction>(BI);
            IRBuilder<> builder(inst);
            if (LoadInst *newInst = dyn_cast<LoadInst>(inst))
            {
                auto memCall = builder.CreateCall(MemoryRead, loadMap[newInst->getPointerOperand()]);
                auto casted = cast<Instruction>(builder.CreateIntToPtr(memCall, newInst->getPointerOperand()->getType()));
                MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikSynthetic::Cast))));
                casted->setMetadata("TikSynthetic", tikNode);
                auto newLoad = builder.CreateLoad(casted);
                newInst->replaceAllUsesWith(newLoad);
                toRemove.push_back(newInst);
            }
            else if (StoreInst *newInst = dyn_cast<StoreInst>(inst))
            {
                auto memCall = builder.CreateCall(MemoryWrite, storeMap[newInst->getPointerOperand()]);
                auto casted = cast<Instruction>(builder.CreateIntToPtr(memCall, newInst->getPointerOperand()->getType()));
                MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikSynthetic::Cast))));
                casted->setMetadata("TikSynthetic", tikNode);
                auto newStore = builder.CreateStore(newInst->getValueOperand(), casted); //storee);
                toRemove.push_back(newInst);
            }
        }
    }

    // find instructions in Conditional block not belonging to parent
    for (BasicBlock::iterator BI = Conditional->begin(), BE = Conditional->end(); BI != BE; ++BI)
    {
        Instruction *inst = cast<Instruction>(BI);
        IRBuilder<> builder(inst);
        if (LoadInst *newInst = dyn_cast<LoadInst>(inst))
        {
            auto memCall = builder.CreateCall(MemoryRead, loadMap[newInst->getPointerOperand()]);
            auto casted = builder.CreateIntToPtr(memCall, newInst->getPointerOperand()->getType());
            auto newLoad = builder.CreateLoad(casted);
            newInst->replaceAllUsesWith(newLoad);
            VMap[inst] = newLoad;
            toRemove.push_back(inst);
        }
        else if (StoreInst *newInst = dyn_cast<StoreInst>(inst))
        {
            auto memCall = builder.CreateCall(MemoryWrite, storeMap[newInst->getPointerOperand()]);
            auto casted = builder.CreateIntToPtr(memCall, newInst->getPointerOperand()->getType());
            auto newStore = builder.CreateStore(newInst->getValueOperand(), casted);
            toRemove.push_back(inst);
        }
    }

    // remove the instructions that don't belong
    for (auto inst : toRemove)
    {
        inst->removeFromParent();
    }
}

void Kernel::BuildExit()
{
    int exitId = 0;
    // search for exit basic blocks
    auto term = Conditional->getTerminator();
    int sucCount = term->getNumSuccessors();
    for (int i = 0; i < sucCount; i++)
    {
        auto suc = term->getSuccessor(i);
        if (find(Body.begin(), Body.end(), VMap[suc]) == Body.end())
        {
            ExitTarget[exitId++] = suc;
        }
    }

    IRBuilder<> exitBuilder(Exit);
    auto a = exitBuilder.CreateRetVoid();
    /*
    set<BasicBlock *> exits;
    for (auto block : Body)
    {
        Instruction *term = block->getTerminator();
        if (BranchInst *brInst = dyn_cast<BranchInst>(term))
        {
            // put the successors of the exit block in a unique-entry vector
            for (unsigned int i = 0; i < brInst->getNumSuccessors(); i++)
            {
                BasicBlock *succ = brInst->getSuccessor(i);
                if (find(Body.begin(), Body.end(), succ) == Body.end())
                {
                    exits.insert(succ);
                    ExitTarget[exitId++] = succ;
                }
            }
        }
        else if (ReturnInst *rInst = dyn_cast<ReturnInst>(term))
        {
            Function *parentFunction = block->getParent();
            vector<BasicBlock *> internalUse;
            vector<BasicBlock *> externalUse;
            for (auto user : parentFunction->users())
            {
                if (CallInst *ci = dyn_cast<CallInst>(user))
                {
                    BasicBlock *parent = ci->getParent();
                    if (find(Body.begin(), Body.end(), parent) == Body.end())
                    {
                        externalUse.push_back(parent);
                    }
                    else
                    {
                        internalUse.push_back(parent);
                    }
                }
                else
                {
                    throw TikException("Function not called from callinst. Unexpected behavior.");
                }
            }
            if (internalUse.size() != 0 && externalUse.size() == 0)
            {
                //use is internal so ignore
            }
            else if (internalUse.size() == 0 && externalUse.size() != 0)
            {
                for (auto a : externalUse)
                {
                    exits.insert(a);
                    ExitTarget[exitId++] = a;
                }
            }
            else if (internalUse.size() != 0 && externalUse.size() != 0)
            {
                throw TikException("Function called both externally and internally. Unimplemented.")
            }
        }
        else if (SwitchInst *sw = dyn_cast<SwitchInst>(term))
        {
            for (unsigned int i = 0; i < sw->getNumSuccessors(); i++)
            {
                BasicBlock *succ = sw->getSuccessor(i);
                if (find(Body.begin(), Body.end(), succ) == Body.end())
                {
                    exits.insert(succ);
                    ExitTarget[exitId++] = succ;
                }
            }
        }
        else
        {
            throw TikException("Not Implemented");
        }
    }

    // we should have exactly one successor to our tik representation because we assume there are no embedded loops
    if (exits.size() != 1)
    {
        throw TikException("Kernel Exception: kernels must have one exit");
    }
    assert(exits.size() == 1);
    //ExitTarget[0] = exits[0];*/
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
            if (brInst->getNumSuccessors() != 2)
            {
                //sanity check
                throw TikException("Unexpected number of brInst successors");
            }
            auto suc0 = brInst->getSuccessor(0);
            auto suc1 = brInst->getSuccessor(1);
            bool valid0 = find(blocks.begin(), blocks.end(), suc0) != blocks.end();
            bool valid1 = find(blocks.begin(), blocks.end(), suc1) != blocks.end();
            if (!(valid0 || valid1))
            {
                throw TikException("Branch instruction with no valid successors reached");
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
        throw TikException("Not Implemented");
    }
    if (result.size() == 0)
    {
        throw TikException("Return instruction tree must have at least one result");
    }
    return result;
}

void Kernel::ApplyMetadata()
{
    MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikMetadata::KernelFunction))));
    MDNode *writeNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikMetadata::MemoryRead))));
    MDNode *readNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikMetadata::MemoryWrite))));
    KernelFunction->setMetadata("TikFunction", tikNode);
    MemoryRead->setMetadata("TikFunction", readNode);
    MemoryWrite->setMetadata("TikFunction", writeNode);
}

void Kernel::Repipe()
{
    //remap the body stuff to the conditional
    for (auto b : Body)
    {
        auto term = b->getTerminator();
        int sucCount = term->getNumSuccessors();
        for (int i = 0; i < sucCount; i++)
        {
            auto suc = term->getSuccessor(i);
            if (find(Body.begin(), Body.end(), suc) == Body.end())
            {
                term->setSuccessor(i, Conditional);
            }
        }
    }

    //remap the conditional to the exit
    auto cTerm = Conditional->getTerminator();
    int cSuc = cTerm->getNumSuccessors();
    for (int i = 0; i < cSuc; i++)
    {
        auto suc = cTerm->getSuccessor(i);
        if (find(Body.begin(), Body.end(), suc) == Body.end())
        {
            cTerm->setSuccessor(i, Exit);
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

void Kernel::GetPrequel(set<BasicBlock *> &blocks)
{
    queue<BasicBlock *> toProcess;
    set<BasicBlock *> pushedBlocks;
    for (auto b : Entrances)
    {
        if (b != Conditional)
        {
            toProcess.push(b);
            pushedBlocks.insert(b);
        }
    }

    while (toProcess.size() != 0)
    {
        BasicBlock *processing = toProcess.front();
        toProcess.pop();
        for (auto b : successors(processing))
        {
            if (b != Conditional && pushedBlocks.find(b) == pushedBlocks.end())
            {
                toProcess.push(b);
                pushedBlocks.insert(b);
            }
        }
        Prequel.push_back(processing);
    }
}

void Kernel::GetEntrances(set<BasicBlock *> &blocks)
{
    for (BasicBlock *block : blocks)
    {
        for (BasicBlock *pred : predecessors(block))
        {
            if (pred)
            {
                if (blocks.find(pred) == blocks.end())
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
    if (Entrances.size() != 1)
    {
        throw TikException("Kernel Exception: tik only supports single entrance kernels");
    }
}