#include "Kernel.h"
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
    Conditional = NULL;
    Name = "Kernel_" + to_string(KernelUID++);
    baseModule = new Module(Name, M->getContext());

    FunctionType *mainType = FunctionType::get(Type::getVoidTy(baseModule->getContext()), false);
    mainFunction = Function::Create(mainType, GlobalValue::LinkageTypes::ExternalLinkage, Name, baseModule);
    Body = BasicBlock::Create(baseModule->getContext(), "Body", mainFunction);
    Init = BasicBlock::Create(baseModule->getContext(), "Init", mainFunction);
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

    //now get the actual body
    GetBodyInsts(blocks);

    //get conditional logic
    GetLoopInsts(blocks);

    //to be removed
    GetInitInsts(blocks);

    //now get the memory array
    GetMemoryFunctions(baseModule);

    //finally remap everything
    Remap();
}

nlohmann::json Kernel::GetJson()
{
    nlohmann::json j = TikBase::GetJson();
    if (Conditional != NULL)
    {
        j["Loop"] = GetStrings(Conditional);
    }
    return j;
}

Kernel::~Kernel()
{
    delete Conditional;
    delete baseModule;
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

        for (int i = 0; i < currentBlocks.size(); i++)
        {
            Instruction *newTerm = currentBlocks[i]->getTerminator();
            unsigned int subCount = newTerm->getNumSuccessors();
            if (subCount > 1)
            {
                throw 2;
                cout << "hi";
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
    BasicBlock * currentBlock;
    Instruction *term = start->getTerminator();
    unsigned int pathCount = term->getNumSuccessors();
    vector<BasicBlock *> currentBlocks(pathCount);
    map<int, vector<StoreInst *>> stores;
    for (int i = 0; i < pathCount; i++)
    {
        currentBlocks[i] = term->getSuccessor(i);
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
                for (BasicBlock::iterator BI = currentBlock->begin(), BE = currentBlock->end(); BI != BE; ++BI)
                {
                    Instruction *inst = cast<Instruction>(BI);
                    if(StoreInst *lInst = dyn_cast<StoreInst>(inst))
                    {
                        //a load we need to buffer
                        stores[i].push_back(lInst);
                    }
                    else if(!inst->isTerminator())
                    {
                        //the general case
                        result.push_back(inst);
                    }
                }
                Instruction *newTerm = currentBlock->getTerminator();
                unsigned int subCount = newTerm->getNumSuccessors();
                if (subCount > 1)
                {
                    throw 2;
                    cout << "hi";
                }
                else
                {
                    BasicBlock *newSuc = newTerm->getSuccessor(0);
                    currentBlocks[i] = newSuc;
                }
            }
        }
    }
    for(auto a : result)
    {
        PrintVal(a);
    }
    return result;
}

void Kernel::Remap()
{
    for (Module::iterator F = baseModule->begin(), E = baseModule->end(); F != E; ++F)
    {
        for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
        {
            for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI)
            {
                Instruction *inst = cast<Instruction>(BI);
                RemapInstruction(inst, VMap, llvm::RF_IgnoreMissingLocals);
            }
        }
    }
}

void Kernel::GetLoopInsts(vector<BasicBlock *> blocks)
{
    Conditional = BasicBlock::Create(baseModule->getContext(), "Loop", mainFunction);
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
            bool found = false;
            for (auto i : result)
                if (i->isIdenticalTo(usr))
                    found = true;
            if (!found && result.size()) //we haven't already done it
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
            //Instruction *ret = cast<Instruction>(VMap[check]);
            //ret->removeFromParent();
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
        cond->eraseFromParent();
        condList->push_back(cl);
    }
}

