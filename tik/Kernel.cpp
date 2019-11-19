#include "Kernel.h"
#include "Tik.h"
#include "Util.h"
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

    GetInitInsts(blocks);

    GetExits(blocks);

    //now get the memory array
    GetMemoryFunctions();

    CreateExitBlock();
    
    Remap();
    
    MorphKernelFunction(blocks);
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
            RemapInstruction(inst, VMap, llvm::RF_None);
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
        throw 2;
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
                unsigned int validSuccessors = 0; /*
                for (int i = 0; i < subCount; i++)
                {
                    if (find(validBlocks.begin(), validBlocks.end(), newTerm->getSuccessor(i)) != validBlocks.end())
                    {
                        validSuccessors++;
                    }
                }*/
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
                LoadInst *lInst = new LoadInst(ptr); // tempBuilder.CreateLoad(ptr);
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
                        SelectInst *sInst = SelectInst::Create(branchCondition, v, priorValue); // cast<SelectInst>(tempBuilder.CreateSelect(branchCondition, v, priorValue));
                        priorValue = sInst;
                        result.push_back(sInst);
                    }
                    else
                    {
                        priorValue = v;
                    }
                }
                auto finStore = new StoreInst(priorValue, ptr); // tempBuilder.CreateStore(priorValue, ptr);
                result.push_back(finStore);
            }
        }
    }
    return result;
}

void Kernel::MorphKernelFunction(std::vector<llvm::BasicBlock* > blocks)
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
    FunctionType *funcType = FunctionType::get(Type::getInt32Ty(TikModule->getContext()), inputArgs, false);
    llvm::Function *newFunc = llvm::Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, KernelFunction->getName() + "_Reformatted", TikModule);
    Init = CloneBasicBlock(Init, localVMap, "", newFunc); 
    Body = CloneBasicBlock(Body, localVMap, "", newFunc);
    Exit = CloneBasicBlock(Exit, localVMap, "", newFunc);
    Conditional = CloneBasicBlock(Conditional, localVMap, "", newFunc);
    
    // remove the old function from the parent but do not erase it
    KernelFunction->removeFromParent();
    KernelFunction = newFunc;

    // for each input instruction, store them into one of our global pointers
    // GlobalMap already contains the input arg->global pointer relationships we need
    std::set<llvm::StoreInst *> newStores;
    for( int i = 0; i < ExternalValues.size(); i++ )
    {
        IRBuilder<> builder(Init);
        PrintVal(ExternalValues[i]);
        auto b = builder.CreateStore( KernelFunction->arg_begin()+i, GlobalMap[ ExternalValues[i] ] );
        newStores.insert(b);
    }

    // now create the branches between basic blocks
    // init->loop (unconditional)
    IRBuilder<> initBuilder(Init);
    auto a = initBuilder.CreateBr(Conditional);
    // loop->body (conditional)
    IRBuilder<> loopBuilder(Conditional);
    auto b = loopBuilder.CreateCondBr(VMap[cond], Body, Exit);
    // body->loop (unconditional)
    IRBuilder<> bodyBuilder(Body);
    auto c = bodyBuilder.CreateBr(Conditional);

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
    cond = conditions[0];

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
            {
                if (i->isIdenticalTo(usr))
                {
                    found = true;
                }
            }
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
        if (!inst->isTerminator())
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
            CallInst *ci = CallInst::Create(k->KernelFunction);
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
    // I need to find all operands in the kernel that are not initialized in the kernel
    // Then I need to make them globals and assign them to the input args
    // Operands are the values to the right of an instruction

    // for all instructions in a basic block
    for (BasicBlock::iterator BI = Body->begin(), BE = Body->end(); BI != BE; ++BI)
    {
        Instruction *inst = cast<Instruction>(BI);
        // get my operands of this instruction
        int numOps = inst->getNumOperands();
        // for all my (potentially unresolved) values
        for (int i = 0; i < numOps; i++)
        {
            Value *op = inst->getOperand(i);
            // dyn_cast actually checks if op is an instruction, if it is, dyn_cast will return an instruction pointer, and if not, NULL
            if (Instruction *operand = dyn_cast<Instruction>(op))
            {
                BasicBlock *parentBlock = operand->getParent();
                // if the parentBlock of the operand is not in this block, then these values will be unresolved in the TIK representation
                if (std::find(blocks.begin(), blocks.end(), parentBlock) == blocks.end() && parentBlock != NULL)
                {
                    // if the operand is not in our vector of operands, add it
                    if (find(ExternalValues.begin(), ExternalValues.end(), operand) == ExternalValues.end())
                    {
                        ExternalValues.push_back(operand);
                    }
                }
            }
        }
    }

    /*for (auto init : ExternalValues)
    {
        VMap[init] = init->clone();
    }*/
}

