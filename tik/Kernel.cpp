#include "tik/Kernel.h"
#include "tik/Exceptions.h"
#include "tik/Metadata.h"
#include "tik/Util.h"
#include "tik/tik.h"
#include <algorithm>
#include <iostream>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <vector>
using namespace llvm;
using namespace std;

static int KernelUID = 0;

Kernel::Kernel(std::vector<int> basicBlocks, Module *M)
{
    MemoryRead = NULL;
    MemoryWrite = NULL;
    Init = NULL;
    Conditional = NULL;
    Exit = NULL;
    Name = "Kernel_" + to_string(KernelUID++);
    FunctionType *mainType = FunctionType::get(Type::getVoidTy(TikModule->getContext()), false);
    KernelFunction = Function::Create(mainType, GlobalValue::LinkageTypes::ExternalLinkage, Name, TikModule);
    Init = BasicBlock::Create(TikModule->getContext(), "Init", KernelFunction);
    Exit = BasicBlock::Create(TikModule->getContext(), "Exit", KernelFunction);

    vector<BasicBlock *> blocks;
    for (Module::iterator F = M->begin(), E = M->end(); F != E; ++F)
    {
        for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
        {
            BasicBlock *b = cast<BasicBlock>(BB);
            string blockName = b->getName();
            uint64_t id = std::stoul(blockName.substr(7));
            if (find(basicBlocks.begin(), basicBlocks.end(), id) != basicBlocks.end())
            {
                blocks.push_back(b);
            }
        }
    }

    GetBodyInsts(blocks);

    GetExits();

    GetLoopInsts();

    GetInitInsts();

    GetMemoryFunctions();

    CreateExitBlock();

    MorphKernelFunction();

    ApplyMetadata();
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
        j["Loop"] = GetStrings(Conditional);
    }
    return j;
}

Kernel::~Kernel()
{
    if (MemoryRead != NULL)
    {
        delete MemoryRead;
    }
    if (MemoryWrite != NULL)
    {
        delete MemoryWrite;
    }
    delete Conditional;
    delete KernelFunction;
}

void Kernel::Remap()
{
    for (Function::iterator BB = KernelFunction->begin(), E = KernelFunction->end(); BB != E; ++BB)
    {
        for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI)
        {
            Instruction *inst = cast<Instruction>(BI);
            RemapInstruction(inst, VMap, llvm::RF_IgnoreMissingLocals);
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

    Init = CloneBasicBlock(Init, localVMap, "", newFunc);
    vector<BasicBlock *> newBody;
    for (auto b : Body)
    {
        auto cb = CloneBasicBlock(b, localVMap, "", newFunc);
        newBody.push_back(cb);
        localVMap[b] = cb;
    }
    Body = newBody;
    Exit = CloneBasicBlock(Exit, localVMap, "", newFunc);
    Conditional = CloneBasicBlock(Conditional, localVMap, "", newFunc);
    // remove the old function from the parent but do not erase it
    KernelFunction->removeFromParent();
    newFunc->setName(KernelFunction->getName());
    KernelFunction = newFunc;

    // for each input instruction, store them into one of our global pointers
    // GlobalMap already contains the input arg->global pointer relationships we need
    std::set<llvm::StoreInst *> newStores;
    for (int i = 0; i < ExternalValues.size(); i++)
    {
        IRBuilder<> builder(Init);
        if (GlobalMap.find(ExternalValues[i]) != GlobalMap.end())
        {
            auto b = builder.CreateStore(KernelFunction->arg_begin() + i, GlobalMap[ExternalValues[i]]);
            MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikSynthetic::Store))));
            b->setMetadata("TikSynthetic", tikNode);
            newStores.insert(b);
        }
    }

    // now create the branches between basic blocks
    // init->loop (unconditional)
    IRBuilder<> initBuilder(Init);
    auto a = initBuilder.CreateBr(Conditional);
    // loop->body or loop->exit (conditional)
    IRBuilder<> loopBuilder(Conditional);
    if (VMap[LoopCondition] == NULL)
    {
        throw TikException("Condition not found in VMap");
    }

    auto b = loopBuilder.CreateCondBr(VMap[LoopCondition], cast<BasicBlock>(VMap[EnterTarget]), Exit);
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
            //PrintVal(inst);
            RemapInstruction(inst, localVMap, llvm::RF_IgnoreMissingLocals);
        }
    }
}

