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
    Body = NULL;
    Init = NULL;
    Conditional = NULL;
    Exit = NULL;
    Name = "Kernel_" + to_string(KernelUID++);

    FunctionType *mainType = FunctionType::get(Type::getVoidTy(TikModule->getContext()), false);
    KernelFunction = Function::Create(mainType, GlobalValue::LinkageTypes::ExternalLinkage, Name, TikModule);
    Init = BasicBlock::Create(TikModule->getContext(), "Init", KernelFunction);
    Body = BasicBlock::Create(TikModule->getContext(), "Body", KernelFunction);
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

    GetLoopInsts(blocks);

    GetInitInsts(blocks);

    GetExits(blocks);

    GetMemoryFunctions();

    CreateExitBlock();

    Remap();

    MorphKernelFunction(blocks);

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
    if (Body != NULL)
    {
        j["Body"] = GetStrings(Body);
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
    if (Body != NULL)
    {
        delete Body;
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

BasicBlock *Kernel::getPathMerge(llvm::BasicBlock *start)
{
    vector<BasicBlock *> result;
    Instruction *term = start->getTerminator();
    unsigned int pathCount = term->getNumSuccessors();
    vector<BasicBlock *> currentBlocks(pathCount);
    vector<set<BasicBlock *>> exploredBlocks(pathCount);
    for (int i = 0; i < pathCount; i++)
    {
        currentBlocks[i] = term->getSuccessor(i);
    }
    BasicBlock *exit;
    bool done = false;
    while (!done)
    {
        // go through all block terminator instructions
        for (int i = 0; i < currentBlocks.size(); i++)
        {
            Instruction *newTerm = currentBlocks[i]->getTerminator();
            unsigned int subCount = newTerm->getNumSuccessors();
            if (subCount > 1)
            {
                throw TikException("Not Implemented");
            }
            else
            {
                BasicBlock *newSuc = newTerm->getSuccessor(0);
                exploredBlocks[i].insert(newSuc);
                currentBlocks[i] = newSuc;
            }
        }
        for (int i = 0; i < pathCount; i++)
        {
            BasicBlock *toComp = currentBlocks[i];
            bool missing = false;
            for (int j = 0; j < pathCount; j++)
            {
                if (exploredBlocks[i].find(toComp) == exploredBlocks[i].end())
                {
                    missing = true;
                    break;
                }
            }
            if (!missing)
            {
                exit = toComp;
                done = true;
                break;
            }
        }
    }

    return exit;
}

vector<Instruction *> Kernel::GetPathInstructions(BasicBlock *start, BasicBlock *end)
{
    //we now know the merge point so we will add all instructions to the result
    //if it is a load we need to delay adding it till the end
    //if we find another conditional we must recurse
    vector<Instruction *> result;
    BasicBlock *currentBlock;
    Instruction *term = start->getTerminator();
    Value *branchCondition = NULL;
    if (BranchInst *brTerm = dyn_cast<BranchInst>(term))
    {
        branchCondition = brTerm->getCondition();
    }
    else
    {
        throw TikException("Not Implemented");
    }

    unsigned int pathCount = term->getNumSuccessors();
    vector<BasicBlock *> currentBlocks(pathCount);
    map<int, vector<StoreInst *>> stores;
    for (int i = 0; i < pathCount; i++)
    {
        currentBlocks[i] = term->getSuccessor(i);
        stores[i] = {};
    }
    bool done = false;
    while (!done)
    {
        done = true;
        for (int i = 0; i < currentBlocks.size(); i++)
        {
            currentBlock = currentBlocks[i];
            if (currentBlock != end)
            {
                done = false;
                for (BasicBlock::iterator BI = currentBlock->begin(), BE = currentBlock->end(); BI != BE; ++BI)
                {
                    Instruction *inst = cast<Instruction>(BI);
                    if (StoreInst *lInst = dyn_cast<StoreInst>(inst))
                    {
                        //a load we need to buffer
                        stores[i].push_back(lInst);
                    }
                    else if (!inst->isTerminator())
                    {
                        //the general case
                        result.push_back(inst);
                    }
                }
                Instruction *newTerm = currentBlock->getTerminator();
                unsigned int subCount = newTerm->getNumSuccessors();
                unsigned int validSuccessors = 0;
                if (subCount > 1)
                {
                    throw TikException("Not Implemented");
                }
                else
                {
                    BasicBlock *newSuc = newTerm->getSuccessor(0);
                    currentBlocks[i] = newSuc;
                }
            }
        }
    }

    vector<StoreInst *> handledStores;
    for (auto pair : stores)
    {
        auto storeVec = pair.second;
        for (auto store : storeVec)
        {
            //we need to create this store somehow
            //first check if it exists in the others, if so use it otherwise create a load
            if (find(handledStores.begin(), handledStores.end(), store) == handledStores.end())
            {
                //not already handled so get check if there are others
                vector<Value *> valsToSelect;
                auto ptr = store->getPointerOperand();
                LoadInst *lInst = new LoadInst(ptr);
                bool loadUsed = false;

                for (auto p2 : stores)
                {
                    StoreInst *sInst = NULL;
                    //check each entry of the dictionary
                    for (auto i : p2.second)
                    {
                        auto ptr2 = i->getPointerOperand();
                        if (ptr2 == ptr) //these are writing to the same address (ideally a better check will exist)
                        {
                            sInst = i;
                            break;
                        }
                    }
                    if (sInst == NULL)
                    {
                        //we never found a match so we create a load instead
                        valsToSelect.push_back(lInst);
                        if (!loadUsed)
                        {
                            loadUsed = true;
                            result.push_back(lInst);
                        }
                    }
                    else
                    {
                        valsToSelect.push_back(sInst->getValueOperand());
                        handledStores.push_back(sInst);
                    }
                }
                //now that we have the values, create the select tree

                Value *priorValue = NULL;
                int i = 0;
                for (auto v : valsToSelect)
                {
                    Constant *indexConstant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), i++);
                    if (priorValue != NULL)
                    {
                        //this will need to be expanded for switch instructions
                        SelectInst *sInst = SelectInst::Create(branchCondition, v, priorValue);
                        priorValue = sInst;
                        result.push_back(sInst);
                    }
                    else
                    {
                        priorValue = v;
                    }
                }
                auto finStore = new StoreInst(priorValue, ptr);
                result.push_back(finStore);
            }
        }
    }
    return result;
}