void Kernel::GetMemoryFunctions()
{
    // load and store instruction sets, to hold the instructions for each function
    set<LoadInst *> loadInst;
    set<StoreInst *> storeInst;

    // parse through all basic block instructions and look for load and store insts, put them in our sets
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

    // values to put into load and store instructions
    set<Value *> loadValues;
    set<Value *> storeValues;
    Type *memType;

    // for each mem instruction, grab its pointers and put them into our sets
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

    // do memread function
    int i = 0;
    BasicBlock *loadBlock = BasicBlock::Create(TikModule->getContext(), "entry", MemoryRead);
    IRBuilder<> loadBuilder(loadBlock);
    Value *priorValue = NULL;

    // maps whose keys are loadValue (storeValue) and values are the name of the register in IR (the loop iterator)
    // these maps store pointers to an index
    map<Value *, Value *> loadMap;
    map<Value *, Value *> storeMap;
    for (Value *lVal : loadValues)
    {
        // every time we make a pointer to an index, do the same thing to the global map
        if (GlobalMap.find(lVal) == GlobalMap.end())
        {
            llvm::Constant *globalInt = ConstantPointerNull::get(cast<PointerType>(lVal->getType()));
            llvm::GlobalVariable *g = new GlobalVariable(*TikModule, globalInt->getType(), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, globalInt);
            GlobalMap[lVal] = g;
        }
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

    //now do the memwrite
    i = 0;
    BasicBlock *storeBlock = BasicBlock::Create(TikModule->getContext(), "entry", MemoryWrite);
    IRBuilder<> storeBuilder(storeBlock);

    priorValue = NULL;
    for (Value *sVal : storeValues)
    {
        if (GlobalMap.find(sVal) == GlobalMap.end())
        {
            llvm::Constant *globalInt = ConstantPointerNull::get(cast<PointerType>(sVal->getType()));
            llvm::GlobalVariable *g = new GlobalVariable(*TikModule, globalInt->getType(), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, globalInt);
            GlobalMap[sVal] = g;
        }
        // every time we make a pointer to an index, do the same thing to the global map
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

    // set of global variables we have created
    std::set<llvm::Value *> globalSet;
    // for each instruction in memread (for example)
    for (BasicBlock::iterator BI = Body->begin(), BE = Body->end(); BI != BE; ++BI)
    {
        // search each instruction for each key in GlobalMap
        Instruction *inst = cast<Instruction>(BI);
        for( auto pair : GlobalMap )
        {
            if( llvm::cast<Instruction>(pair.first)->isIdenticalTo( inst ) )
            {
                IRBuilder<> builder(inst->getNextNode());
                Constant *constant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0);
                auto a = builder.CreateGEP(inst->getType(), GlobalMap[pair.first], constant);
                auto b = builder.CreateStore(inst, a);
                globalSet.insert(b);
            }
        }
   }

    // remove instructions in body block not belonging to parent kernel
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
            //VMap[newInst] = newLoad;
            toRemove.push_back(newInst);
        }
        else if (StoreInst *newInst = dyn_cast<StoreInst>(inst))
        {
            PrintVal(newInst);
            auto memCall = builder.CreateCall(MemoryWrite, storeMap[newInst->getPointerOperand()]);
            auto casted = builder.CreateIntToPtr(memCall, newInst->getPointerOperand()->getType());
            auto newStore = builder.CreateStore(newInst->getValueOperand(), casted);
            toRemove.push_back(newInst);
        }
    }

    // remove instructions in loop  block not belonging to parent
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

void Kernel::CreateExitBlock(void)
{
    IRBuilder<> exitBuilder(Exit);
    auto a = exitBuilder.CreateRetVoid();
}