void Kernel::GetLoopInsts()
{
    Conditional = BasicBlock::Create(TikModule->getContext(), "Loop", KernelFunction);
    vector<Instruction *> result;

    // identify all exit blocks
    vector<BasicBlock *> exits;
    for (BasicBlock *block : Body)
    {
        bool exit = false;
        Instruction *term = block->getTerminator();
        for (int i = 0; i < term->getNumSuccessors(); i++)
        {
            BasicBlock *succ = term->getSuccessor(i);
            if (find(Body.begin(), Body.end(), succ) == Body.end())
            {
                // it isn't in the kernel
                exit = true;
            }
        }
        if (exit)
        {
            exits.push_back(block);
        }
    }

    //now that we have the exits we can get the conditional logic for them
    if (exits.size() != 1)
    {
        throw TikException("Kernel Exception: tik only supports single exit kernels");
    }
    std::vector<Instruction *> conditions;
    for (BasicBlock *exit : exits)
    {
        Instruction *inst = exit->getTerminator();
        if (BranchInst *brInst = dyn_cast<BranchInst>(inst))
        {
            if (brInst->isConditional())
            {
                Instruction *condition = cast<Instruction>(brInst->getCondition());
                conditions.push_back(condition);
            }
            else
            {
                // this means the block's terminator is the wrong, so the input bitcode is flawed
                throw TikException("Not Implemented");
            }
        }
        else
        {
            // same story
            throw TikException("Not Implemented");
        }
    }
    // we should only find one condition branch, because we assume that a detected kernel has no embedded loops
    if (conditions.size() != 1)
    {
        throw TikException("Kernel Exception: tik only supports single condition kernels");
    }
    LoopCondition = conditions[0];

    //first we look for all potential users of the condition
    vector<BasicBlock *> redirectedBlocks;
    for (auto loopUse : LoopCondition->users())
    {
        auto priorBranch = cast<Instruction>(loopUse);
        if (BranchInst *bi = dyn_cast<BranchInst>(priorBranch))
        {
            if (bi->isConditional())
            {
                BasicBlock *brTarget = NULL;
                for (auto target : bi->successors())
                {
                    if (find(Body.begin(), Body.end(), target) != Body.end())
                    {
                        assert(brTarget == NULL);
                        brTarget = target;
                    }
                }
                BasicBlock *b = bi->getParent();
                IRBuilder<> tBuilder(b);
                tBuilder.CreateBr(brTarget);
                bi->removeFromParent();
                redirectedBlocks.push_back(b);
            }
            else
            {
                throw TikException("Loop branch is unconditional, unexpected");
            }
        }
        else
        {
            throw TikException("Tik conitions must end with branches");
        }
    }

    //now that it has been rerouted we need to reroute the actual kernel
    for (auto b : Body)
    {
        auto term = b->getTerminator();
        if (BranchInst *bi = dyn_cast<BranchInst>(term))
        {
            int sucCount = bi->getNumSuccessors();
            for (int i = 0; i < sucCount; i++)
            {
                auto suc = bi->getSuccessor(i);
                if (find(redirectedBlocks.begin(), redirectedBlocks.end(), suc) != redirectedBlocks.end())
                {
                    bi->setSuccessor(i, Conditional);
                }
            }
        }
        else if (SwitchInst *sw = dyn_cast<SwitchInst>(term))
        {
            int sucCount = sw->getNumSuccessors();
            for (int i = 0; i < sucCount; i++)
            {
                auto suc = sw->getSuccessor(i);
                if (find(redirectedBlocks.begin(), redirectedBlocks.end(), suc) != redirectedBlocks.end())
                {
                    sw->setSuccessor(i, Conditional);
                }
            }
        }
        else
        {
            throw TikException("Expected only branch instructions. Unimplemented");
        }
    }

    // now check if the condition instruction's users are eligible instructions themselves for out body block, and throw it out after
    while (conditions.size() != 0)
    {
        Instruction *check = conditions.back();
        conditions.pop_back();
        bool eligible = true;
        // a value is eligible only if all of its users are in the loop
        for (auto user : check->users())
        {
            Instruction *usr = cast<Instruction>(user);
            bool found = false;
            for (auto i : result)
            {
                if (i->isIdenticalTo(usr))
                {
                    found = true;
                }
            }
            if (!found && result.size()) // we haven't already done it
            {
                if (BranchInst *br = dyn_cast<BranchInst>(usr))
                {
                }
                else
                {
                    eligible = false;
                }
            }
        }
        if (eligible)
        {
            result.push_back(check);

            /** WHY DO WE NEED THE FOLLOWING EXTRA LOGIC **/

            // if we are eligible we can then check all ops
            int opCount = check->getNumOperands();
            for (int i = 0; i < opCount; i++)
            {
                Value *opValue = check->getOperand(i);
                if (Instruction *op = dyn_cast<Instruction>(opValue))
                {
                    // assuming it is an instruction we should check it
                    conditions.push_back(op);
                }
            }
        }
    }

    // now clone the old instructions into the body block
    reverse(result.begin(), result.end());
    auto condList = &Conditional->getInstList();
    for (auto cond : result)
    {
        Instruction *cl = cond->clone();
        cond->replaceAllUsesWith(cl);
        cond->removeFromParent();
        VMap[cond] = cl;
        condList->push_back(cl);
    }
}