vector<Instruction *> Kernel::getInstructionPath(BasicBlock *start, vector<BasicBlock *> validBlocks, vector<Instruction *> currentSet)
{
    for (BasicBlock::iterator BI = start->begin(), BE = start->end(); BI != BE; ++BI)
    {
        Instruction *inst = cast<Instruction>(BI);
        if (!inst->isTerminator())
        {
            Instruction *cl = inst->clone();
            VMap[inst] = cl;
            currentSet.push_back(cl);
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
    vector<Instruction *> result;
    if (validSuccessors > 1)
    {
        PrintVal(start);
        auto mergePoint = getPathMerge(start);
        auto mergeInsts = GetPathInstructions(start, mergePoint);
        //PrintVal(mergePoint);

        throw 2;
        //we have a branch
    }
    else
    {
        //only one successor
        BasicBlock *succ = term->getSuccessor(0);
        vector<BasicBlock *> trimmed = validBlocks;
        trimmed.erase(remove(trimmed.begin(), trimmed.end(), start), trimmed.end());

        if (find(validBlocks.begin(), validBlocks.end(), succ) != validBlocks.end())
        {
            return getInstructionPath(succ, trimmed, currentSet);
        }
        else
        {
            //this is the end
            return currentSet;
        }
    }
    return result;
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

    auto asdf = getInstructionPath(currentBlock, blocks);

    auto instList = &Body->getInstList();
    for (Instruction *i : asdf)
    {
        instList->push_back(i);
    }
}

void Kernel::GetInitInsts(vector<BasicBlock *> blocks)
{
    std::set<Instruction *> toAdd;

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
                        toAdd.insert(operand);
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

void Kernel::GetMemoryFunctions(Module *m)
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
    FunctionType *funcType = FunctionType::get(Type::getInt32Ty(m->getContext()), Type::getInt32Ty(m->getContext()), false);
    MemoryRead = Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, "MemoryRead", m);
    MemoryWrite = Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, "MemoryWrite", m);

    int i = 0;
    BasicBlock *loadBlock = BasicBlock::Create(m->getContext(), "entry", MemoryRead);
    IRBuilder<> loadBuilder(loadBlock);
    Value *priorValue = NULL;
    map<Value *, Value *> loadMap;
    map<Value *, Value *> storeMap;
    if (loadValues.size() > 1)
    {
        for (Value *lVal : loadValues)
        {
            Instruction *converted = cast<Instruction>(loadBuilder.CreatePtrToInt(lVal, Type::getInt32Ty(m->getContext())));
            Constant *indexConstant = ConstantInt::get(Type::getInt32Ty(m->getContext()), i++);
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
        Instruction *converted = cast<Instruction>(loadBuilder.CreatePtrToInt(*loadValues.begin(), Type::getInt32Ty(m->getContext())));
        priorValue = converted;
    }
    Instruction *loadRet = cast<ReturnInst>(loadBuilder.CreateRet(priorValue));

    //now do the store
    i = 0;
    BasicBlock *storeBlock = BasicBlock::Create(m->getContext(), "entry", MemoryWrite);
    IRBuilder<> storeBuilder(storeBlock);
    if (storeValues.size() > 1)
    {
        for (Value *lVal : storeValues)
        {
            Instruction *converted = cast<Instruction>(storeBuilder.CreatePtrToInt(lVal, Type::getInt32Ty(m->getContext())));
            if (priorValue == NULL)
            {
                priorValue = converted;
                Constant *indexConstant = ConstantInt::get(Type::getInt32Ty(m->getContext()), i);
                storeMap[lVal] = indexConstant;
            }
            else
            {
                Constant *indexConstant = ConstantInt::get(Type::getInt32Ty(m->getContext()), i++);
                ICmpInst *cmpInst = cast<ICmpInst>(storeBuilder.CreateICmpEQ(MemoryWrite->arg_begin(), indexConstant));
                SelectInst *sInst = cast<SelectInst>(storeBuilder.CreateSelect(cmpInst, converted, priorValue));
                priorValue = sInst;
                storeMap[lVal] = indexConstant;
            }
        }
    }
    else if (storeValues.size() == 1)
    {
        Instruction *converted = cast<Instruction>(storeBuilder.CreatePtrToInt(*storeValues.begin(), Type::getInt32Ty(m->getContext())));
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