void Kernel::MorphKernelFunction(std::vector<llvm::BasicBlock *> blocks)
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
    Body = CloneBasicBlock(Body, localVMap, "", newFunc);
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
            newStores.insert(b);
        }
    }

    // now create the branches between basic blocks
    // init->loop (unconditional)
    IRBuilder<> initBuilder(Init);
    auto a = initBuilder.CreateBr(Conditional);
    // loop->body or loop->exit (conditional)
    IRBuilder<> loopBuilder(Conditional);
    auto b = loopBuilder.CreateCondBr(VMap[LoopCondition], Body, Exit);
    // body->loop (unconditional)
    IRBuilder<> bodyBuilder(Body);
    auto c = bodyBuilder.CreateBr(Conditional);

    // Now find all calls to the embedded kernel functions in the body, if any, and change their arguments to the new ones
    std::map<Argument *, Value *> embeddedCallArgs;
    auto instList = &Body->getInstList();
    for (BasicBlock::iterator i = Body->begin(), BE = Body->end(); i != BE; ++i)
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
                        for (BasicBlock::iterator j = Body->begin(), BE2 = Body->end(); j != BE2; ++j)
                        {
                            if (subK->ArgumentMap.find(sarg) != subK->ArgumentMap.end())
                            {
                                if (subK->ArgumentMap[sarg] == cast<Instruction>(j))
                                {
                                    found = true;
                                    embeddedCallArgs[sarg] = cast<Instruction>(j);
                                }
                            }
                        }
                    }
                    if (!found)
                    {
                        for (auto i : ExternalValues)
                        {
                            for (auto sarg = calledFunc->arg_begin(); sarg < calledFunc->arg_end(); sarg++)
                            {
                                for (auto arg = KernelFunction->arg_begin(); arg < KernelFunction->arg_end(); arg++)
                                {
                                    if (subK->ArgumentMap[sarg] == ArgumentMap[arg])
                                    {
                                        embeddedCallArgs[arg] = subK->ArgumentMap[sarg];
                                    }
                                }
                            }
                        }
                    }
                    std::vector<Value *> embeddedArgs;
                    unsigned int j = 0;
                    for (auto &i : embeddedCallArgs)
                    {
                        embeddedArgs.push_back(i.second);
                        callInst->setOperand(j, i.second);
                        j++;
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

void Kernel::GetLoopInsts(vector<BasicBlock *> blocks)
{
    Conditional = BasicBlock::Create(TikModule->getContext(), "Loop", KernelFunction);
    vector<Instruction *> result;

    // identify all exit blocks
    vector<BasicBlock *> exits;
    for (BasicBlock *block : blocks)
    {
        bool exit = false;
        Instruction *term = block->getTerminator();
        for (int i = 0; i < term->getNumSuccessors(); i++)
        {
            BasicBlock *succ = term->getSuccessor(i);
            if (find(blocks.begin(), blocks.end(), succ) == blocks.end())
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
        for (BasicBlock::iterator BI = Body->begin(), BE = Body->end(); BI != BE; ++BI)
        {
            Instruction *I = cast<Instruction>(BI);
            if (I->isIdenticalTo(cond))
            {
                I->removeFromParent();
                Instruction *cl = cond->clone();
                VMap[cond] = cl;
                condList->push_back(cl);
                break;
            }
        }
    }
}

vector<Instruction *> Kernel::getInstructionPath(BasicBlock *start, vector<BasicBlock *> validBlocks)
{
    vector<Instruction *> result;
    //clone every instruction in the basic block
    for (BasicBlock::iterator BI = start->begin(), BE = start->end(); BI != BE; ++BI)
    {
        Instruction *inst = cast<Instruction>(BI);
        if (CallInst *ci = dyn_cast<CallInst>(inst))
        {
            Function *calledFunc = ci->getCalledFunction();
            if (calledFunc->empty())
            {
                Instruction *cl = inst->clone();
                VMap[inst] = cl;
                result.push_back(cl);
            }
            else
            {
                auto subPath = getInstructionPath(&calledFunc->getEntryBlock(), validBlocks);
                result.insert(result.end(), subPath.begin(), subPath.end());
                int args = calledFunc->arg_size();
                int i = 0;
                for (auto ai = calledFunc->arg_begin(); ai < calledFunc->arg_end(); ai++, i++)
                {
                    Value *arg = cast<Value>(ai);
                    VMap[arg] = VMap[ci->getOperand(i)];
                }
                Type *retType = calledFunc->getReturnType();
                if (!retType->isVoidTy())
                {
                    BasicBlock *block = &calledFunc->getEntryBlock();
                    auto retTree = BuildReturnTree(block, validBlocks);
                    Value *toRemap = NULL;
                    if (retTree.size() == 1)
                    {
                        //this is a single value
                        toRemap = VMap[retTree[0]];
                    }
                    else
                    {
                        //this is a set of instructions where the last is the value of note
                        toRemap = retTree.back();
                        for (auto val : retTree)
                        {
                            result.push_back(cast<Instruction>(val));
                        }
                    }
                    VMap[ci] = toRemap;
                }
            }
        }
        else if (!inst->isTerminator())
        {
            Instruction *cl = inst->clone();
            VMap[inst] = cl;
            result.push_back(cl);
        }
    }
    Instruction *term = start->getTerminator();
    unsigned int succCount = term->getNumSuccessors();
    unsigned int validSuccessors = 0;
    for (int i = 0; i < succCount; i++)
    {
        if (find(validBlocks.begin(), validBlocks.end(), term->getSuccessor(i)) != validBlocks.end())
        {
            validSuccessors++;
        }
    }
    if (validSuccessors > 1)
    {
        //we have a branch
        auto mergePoint = getPathMerge(start);
        auto mergeInsts = GetPathInstructions(start, mergePoint);
        for (auto a : mergeInsts)
        {
            Instruction *cl = a->clone();
            VMap[a] = cl;
            result.push_back(cl);
        }
        auto subPath = getInstructionPath(mergePoint, validBlocks);
        result.insert(result.end(), subPath.begin(), subPath.end());
    }
    else if (validSuccessors == 0)
    {
        //we don't need to do anything unless we started in a function and exited outside of it
        //basically that shouldn't happen so we don't do anything
    }
    else
    {
        //only one successor
        BasicBlock *succ = term->getSuccessor(0);
        string blockName = succ->getName();
        uint64_t id = std::stoul(blockName.substr(7));
        if (KernelMap.find(id) != KernelMap.end())
        {
            //we are in a kernel
            Kernel *k = KernelMap[id];
            std::vector<llvm::Value *> inargs;
            for (auto ai = k->KernelFunction->arg_begin(); ai < k->KernelFunction->arg_end(); ai++)
            {
                inargs.push_back(cast<Value>(ai));
            }
            CallInst *ci = CallInst::Create(k->KernelFunction, inargs);
            result.push_back(ci);
            succ = k->ExitTarget;
        }

        vector<BasicBlock *> trimmed;
        for (auto block : validBlocks)
        {
            if (block != start)
            {
                trimmed.push_back(block);
            }
        }
        if (find(validBlocks.begin(), validBlocks.end(), succ) != validBlocks.end())
        {
            auto subResult = getInstructionPath(succ, trimmed);
            result.insert(result.end(), subResult.begin(), subResult.end());
        }
    }
    return result;
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
            if (find(blocks.begin(), blocks.end(), pred) == blocks.end())
            {
                entrances.push_back(block);
            }
        }
    }
    if (entrances.size() != 1)
    {
        throw TikException("Kernel Exception: tik only supports single entrance kernels");
    }
    BasicBlock *currentBlock = entrances[0];

    // next, find all the instructions in the entrance block bath who call functions, and make our own references to them
    auto path = getInstructionPath(currentBlock, blocks);
    auto instList = &Body->getInstList();
    for (Instruction *i : path)
    {
        if (CallInst *callInst = dyn_cast<CallInst>(i))
        {
            Function *funcCal = callInst->getCalledFunction();
            llvm::Function *funcName = TikModule->getFunction(funcCal->getName());
            if (!funcName)
            {
                llvm::Function *funcDec = llvm::Function::Create(funcCal->getFunctionType(), GlobalValue::LinkageTypes::ExternalLinkage, funcCal->getName(), TikModule);
                funcDec->setAttributes(funcCal->getAttributes());
                callInst->setCalledFunction(funcDec);
            }
            else // must be a kernel function call
            {
                auto calledFunc = callInst->getCalledFunction();
                auto subK = KfMap[calledFunc];
                for (auto ai = calledFunc->arg_begin(); ai < calledFunc->arg_end(); ai++)
                {
                    bool found = false;
                    for (Instruction *j : path) // look through the entire body for this argument value
                    {
                        if (subK->ArgumentMap.find(ai) != subK->ArgumentMap.end())
                        {
                            if (subK->ArgumentMap[ai] == j)
                            {
                                // find the argument in our arguments that map to the subkernels arguments in MorphKernelFunction
                                found = true;
                                break;
                            }
                        }
                    }
                    if (!found)
                    {
                        if (subK->ArgumentMap.find(ai) != subK->ArgumentMap.end())
                        {
                            ExternalValues.push_back(subK->ArgumentMap[ai]);
                        }
                    }
                }
            }
        }
        instList->push_back(i);
    }
}