void Kernel::GetBodyInsts(vector<BasicBlock *> blocks)
{
    // search blocks for BBs whose predecessors are not in blocks, this will be the entry block
    std::vector<Instruction *> result;
    vector<BasicBlock *> entrances;
    for (BasicBlock *block : blocks)
    {
        for (BasicBlock *pred : predecessors(block))
        {
            if (pred)
            {
                if (find(blocks.begin(), blocks.end(), pred) == blocks.end())
                {
                    entrances.push_back(block);
                }
            }
        }
    }
    if (entrances.size() != 1)
    {
        throw TikException("Kernel Exception: tik only supports single entrance kernels");
    }
    BasicBlock *currentBlock = entrances[0];
    EnterTarget = entrances[0];
    // next, find all the instructions in the entrance block bath who call functions, and make our own references to them
    int id = 0;
    vector<BasicBlock *> potentialBlocks;
    for (auto b : blocks)
    {
        string blockName = b->getName();
        uint64_t id = std::stoul(blockName.substr(7));
        if (KernelMap.find(id) == KernelMap.end()) //only map blocks that haven't already been mapped
        {
            //this hasn't already been mapped
            auto cb = CloneBasicBlock(b, VMap, Name, KernelFunction);
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
                    //needs to be inlined
                    BasicBlock *suffix = cb->splitBasicBlock(ci);
                    Body.push_back(suffix);
                    //first create the phi block
                    BasicBlock *phiBlock = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
                    Body.push_back(phiBlock);
                    IRBuilder<> phiBuilder(phiBlock);
                    //first phi we need is the exit path, to know that we need the total entries
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
                    auto branchPhi = phiBuilder.CreatePHI(Type::getInt8Ty(TikModule->getContext()), funcUses.size());
                    int i = 0;
                    for(auto func : funcUses)
                    {
                        branchPhi->addIncoming(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), i++) , func);
                    }
                    //this phi needs to be filled out
                    int argIndex = 0;
                    for(auto ai = calledFunc->arg_begin(); ai != calledFunc->arg_end(); ai++)
                    {
                        Argument *arg = cast<Argument>(ai);
                        Value *passedValue = ci->getOperand(argIndex);
                        auto argPhi = phiBuilder.CreatePHI(arg->getType(), funcUses.size());
                        argPhi->addIncoming(passedValue, cb);
                        VMap[arg] = argPhi;
                        argIndex++;
                    }
                    //we also need to do this for all the call args
                    phiBuilder.CreateBr(&calledFunc->getEntryBlock()); //needs phis before

                    auto returnBlock = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
                    Body.push_back(returnBlock);
                    int returnCount = 0;
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
                            ri->removeFromParent();
                            returnCount++;
                            IRBuilder<> fIteratorBuilder(fBasicBlock);
                            fIteratorBuilder.CreateBr(returnBlock);
                        }
                    }

                    IRBuilder<> returnBuilder(returnBlock);
                    if (calledFunc->getReturnType() != Type::getVoidTy(TikModule->getContext()))
                    {
                        auto returnPhi = returnBuilder.CreatePHI(calledFunc->getReturnType(), returnCount);
                        int l = 0;
                        for (auto pair : returnMap)
                        {
                            returnPhi->addIncoming(pair.second, pair.first);
                        }
                        VMap[ci] = returnPhi;
                    }
                    auto branchSwitch = returnBuilder.CreateSwitch(branchPhi, suffix, funcUses.size());

                    //we now need to fill out the switch instructoin

                    //redirect the first block
                    BranchInst *priorBranch = cast<BranchInst>(cb->getTerminator());
                    priorBranch->setSuccessor(0, phiBlock);
                    ci->removeFromParent();
                }
            }
            Body.push_back(cb);
            VMap[b] = cb;
        }
    }
    Remap();
    //now that we mapped we need to move all kernel calls into an actual function
    for (auto b : Body)
    {
        auto term = b->getTerminator();
        int sucCount = term->getNumSuccessors();
        for (int i = 0; i < sucCount; i++)
        {
            auto suc = term->getSuccessor(i);
            if (find(Body.begin(), Body.end(), suc) == Body.end())
            {
                //it isn't in the kernel, so its an exit or a subkernel
                //we should be able to presume that if it isn't in the kernel map it must be an exit
                //this is because kernels are built from the inside out
                string blockName = suc->getName();
                uint64_t id = std::stoul(blockName.substr(7, blockName.size() - 7 - Name.size()));
                if (KernelMap.find(id) != KernelMap.end())
                {
                    //this belongs to a subkernel
                    Kernel *nestedKernel = KernelMap[id];
                    std::vector<llvm::Value *> inargs;
                    for (auto ai = nestedKernel->KernelFunction->arg_begin(); ai < nestedKernel->KernelFunction->arg_end(); ai++)
                    {
                        inargs.push_back(cast<Value>(ai));
                    }
                    BasicBlock *intermediateBlock = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
                    IRBuilder<> intBuilder(intermediateBlock);
                    intBuilder.CreateCall(nestedKernel->KernelFunction, inargs);
                    intBuilder.CreateBr(cast<BasicBlock>(VMap[nestedKernel->ExitTarget[0]]));
                    term->setSuccessor(i, intermediateBlock);
                    Body.push_back(intermediateBlock);
                }
            }
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

    // now create MemoryRead and MemoryWrite functions
    FunctionType *funcType = FunctionType::get(Type::getInt32Ty(TikModule->getContext()), Type::getInt32Ty(TikModule->getContext()), false);
    MemoryRead = Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, "MemoryRead", TikModule);
    MemoryWrite = Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, "MemoryWrite", TikModule);

    int i = 0;
    BasicBlock *loadBlock = BasicBlock::Create(TikModule->getContext(), "entry", MemoryRead);
    IRBuilder<> loadBuilder(loadBlock);
    Value *priorValue = NULL;

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
        // create a load for every time we use these global pointers
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

    // every time we use a global pointer in the body, store to it
    std::set<llvm::Value *> globalSet;
    for (auto bb : Body)
    {
        for (BasicBlock::iterator BI = bb->begin(), BE = bb->end(); BI != BE; ++BI)
        {
            Instruction *inst = cast<Instruction>(BI);
            for (auto pair : GlobalMap)
            {
                if (llvm::cast<Instruction>(pair.first)->isIdenticalTo(inst))
                {
                    IRBuilder<> builder(inst->getNextNode());
                    Constant *constant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0);
                    auto a = builder.CreateGEP(inst->getType(), GlobalMap[pair.first], constant);
                    auto b = builder.CreateStore(inst, a);

                    MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), static_cast<int>(TikSynthetic::Store))));
                    b->setMetadata("TikSynthetic", tikNode);
                    globalSet.insert(b);
                }
            }
        }
    }

    // find instructions in body block not belonging to parent kernel
    vector<Instruction *> toRemove;
    for (auto bb : Body)
    {
        for (BasicBlock::iterator BI = bb->begin(), BE = bb->end(); BI != BE; ++BI)
        {
            Instruction *inst = cast<Instruction>(BI);
            if (globalSet.find(inst) != globalSet.end())
            {
                continue;
            }

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
        if (globalSet.find(inst) != globalSet.end())
        {
            continue;
        }

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

void Kernel::GetExits()
{
    int exitId = 0;
    // search for exit basic blocks
    vector<BasicBlock *> exits;
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
                    exits.push_back(succ);
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
                    exits.push_back(a);
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
                    exits.push_back(succ);
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
    //ExitTarget[0] = exits[0];
}

void Kernel::CreateExitBlock(void)
{
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
    IRBuilder<> exitBuilder(Exit);
    auto a = exitBuilder.CreateRetVoid();
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