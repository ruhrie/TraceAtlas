#include "Kernel.h"
#include "Tik.h"
#include "Util.h"
#include <algorithm>
#include <iostream>
#include <llvm/IR/CFG.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
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
    Name = "Kernel_" + to_string(KernelUID++);

    FunctionType *mainType = FunctionType::get(Type::getVoidTy(TikModule->getContext()), false);
    KernelFunction = Function::Create(mainType, GlobalValue::LinkageTypes::ExternalLinkage, Name, TikModule);
    Body = BasicBlock::Create(TikModule->getContext(), "Body", KernelFunction);
    Init = BasicBlock::Create(TikModule->getContext(), "Init", KernelFunction);
    //start by getting a reference to all the blocks
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

    //get conditional logic
    GetLoopInsts(blocks);

    //now get the actual body
    GetBodyInsts(blocks);

    GetInitInsts(blocks);

    //now get the memory array
    GetMemoryFunctions();

    GetExits(blocks);

    //finally remap everything
    Remap();
}

nlohmann::json Kernel::GetJson()
{
    nlohmann::json j;
    if (Body != NULL)
    {
        j["Body"] = GetStrings(Body);
    }
    if (Init != NULL)
    {
        j["Init"] = GetStrings(Init);
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

void Kernel::GetLoopInsts(vector<BasicBlock *> blocks)
{
    Conditional = BasicBlock::Create(TikModule->getContext(), "Loop", KernelFunction);
    vector<Instruction *> result;
    //now identify all exit blocks
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
                //it isn't in the kernel
                exit = true;
            }
        }
        if (exit)
        {
            exits.push_back(block);
        }
    }
    //now that we have the exits we can get the conditional logic for them
    assert(exits.size() == 1);
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
                throw 2;
            }
        }
        else
        {
            throw 2;
        }
    }
    assert(conditions.size() == 1);

    while (conditions.size() != 0)
    {
        Instruction *check = conditions.back();
        conditions.pop_back();
        bool elegible = true;
        //a value is elegible only if all of its users are in the loop
        for (auto user : check->users())
        {
            Instruction *usr = cast<Instruction>(user);
            if (find(result.begin(), result.end(), usr) == result.end() && result.size())
            {
                if (BranchInst *br = dyn_cast<BranchInst>(usr))
                {
                }
                else
                {
                    elegible = false;
                }
            }
        }
        if (elegible)
        {
            result.push_back(check);
            //if we are elegible we can then check all ops
            int opCount = check->getNumOperands();
            for (int i = 0; i < opCount; i++)
            {
                Value *opValue = check->getOperand(i);
                if (Instruction *op = dyn_cast<Instruction>(opValue))
                {
                    //assuming it is an instruction we should check it
                    conditions.push_back(op);
                }
            }
        }
    }

    reverse(result.begin(), result.end());
    auto condList = &Conditional->getInstList();
    for (auto cond : result)
    {
        Instruction *cl = cond->clone();
        VMap[cond] = cl;
        condList->push_back(cl);
    }
}

void Kernel::GetBodyInsts(vector<BasicBlock *> blocks)
{
    std::vector<Instruction *> result;
    vector<BasicBlock *> entrances;
    for (BasicBlock *block : blocks)
    {
        for (BasicBlock *pred : predecessors(block))
        {
            if (find(blocks.begin(), blocks.end(), pred) == blocks.end())
            {
                //this is an entry block
                entrances.push_back(block);
            }
        }
    }
    assert(entrances.size() == 1);

    //this code won't support loops/internal kernels initially
    //!! I'm serious
    BasicBlock *currentBlock = entrances[0];
    vector<BasicBlock *> exploredBlocks;
    while (true)
    {
        exploredBlocks.push_back(currentBlock);
        for (BasicBlock::iterator bi = currentBlock->begin(), be = currentBlock->end(); bi != be; bi++)
        {
            Instruction *inst = cast<Instruction>(bi);
            if (inst->isTerminator())
            {
                //we should ignore for the time being
            }
            else
            {
                //we now check if the instruction is already present
                if (VMap.find(inst) == VMap.end())
                {
                    Instruction *cl = inst->clone();
                    VMap[inst] = cl;
                    result.push_back(cl);
                }
            }
        }
        Instruction *term = currentBlock->getTerminator();
        if (BranchInst *brInst = dyn_cast<BranchInst>(term))
        {
            if (brInst->isConditional())
            {
                bool explored = true;
                //this indicates either the exit or a for loop within the code
                //aka a kernel or something to unroll
                int sucNum = brInst->getNumSuccessors();
                for (int i = 0; i < sucNum; i++)
                {
                    BasicBlock *succ = brInst->getSuccessor(i);
                    if (find(blocks.begin(), blocks.end(), succ) != blocks.end())
                    {
                        //this is a branch to a kernel block
                        if (find(exploredBlocks.begin(), exploredBlocks.end(), succ) == exploredBlocks.end())
                        {
                            //and we haven't explored it yet
                            explored = false;
                            currentBlock = succ;
                        }
                    }
                }
                if (explored)
                {
                    break;
                }
            }
            else
            {
                BasicBlock *succ = brInst->getSuccessor(0);
                if (find(exploredBlocks.begin(), exploredBlocks.end(), succ) == exploredBlocks.end())
                {
                    currentBlock = succ;
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            throw 2;
        }
    }
    auto instList = &Body->getInstList();
    for (Instruction *i : result)
    {
        instList->push_back(i);
    }
}

void Kernel::GetInitInsts(vector<BasicBlock *> blocks)
{
    std::vector<Instruction *> toAdd;

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
                if (std::find(blocks.begin(), blocks.end(), parentBlock) == blocks.end())
                {
                    if (find(toAdd.begin(), toAdd.end(), operand) == toAdd.end())
                    {
                        toAdd.push_back(operand);
                    }
                }
            }
        }
    }

    auto initList = &Init->getInstList();
    for (auto init : toAdd)
    {
        Instruction *i = init->clone();
        VMap[init] = i;
        initList->push_back(i);
    }
}