void Kernel::GetInitInsts(vector<BasicBlock *> blocks)
{
    for (BasicBlock::iterator BI = Body->begin(), BE = Body->end(); BI != BE; ++BI)
    {
        Instruction *inst = cast<Instruction>(BI);
        int numOps = inst->getNumOperands();
        for (int i = 0; i < numOps; i++)
        {
            Value *op = inst->getOperand(i);
            if (Instruction *operand = dyn_cast<Instruction>(op))
            {
                BasicBlock *parentBlock = operand->getParent();
                if (std::find(blocks.begin(), blocks.end(), parentBlock) == blocks.end() && parentBlock != NULL)
                {
                    if (find(ExternalValues.begin(), ExternalValues.end(), operand) == ExternalValues.end())
                    {
                        ExternalValues.push_back(operand);
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
    for (BasicBlock::iterator BI = Body->begin(), BE = Body->end(); BI != BE; ++BI)
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
    for (BasicBlock::iterator BI = Body->begin(), BE = Body->end(); BI != BE; ++BI)
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
                globalSet.insert(b);
            }
        }
    }

    // find instructions in body block not belonging to parent kernel
    vector<Instruction *> toRemove;
    for (BasicBlock::iterator BI = Body->begin(), BE = Body->end(); BI != BE; ++BI)
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
            toRemove.push_back(newInst);
        }
        else if (StoreInst *newInst = dyn_cast<StoreInst>(inst))
        {
            auto memCall = builder.CreateCall(MemoryWrite, storeMap[newInst->getPointerOperand()]);
            auto casted = builder.CreateIntToPtr(memCall, newInst->getPointerOperand()->getType());
            auto newStore = builder.CreateStore(newInst->getValueOperand(), casted);
            toRemove.push_back(newInst);
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

void Kernel::GetExits(std::vector<llvm::BasicBlock *> blocks)
{
    // search for exit basic blocks
    vector<BasicBlock *> exits;
    for (auto block : blocks)
    {
        Instruction *term = block->getTerminator();
        if (BranchInst *brInst = dyn_cast<BranchInst>(term))
        {
            // put the successors of the exit block in a unique-entry vector
            for (unsigned int i = 0; i < brInst->getNumSuccessors(); i++)
            {
                BasicBlock *succ = brInst->getSuccessor(i);
                if (find(blocks.begin(), blocks.end(), succ) == blocks.end())
                {
                    exits.push_back(succ);
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
                    if (find(blocks.begin(), blocks.end(), parent) == blocks.end())
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
                }
            }
            else
            {
                throw TikException("Function called both externally and internally. Unimplemented.")
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
    ExitTarget = exits[0];
}

void Kernel::CreateExitBlock(void)
{
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