void Kernel::GetMemoryFunctions()
{
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
    set<Value *> loadValues;
    set<Value *> storeValues;
    Type *memType;
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
    FunctionType *funcType = FunctionType::get(Type::getInt32Ty(TikModule->getContext()), Type::getInt32Ty(TikModule->getContext()), false);
    MemoryRead = Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, "MemoryRead", TikModule);
    MemoryWrite = Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, "MemoryWrite", TikModule);

    int i = 0;
    BasicBlock *loadBlock = BasicBlock::Create(TikModule->getContext(), "entry", MemoryRead);
    IRBuilder<> loadBuilder(loadBlock);
    Value *priorValue = NULL;
    map<Value *, Value *> loadMap;
    map<Value *, Value *> storeMap;
    if (loadValues.size() > 1)
    {
        for (Value *lVal : loadValues)
        {
            Instruction *converted = cast<Instruction>(loadBuilder.CreatePtrToInt(lVal, Type::getInt32Ty(TikModule->getContext())));
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
    }
    else if (loadValues.size() == 1)
    {
        Instruction *converted = cast<Instruction>(loadBuilder.CreatePtrToInt(*loadValues.begin(), Type::getInt32Ty(TikModule->getContext())));
        priorValue = converted;
    }
    Instruction *loadRet = cast<ReturnInst>(loadBuilder.CreateRet(priorValue));

    //now do the store
    i = 0;
    BasicBlock *storeBlock = BasicBlock::Create(TikModule->getContext(), "entry", MemoryWrite);
    IRBuilder<> storeBuilder(storeBlock);
    if (storeValues.size() > 1)
    {
        for (Value *lVal : storeValues)
        {
            Instruction *converted = cast<Instruction>(storeBuilder.CreatePtrToInt(lVal, Type::getInt32Ty(TikModule->getContext())));
            if (priorValue == NULL)
            {
                priorValue = converted;
                Constant *indexConstant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), i);
                storeMap[lVal] = indexConstant;
            }
            else
            {
                Constant *indexConstant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), i++);
                ICmpInst *cmpInst = cast<ICmpInst>(storeBuilder.CreateICmpEQ(MemoryWrite->arg_begin(), indexConstant));
                SelectInst *sInst = cast<SelectInst>(storeBuilder.CreateSelect(cmpInst, converted, priorValue));
                priorValue = sInst;
                storeMap[lVal] = indexConstant;
            }
        }
    }
    else if (storeValues.size() == 1)
    {
        Instruction *converted = cast<Instruction>(storeBuilder.CreatePtrToInt(*storeValues.begin(), Type::getInt32Ty(TikModule->getContext())));
        priorValue = converted;
    }
    Instruction *storeRet = cast<ReturnInst>(storeBuilder.CreateRet(priorValue));

    vector<Instruction *> toRemove;
    for (BasicBlock::iterator BI = Body->begin(), BE = Body->end(); BI != BE; ++BI)
    {
        Instruction *inst = cast<Instruction>(BI);
        IRBuilder<> builder(inst);
        if (LoadInst *newInst = dyn_cast<LoadInst>(inst))
        {
            auto memCall = builder.CreateCall(MemoryRead, loadMap[newInst->getPointerOperand()]);
            auto casted = builder.CreateIntToPtr(memCall, newInst->getPointerOperand()->getType());
            auto newLoad = builder.CreateLoad(casted);
            newInst->replaceAllUsesWith(newLoad);
            //VMap[newInst] = newLoad;
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
    for (auto inst : toRemove)
    {
        inst->removeFromParent();
    }
}

void Kernel::GetExits(std::vector<llvm::BasicBlock *> blocks)
{
    vector<BasicBlock *> exits;
    for (auto block : blocks)
    {
        Instruction *term = block->getTerminator();
        if (BranchInst *brInst = dyn_cast<BranchInst>(term))
        {
            for (unsigned int i = 0; i < brInst->getNumSuccessors(); i++)
            {
                BasicBlock *succ = brInst->getSuccessor(i);
                if (find(blocks.begin(), blocks.end(), succ) == blocks.end())
                {
                    exits.push_back(succ);
                }
            }
        }
        else
        {
            throw 2;
        }
    }
    assert(exits.size() == 1);
    ExitTarget = exits[0];
}