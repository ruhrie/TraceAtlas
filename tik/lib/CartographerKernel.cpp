#include "tik/CartographerKernel.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
#include "AtlasUtil/Print.h"
#include "tik/Util.h"
#include "tik/libtik.h"
#include <llvm/IR/CFG.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <nlohmann/json.hpp>
#include <queue>
#include <spdlog/spdlog.h>

using namespace std;
using namespace llvm;
namespace TraceAtlas::tik
{
    std::set<GlobalVariable *> globalDeclarationSet;
    std::set<Value *> remappedOperandSet;

    void findScopedStructures(Value *val, set<BasicBlock *> &scopedBlocks, set<Function *> &scopedFuncs, set<Function *> &embeddedKernels)
    {
        if (auto func = dyn_cast<Function>(val))
        {
            scopedFuncs.insert(func);
            for (auto it = func->begin(); it != func->end(); it++)
            {
                auto block = cast<BasicBlock>(it);
                findScopedStructures(block, scopedBlocks, scopedFuncs, embeddedKernels);
            }
        }
        else if (auto block = dyn_cast<BasicBlock>(val))
        {
            scopedBlocks.insert(block);
            // check whether its an entrance to a subkernel
            for (auto key : KfMap)
            {
                if (key.second.get() != nullptr)
                {
                    for (auto ent : key.second->Entrances)
                    {
                        if (ent->Block == GetBlockID(block))
                        {
                            embeddedKernels.insert(key.first);
                        }
                    }
                }
            }
            for (auto BB = block->begin(); BB != block->end(); BB++)
            {
                auto inst = cast<Instruction>(BB);
                findScopedStructures(inst, scopedBlocks, scopedFuncs, embeddedKernels);
            }
        }
        else if (auto inst = dyn_cast<Instruction>(val))
        {
            if (auto ci = dyn_cast<CallInst>(inst))
            {
                if (ci->getCalledFunction() == nullptr)
                {
                    throw AtlasException("Null function call: indirect call");
                }
                findScopedStructures(ci->getCalledFunction(), scopedBlocks, scopedFuncs, embeddedKernels);
            }
            else if (auto inv = dyn_cast<InvokeInst>(inst))
            {
                if (inv->getCalledFunction() == nullptr)
                {
                    throw AtlasException("Null invoke call: indirect call");
                }
                findScopedStructures(inv->getCalledFunction(), scopedBlocks, scopedFuncs, embeddedKernels);
            }
        }
        return;
    }

    void CopyOperand(llvm::User *inst, llvm::ValueToValueMapTy &VMap)
    {
        if (auto func = dyn_cast<Function>(inst))
        {
            auto m = func->getParent();
            if (m != TikModule)
            {
                auto *funcDec = cast<Function>(TikModule->getOrInsertFunction(func->getName(), func->getFunctionType()).getCallee());
                funcDec->setAttributes(func->getAttributes());
                VMap[cast<Value>(func)] = funcDec;
                for (auto arg = funcDec->arg_begin(); arg < funcDec->arg_end(); arg++)
                {
                    auto argVal = cast<Value>(arg);
                    if (auto Use = dyn_cast<User>(argVal))
                    {
                        CopyOperand(Use, VMap);
                    }
                }
            }
        }
        else if (auto *gv = dyn_cast<GlobalVariable>(inst))
        {
            Module *m = gv->getParent();
            if (m != TikModule)
            {
                //its the wrong module
                if (globalDeclarationSet.find(gv) == globalDeclarationSet.end())
                {
                    // iterate through all internal operators of this global
                    if (gv->hasInitializer())
                    {
                        llvm::Constant *value = gv->getInitializer();
                        for (uint32_t iter = 0; iter < value->getNumOperands(); iter++)
                        {
                            auto *internal = cast<llvm::User>(value->getOperand(iter));
                            CopyOperand(internal, VMap);
                        }
                    }
                    //and not already in the vmap

                    //for some reason if we don't do this first the verifier fails
                    //we do absolutely nothing with it and it doesn't even end up in our output
                    //its technically a memory leak, but its an acceptable sacrifice
                    auto *newVar = new GlobalVariable(
                        gv->getValueType(),
                        gv->isConstant(), gv->getLinkage(), nullptr, "",
                        gv->getThreadLocalMode(),
                        gv->getType()->getAddressSpace());
                    newVar->copyAttributesFrom(gv);
                    //end of the sacrifice
                    auto newGlobal = cast<GlobalVariable>(TikModule->getOrInsertGlobal(gv->getName(), gv->getType()->getPointerElementType()));
                    newGlobal->setConstant(gv->isConstant());
                    newGlobal->setLinkage(gv->getLinkage());
                    newGlobal->setThreadLocalMode(gv->getThreadLocalMode());
                    newGlobal->copyAttributesFrom(gv);
                    if (gv->hasInitializer())
                    {
                        newGlobal->setInitializer(MapValue(gv->getInitializer(), VMap));
                    }
                    SmallVector<std::pair<unsigned, MDNode *>, 1> MDs;
                    gv->getAllMetadata(MDs);
                    for (auto MD : MDs)
                    {
                        newGlobal->addMetadata(MD.first, *MapMetadata(MD.second, VMap, RF_MoveDistinctMDs));
                    }
                    if (Comdat *SC = gv->getComdat())
                    {
                        Comdat *DC = newGlobal->getParent()->getOrInsertComdat(SC->getName());
                        DC->setSelectionKind(SC->getSelectionKind());
                        newGlobal->setComdat(DC);
                    }
                    globalDeclarationSet.insert(newGlobal);
                    VMap[gv] = newGlobal;
                    for (auto user : gv->users())
                    {
                        if (auto *newInst = dyn_cast<llvm::Instruction>(user))
                        {
                            if (newInst->getModule() == TikModule)
                            {
                                user->replaceUsesOfWith(gv, newGlobal);
                            }
                        }
                    }
                    // check for arguments within the global variable
                    for (unsigned int i = 0; i < newGlobal->getNumOperands(); i++)
                    {
                        if (auto newOp = dyn_cast<GlobalVariable>(newGlobal->getOperand(i)))
                        {
                            CopyOperand(newOp, VMap);
                        }
                        else if (auto newOp = dyn_cast<Constant>(newGlobal->getOperand(i)))
                        {
                            CopyOperand(newOp, VMap);
                        }
                    }
                }
            }
        }
        for (uint32_t j = 0; j < inst->getNumOperands(); j++)
        {
            if (auto newGP = dyn_cast<GlobalVariable>(inst->getOperand(j)))
            {
                CopyOperand(newGP, VMap);
            }
            else if (auto newFunc = dyn_cast<Function>(inst->getOperand(j)))
            {
                CopyOperand(newFunc, VMap);
            }
            else if (auto newOp = dyn_cast<GEPOperator>(inst->getOperand(j)))
            {
                CopyOperand(newOp, VMap);
            }
            else if (auto newBitCast = dyn_cast<BitCastOperator>(inst->getOperand(j)))
            {
                CopyOperand(newBitCast, VMap);
            }
        }
    }

    CartographerKernel::CartographerKernel(std::vector<int64_t> basicBlocks, std::string name)
    {
        // Validate input
        llvm::ValueToValueMapTy VMap;
        auto blockSet = set<int64_t>(basicBlocks.begin(), basicBlocks.end());
        set<BasicBlock *> blocks;
        for (auto id : blockSet)
        {
            if (IDToBlock.find(id) == IDToBlock.end())
            {
                throw AtlasException("Found a basic block with no ID!");
            }
            blocks.insert(IDToBlock[id]);
        }
        string Name;
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
            throw AtlasException("Kernel Error: Kernel names must be unique!");
        }
        spdlog::debug("Started converting kernel {0}", Name);
        reservedNames.insert(Name);

        try
        {
            //this is a recursion check, just so we can enumerate issues
            for (auto block : blocks)
            {
                Function *f = block->getParent();
                for (auto bi = block->begin(); bi != block->end(); bi++)
                {
                    if (auto *cb = dyn_cast<CallBase>(bi))
                    {
                        // if this is the parent function, its on our context level so don't
                        if (cb->getCalledFunction() == f)
                        {
                            throw AtlasException("Tik Error: Recursion is unimplemented")
                        }
                    }
                }
            }

            auto ent = GetEntrances(blocks);
            int entranceId = 0;
            for (auto e : ent)
            {
                Entrances.insert(make_shared<KernelInterface>(entranceId++, GetBlockID(e)));
            }
            map<Value *, GlobalObject *> GlobalMap;
            vector<int64_t> KernelImports;
            vector<int64_t> KernelExports;
            set<BasicBlock *> scopedBlocks = blocks;
            set<Function *> scopedFuncs;
            set<Function *> embeddedKernels;
            for (auto block : blocks)
            {
                findScopedStructures(block, scopedBlocks, scopedFuncs, embeddedKernels);
            }
            for (auto func : scopedFuncs)
            {
                cout << func->getName().data() << endl;
            }
            GetBoundaryValues(scopedBlocks, scopedFuncs, embeddedKernels, KernelImports, KernelExports, VMap);
            //we now have all the information we need
            //start by making the correct function
            vector<Type *> inputArgs;
            // First arg is always the Entrance index
            inputArgs.push_back(Type::getInt8Ty(TikModule->getContext()));
            for (auto inst : KernelImports)
            {
                if (IDToValue.find(inst) != IDToValue.end())
                {
                    inputArgs.push_back(IDToValue[inst]->getType());
                }
                else if (IDToBlock.find(inst) != IDToBlock.end())
                {
                    throw AtlasException("Tried pushing an import of type void into kernel function args!");
                }
                else
                {
                    throw AtlasException("Tried to push a nullptr into the inputArgs when parsing imports.");
                }
            }
            for (auto inst : KernelExports)
            {
                if (IDToValue.find(inst) != IDToValue.end())
                {
                    inputArgs.push_back(IDToValue[inst]->getType()->getPointerTo());
                }
                else if (IDToBlock.find(inst) != IDToBlock.end())
                {
                    throw AtlasException("Tried pushing an export of type void into kernel function args!");
                }
                else
                {
                    throw AtlasException("Tried to push a nullptr into the inputArgs when parsing imports.");
                }
            }
            FunctionType *funcType = FunctionType::get(Type::getInt8Ty(TikModule->getContext()), inputArgs, false);
            KernelFunction = Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, Name, TikModule);
            for (auto arg = KernelFunction->arg_begin(); arg != KernelFunction->arg_end(); arg++)
            {
                uint64_t newId = (uint64_t)(prev(IDToValue.end())->first + 1);
                SetValueIDs(arg, newId);
                IDToValue[(int64_t)newId] = arg;
            }
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
            }
            // map our imports
            for (auto key : ArgumentMap)
            {
                if (key.first->getName()[0] == 'i')
                {
                    VMap[IDToValue[key.second]] = key.first;
                }
            }
            //create the artificial blocks
            Init = BasicBlock::Create(TikModule->getContext(), "Init", KernelFunction);
            Exit = BasicBlock::Create(TikModule->getContext(), "Exit", KernelFunction);
            Exception = BasicBlock::Create(TikModule->getContext(), "Exception", KernelFunction);

            BuildKernelFromBlocks(VMap, blocks);

            BuildInit(VMap, KernelExports);

            Remap(VMap); //we need to remap before inlining

            InlineFunctionsFromBlocks(blockSet);

            CopyGlobals(VMap);

            //remap and repipe
            Remap(VMap);

            // Evaluate embedded kernel exits for export ambiguities
            for (auto embfunc : embeddedKernels)
            {
                for (auto bi = KernelFunction->begin(); bi != KernelFunction->end(); bi++)
                {
                    for (auto it = bi->begin(); it != bi->end(); it++)
                    {
                        if (auto callInst = dyn_cast<CallInst>(it))
                        {
                            if (callInst->getCalledFunction() == embfunc)
                            {
                                // found an embedded kernel, evaluate its exits
                                auto sw = cast<SwitchInst>(bi->getTerminator());
                                //for( unsigned int i = 0; i < sw->getNumSuccessors(); i++ )
                                //{
                                //auto destBlock = sw->getSuccessor(i);
                                for (auto succ : successors(sw->getParent()))
                                {
                                    auto destBlock = succ;
                                    for (auto ii = destBlock->begin(); ii != destBlock->end(); ii++)
                                    {
                                        // look for a phi node in the successor that has exports in it
                                        if (auto phi = dyn_cast<PHINode>(ii))
                                        {
                                            // set of exit index and argument pairs to consolidate to one value and map to the needing phi
                                            set<pair<int64_t, int64_t>> exportsToMap;
                                            // if an incoming value maps to an export of the embedded kernel, we need to map the associated block to an embedded kernel exit
                                            for (unsigned int j = 0; j < phi->getNumIncomingValues(); j++)
                                            {
                                                auto embKern = KfMap[embfunc];
                                                for (auto embKey : embKern->ArgumentMap)
                                                {
                                                    if (GetValueID(phi->getIncomingValue(j)) == embKey.second)
                                                    {
                                                        auto embKernExit = embKern->Exits.find(GetBlockID(phi->getIncomingBlock(j)));
                                                        if (embKernExit != embKern->Exits.end())
                                                        {
                                                            exportsToMap.insert(pair((*embKernExit)->Index, embKey.second));
                                                        }
                                                    }
                                                }
                                            }
                                            if (!exportsToMap.empty())
                                            {
                                                throw AtlasException("Cannot map multiple embedded kernel exports to a single successor!");
                                                // now create a switch instruction to export the correct value to the phi
                                                IRBuilder<> seBuilder(sw->getParent());
                                                Value *prevSel = IDToValue[exportsToMap.begin()->second];
                                                for (const auto exit : exportsToMap)
                                                {
                                                    auto cmp = seBuilder.CreateICmpEQ(ConstantInt::get(Type::getInt8Ty(callInst->getContext()), (uint64_t)exit.first), callInst);
                                                    cast<CmpInst>(cmp)->moveBefore(sw);
                                                    auto sel = seBuilder.CreateSelect(cmp, IDToValue[exit.second], prevSel);
                                                    cast<SelectInst>(sel)->moveBefore(sw);
                                                    prevSel = sel;
                                                }
                                                for (unsigned int j = 0; j < phi->getNumIncomingValues(); j++)
                                                {
                                                    if (phi->getIncomingBlock(j) == sw->getParent())
                                                    {
                                                        phi->removeIncomingValue(j);
                                                    }
                                                }
                                                phi->addIncoming(prevSel, sw->getParent());
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            PatchPhis(VMap);

            // replace parent export uses in embedded kernel call instructions
            for (auto parKey : ArgumentMap)
            {
                if (parKey.first->getName()[0] == 'e')
                {
                    // check each user for possible remapping
                    for (auto bi = KernelFunction->begin(); bi != KernelFunction->end(); bi++)
                    {
                        for (auto it = bi->begin(); it != bi->end(); it++)
                        {
                            for (unsigned int i = 0; i < it->getNumOperands(); i++)
                            {
                                if (GetValueID(it->getOperand(i)) == parKey.second)
                                {
                                    if (auto callInst = dyn_cast<CallInst>(it))
                                    {
                                        if (embeddedKernels.find(callInst->getCalledFunction()) != embeddedKernels.end())
                                        {
                                            callInst->replaceUsesOfWith(IDToValue[parKey.second], parKey.first);
                                        }
                                        else
                                        {
                                            IRBuilder<> ldBuilder(callInst->getParent());
                                            auto ld = ldBuilder.CreateLoad(parKey.first);
                                            ld->moveBefore(callInst);
                                            callInst->replaceUsesOfWith(IDToValue[parKey.second], ld);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // replace child exports to parent with loads from those exports
            for (auto embedded : embeddedKernels)
            {
                auto kernFunc = KfMap[embedded];
                for (auto embExp : kernFunc->ArgumentMap)
                {
                    if (embExp.first->getName()[0] == 'e')
                    {
                        bool found = false;
                        // check to see if this value does not map to a parent export
                        for (auto parArg : ArgumentMap)
                        {
                            if (parArg.second == embExp.second)
                            {
                                found = true;
                            }
                        }
                        if (!found)
                        {
                            // this is an export to the parent, make sure the parent has a use for it first
                            bool useFound = false;
                            for (auto use : IDToValue[embExp.second]->users())
                            {
                                if (auto useInst = dyn_cast<Instruction>(use))
                                {
                                    if (blocks.find(useInst->getParent()) != blocks.end())
                                    {
                                        useFound = true;
                                    }
                                }
                            }
                            if (!useFound)
                            {
                                throw AtlasException("Child export to parent has no uses!");
                            }
                            // make an alloc for it in Init
                            IRBuilder alBuilder(Init);
                            auto sel = Init->getTerminator();
                            auto al = alBuilder.CreateAlloca(IDToValue[embExp.second]->getType());
                            al->moveBefore(sel);
                            // now replace all uses of the export value with loads or references to the alloca
                            for (auto bi = KernelFunction->begin(); bi != KernelFunction->end(); bi++)
                            {
                                for (auto it = bi->begin(); it != bi->end(); it++)
                                {
                                    for (unsigned int i = 0; i < it->getNumOperands(); i++)
                                    {
                                        if (embExp.second == GetValueID(it->getOperand(i)))
                                        {
                                            if (auto st = dyn_cast<StoreInst>(it))
                                            {
                                                continue;
                                            }
                                            else if (auto callInst = dyn_cast<CallInst>(it))
                                            {
                                                if (embeddedKernels.find(callInst->getCalledFunction()) != embeddedKernels.end())
                                                {
                                                    callInst->replaceUsesOfWith(IDToValue[embExp.second], al);
                                                }
                                                else
                                                {
                                                    IRBuilder<> ldBuilder(callInst->getParent());
                                                    auto ld = ldBuilder.CreateLoad(al);
                                                    ld->moveBefore(callInst);
                                                    callInst->replaceUsesOfWith(IDToValue[embExp.second], ld);
                                                }
                                            }
                                            else if (auto phi = dyn_cast<PHINode>(it))
                                            {
                                                // inject loads into the predecessor of our user
                                                for (unsigned int j = 0; j < phi->getNumIncomingValues(); j++)
                                                {
                                                    if (phi->getIncomingValue(j) == IDToValue[embExp.second])
                                                    {
                                                        auto predBlock = phi->getIncomingBlock(j);
                                                        auto term = predBlock->getTerminator();
                                                        IRBuilder<> ldBuilder(predBlock);
                                                        auto ld = ldBuilder.CreateLoad(al);
                                                        ld->moveBefore(term);
                                                        phi->replaceUsesOfWith(IDToValue[embExp.second], ld);
                                                    }
                                                }
                                            }
                                            else if (auto useInst = dyn_cast<Instruction>(it))
                                            {
                                                IRBuilder<> ldBuilder(useInst->getParent());
                                                auto ld = ldBuilder.CreateLoad(al);
                                                useInst->replaceUsesOfWith(IDToValue[embExp.second], ld);
                                                ld->moveBefore(useInst);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // replace external function calls with tik declarations
            for (auto &bi : *(KernelFunction))
            {
                for (auto inst = bi.begin(); inst != bi.end(); inst++)
                {
                    if (auto callBase = dyn_cast<CallBase>(inst))
                    {
                        Function *f = callBase->getCalledFunction();
                        if (f == nullptr)
                        {
                            throw AtlasException("Null function call (indirect call)");
                        }
                        auto *funcDec = cast<Function>(TikModule->getOrInsertFunction(callBase->getCalledFunction()->getName(), callBase->getCalledFunction()->getFunctionType()).getCallee());
                        funcDec->setAttributes(callBase->getCalledFunction()->getAttributes());
                        callBase->setCalledFunction(funcDec);
                    }
                }
            }

            Remap(VMap);

            BuildExit();

            FixInvokes();

            //apply metadata
            ApplyMetadata(GlobalMap);

            //and set a flag that we succeeded
            Valid = true;
        }
        catch (AtlasException &e)
        {
            spdlog::error(e.what());
            if (KernelFunction != nullptr)
            {
                KernelFunction->eraseFromParent();
            }
        }
    }

    void CartographerKernel::GetBoundaryValues(set<BasicBlock *> &scopedBlocks, set<Function *> &scopedFuncs, set<Function *> &embeddedKernels, vector<int64_t> &KernelImports, vector<int64_t> &KernelExports, ValueToValueMapTy &VMap)
    {
        set<int64_t> kernelIE;
        for (const auto block : scopedBlocks)
        {
            // check for an embedded kernel here
            for (auto embeddedKern : embeddedKernels)
            {
                auto subKernel = KfMap[embeddedKern];
                if (subKernel->Entrances.find(GetBlockID(block)) != subKernel->Entrances.end())
                {
                    for (auto key : subKernel->ArgumentMap)
                    {
                        // just look at imports
                        if (key.first->getName()[0] == 'i')
                        {
                            auto importVal = IDToValue[key.second];
                            if (auto importInst = dyn_cast<Instruction>(importVal))
                            {
                                if (scopedBlocks.find(importInst->getParent()) == scopedBlocks.end())
                                {
                                    if (kernelIE.find(key.second) == kernelIE.end())
                                    {
                                        KernelImports.push_back(key.second);
                                        kernelIE.insert(key.second);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            for (auto BI = block->begin(), BE = block->end(); BI != BE; ++BI)
            {
                auto *inst = cast<Instruction>(BI);
                // check its uses
                for (auto use : inst->users())
                {
                    if (auto useInst = dyn_cast<Instruction>(use))
                    {
                        if (scopedBlocks.find(useInst->getParent()) == scopedBlocks.end())
                        {
                            // may belong to a subkernel, which is not an export
                            if (embeddedKernels.find(useInst->getParent()->getParent()) == embeddedKernels.end())
                            {
                                auto sExtVal = GetValueID(inst);
                                if (kernelIE.find(sExtVal) == kernelIE.end())
                                {
                                    KernelExports.push_back(sExtVal);
                                    kernelIE.insert(sExtVal);
                                }
                            }
                        }
                    }
                }
                // now we have to check the block successors
                // if this block can exit the kernel, that means we are replacing a block in the source bitcode that may be left with no predecessors
                // but there may still be users of its values. So they need to be exported
                for (auto succ : successors(block))
                {
                    if (scopedBlocks.find(succ) == scopedBlocks.end())
                    {
                        // this block can exit
                        // if the value uses extend beyond this block, export it
                        for (auto use : inst->users())
                        {
                            if (auto outInst = dyn_cast<Instruction>(use))
                            {
                                if (outInst->getParent() != inst->getParent()) // && scopedBlocks.find(inst->getParent()) == scopedBlocks.end() )
                                {
                                    auto sExtVal = GetValueID(inst);
                                    if (kernelIE.find(sExtVal) == kernelIE.end())
                                    {
                                        KernelExports.push_back(sExtVal);
                                        kernelIE.insert(sExtVal);
                                    }
                                }
                            }
                        }
                    }
                }
                for (uint32_t i = 0; i < inst->getNumOperands(); i++)
                {
                    Value *op = inst->getOperand(i);
                    int64_t valID = GetValueID(op);
                    if (valID < IDState::Artificial)
                    {
                        if (auto testBlock = dyn_cast<BasicBlock>(op))
                        {
                            if (GetBlockID(testBlock) < IDState::Artificial)
                            {
                                throw AtlasException("Found a basic block in the bitcode that did not have a blockID.");
                            }
                        }
                        // check to see if this object can have metadata
                        else if (auto testInst = dyn_cast<Instruction>(op))
                        {
                            throw AtlasException("Found a value in the bitcode that did not have a valueID.");
                        }
                        else if (auto testGO = dyn_cast<GlobalObject>(op))
                        {
                            throw AtlasException("Found a global object in the bitcode that did not have a valueID.");
                        }
                        else if (auto arg = dyn_cast<Argument>(op))
                        {
                            throw AtlasException("Found an argument in the bitcode that did not have a valueID.");
                        }
                        else
                        {
                            // its not an instruction, global object or argument, we don't care about this value
                            continue;
                        }
                    }
                    if (auto arg = dyn_cast<Argument>(op))
                    {
                        if (scopedFuncs.find(arg->getParent()) == scopedFuncs.end())
                        {
                            if (embeddedKernels.find(arg->getParent()) == embeddedKernels.end())
                            {
                                auto sExtVal = GetValueID(arg);
                                // we found an argument of the callinst that came from somewhere else
                                if (kernelIE.find(sExtVal) == kernelIE.end())
                                {
                                    KernelImports.push_back(sExtVal);
                                    kernelIE.insert(sExtVal);
                                }
                            }
                        }
                    }
                    else if (auto *operand = dyn_cast<Instruction>(op))
                    {
                        // if this operand is being used in a phi, this may be an incoming values through a side door (a phi that once had many predecessors, but now has fewer because of the kernel partition)
                        // these values should be ignored
                        if (auto phi = dyn_cast<PHINode>(inst))
                        {
                            for (unsigned int j = 0; j < phi->getNumIncomingValues(); j++)
                            {
                                if (phi->getIncomingValue(j) == operand)
                                {
                                    // below captures values who are used in phi nodes that are one level away from our entrances
                                    bool import = false;
                                    for (auto succ : successors(phi->getIncomingBlock(j)))
                                    {
                                        if (succ != phi->getIncomingBlock(j) && Entrances.find(GetBlockID(succ)) != Entrances.end() && scopedBlocks.find(phi->getIncomingBlock(j)) == scopedBlocks.end())
                                        {
                                            import = true;
                                            auto sExtVal = GetValueID(operand);
                                            if (kernelIE.find(sExtVal) == kernelIE.end())
                                            {
                                                KernelImports.push_back(sExtVal);
                                                kernelIE.insert(sExtVal);
                                            }
                                        }
                                    }
                                    // below captures values who come from far away places and enter through the front door
                                    if (!import)
                                    {
                                        if (scopedBlocks.find(operand->getParent()) == scopedBlocks.end() && (Entrances.find(GetBlockID(phi->getIncomingBlock(j))) != Entrances.end() || scopedBlocks.find(phi->getIncomingBlock(j)) != scopedBlocks.end()))
                                        {
                                            auto sExtVal = GetValueID(operand);
                                            if (kernelIE.find(sExtVal) == kernelIE.end())
                                            {
                                                KernelImports.push_back(sExtVal);
                                                kernelIE.insert(sExtVal);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        else if (scopedBlocks.find(operand->getParent()) == scopedBlocks.end())
                        {
                            if (scopedFuncs.find(operand->getParent()->getParent()) == scopedFuncs.end())
                            {
                                auto sExtVal = GetValueID(operand);
                                if (kernelIE.find(sExtVal) == kernelIE.end())
                                {
                                    KernelImports.push_back(sExtVal);
                                    kernelIE.insert(sExtVal);
                                }
                            }
                        }
                    }
                }
            }
        }
        if (Entrances.empty())
        {
            throw AtlasException("Kernel Exception: tik requires a body entrance");
        }
    }

    void CartographerKernel::BuildKernelFromBlocks(llvm::ValueToValueMapTy &VMap, set<BasicBlock *> &blocks)
    {
        set<Function *> headFunctions;
        for (const auto &ent : Entrances)
        {
            BasicBlock *eTarget = IDToBlock[ent->Block];
            headFunctions.insert(eTarget->getParent());
        }

        if (headFunctions.size() != 1)
        {
            throw AtlasException("Entrances not on same level");
        }

        for (auto block : blocks)
        {
            if (headFunctions.find(block->getParent()) == headFunctions.end())
            {
                continue;
            }
            int64_t id = GetBlockID(block);
            if (KernelMap.find(id) != KernelMap.end())
            {
                //this belongs to a subkernel
                auto nestedKernel = KernelMap[id];
                bool inNested = false;
                for (const auto &ent : nestedKernel->Entrances)
                {
                    if (IDToBlock[ent->Block] == block)
                    {
                        inNested = true;
                        break;
                    }
                }
                if (inNested)
                {
                    //we need to make a unique block for each entrance (there is currently only one)
                    for (uint64_t i = 0; i < nestedKernel->Entrances.size(); i++)
                    {
                        // values to go into arg operands of callinst
                        std::vector<llvm::Value *> inargs;
                        for (auto ai = nestedKernel->KernelFunction->arg_begin(); ai < nestedKernel->KernelFunction->arg_end(); ai++)
                        {
                            if (ai == nestedKernel->KernelFunction->arg_begin())
                            {
                                inargs.push_back(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), i));
                            }
                            else
                            {
                                if (VMap.find(ai) != VMap.end())
                                {
                                    inargs.push_back(VMap[ai]);
                                }
                                else
                                {
                                    inargs.push_back(IDToValue[nestedKernel->ArgumentMap[ai]]);
                                }
                            }
                        }

                        BasicBlock *intermediateBlock = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
                        blocks.insert(intermediateBlock);
                        IRBuilder<> intBuilder(intermediateBlock);
                        auto cc = intBuilder.CreateCall(nestedKernel->KernelFunction, inargs);
                        MDNode *tikNode = MDNode::get(TikModule->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt1Ty(TikModule->getContext()), 1)));
                        SetBlockID(intermediateBlock, IDState::Artificial);
                        cc->setMetadata("KernelCall", tikNode);
                        auto sw = intBuilder.CreateSwitch(cc, Exception, (uint32_t)nestedKernel->Exits.size());
                        for (const auto &exit : nestedKernel->Exits)
                        {
                            sw->addCase(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), (uint64_t)exit->Index), IDToBlock[exit->Block]);
                        }
                        VMap[block] = intermediateBlock;
                    }
                }
                else
                {
                    //this is a block from the nested kernel
                    //it doesn't need to be mapped
                }
            }
            else
            {
                auto cb = CloneBasicBlock(block, VMap, "", KernelFunction);
                VMap[block] = cb;
                // add medadata to this block to remember what its original predecessor was, for swapping later
                if (Conditional.find(block) != Conditional.end())
                {
                    Conditional.erase(block);
                    Conditional.insert(cb);
                }

                //fix the phis
                int rescheduled = 0; //the number of blocks we rescheduled
                for (auto bi = cb->begin(); bi != cb->end(); bi++)
                {
                    if (auto *p = dyn_cast<PHINode>(bi))
                    {
                        for (auto pred : p->blocks())
                        {
                            if (blocks.find(pred) == blocks.end())
                            {
                                //we have an invalid predecessor, replace with init
                                bool found = false;
                                for (const auto &ent : Entrances)
                                {
                                    if (IDToBlock[ent->Block] == block)
                                    {
                                        p->replaceIncomingBlockWith(pred, Init);
                                        rescheduled++;
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found)
                                {
                                    auto a = p->getBasicBlockIndex(pred);
                                    if (a >= 0)
                                    {
                                        p->removeIncomingValue(pred);
                                    }
                                }
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
        // now insert stores at every export site
        set<pair<Instruction *, Argument *>> storeSite;
        for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
        {
            auto block = cast<BasicBlock>(fi);
            for (auto bi = block->begin(); bi != block->end(); bi++)
            {
                auto inst = cast<Instruction>(bi);
                auto instID = GetValueID(inst);
                for (auto key : ArgumentMap)
                {
                    string name = key.first->getName();
                    if (name[0] == 'e')
                    {
                        if (key.second == instID)
                        {
                            storeSite.insert(pair(inst, key.first));
                        }
                    }
                }
            }
        }
        for (auto inst : storeSite)
        {
            IRBuilder<> stBuilder(inst.first->getParent());
            auto st = stBuilder.CreateStore(inst.first, inst.second);
            if (auto phi = dyn_cast<PHINode>(inst.first))
            {
                st->moveBefore(inst.first->getParent()->getFirstNonPHI());
            }
            else
            {
                st->moveAfter(inst.first);
            }
            auto newId = (uint64_t)IDState::Artificial;
            SetValueIDs(st, newId);
        }
    }

    void CartographerKernel::BuildInit(llvm::ValueToValueMapTy &VMap, vector<int64_t> &KernelExports)
    {
        IRBuilder<> initBuilder(Init);

        auto initSwitch = initBuilder.CreateSwitch(KernelFunction->arg_begin(), Exception, (uint32_t)Entrances.size());
        uint64_t i = 0;
        for (const auto &ent : Entrances)
        {
            int64_t id = ent->Block;
            if (KernelMap.find(id) == KernelMap.end() && (VMap.find(IDToBlock[ent->Block]) != VMap.end()))
            {
                initSwitch->addCase(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), i), cast<BasicBlock>(VMap[IDToBlock[ent->Block]]));
            }
            else
            {
                throw AtlasException("Unimplemented");
            }
            i++;
        }
    }

    void RemapOperands(User *op, Instruction *inst, llvm::ValueToValueMapTy &VMap)
    {
        if (remappedOperandSet.find(op) == remappedOperandSet.end())
        {
            IRBuilder Builder(inst);
            // if its a gep or load we need to make a new one (because gep and load args can't be changed after construction)
            if (auto gepInst = dyn_cast<GEPOperator>(op))
            {
                // duplicate the indexes of the GEP
                vector<Value *> idxList;
                for (auto idx = gepInst->idx_begin(); idx != gepInst->idx_end(); idx++)
                {
                    auto indexValue = cast<Value>(idx);
                    idxList.push_back(indexValue);
                }
                // find out what our pointer needs to be
                Value *ptr;
                if (auto gepPtr = dyn_cast<GlobalVariable>(gepInst->getPointerOperand()))
                {
                    if (gepPtr->getParent() != TikModule)
                    {
                        if (globalDeclarationSet.find(gepPtr) == globalDeclarationSet.end())
                        {
                            CopyOperand(gepInst, VMap);
                            ptr = VMap[gepPtr];
                            // sanity check
                            if (ptr == nullptr)
                            {
                                throw AtlasException("Declared global not mapped");
                            }
                        }
                        else
                        {
                            ptr = gepPtr;
                        }
                    }
                    else
                    {
                        ptr = gepPtr;
                    }
                    // finally, construct the new GEP and remap its old value
                    Value *newGep = Builder.CreateGEP(ptr, idxList, gepInst->getName());
                    VMap[cast<Value>(op)] = cast<Value>(newGep);
                }
                // we don't see a global here so don't replace the GEPOperator
                else
                {
                }
            }
            else if (auto loadInst = dyn_cast<LoadInst>(op))
            {
                // find out what our pointer needs to be
                Value *ptr;
                if (auto loadPtr = dyn_cast<GlobalVariable>(loadInst->getPointerOperand()))
                {
                    if (loadPtr->getParent() != TikModule)
                    {
                        if (globalDeclarationSet.find(loadPtr) == globalDeclarationSet.end())
                        {
                            CopyOperand(loadInst, VMap);
                            ptr = VMap[loadPtr];
                            // sanity check
                            if (ptr == nullptr)
                            {
                                throw AtlasException("Declared global not mapped");
                            }
                        }
                        else
                        {
                            ptr = loadPtr;
                        }
                    }
                    else
                    {
                        ptr = loadPtr;
                    }
                    Value *newLoad = Builder.CreateLoad(ptr, loadInst->getName());
                    VMap[cast<Value>(op)] = cast<Value>(newLoad);
                }
                // we don't see a global here so don't replace the loadInst
                else
                {
                }
            }
        }
        for (unsigned int operand = 0; operand < op->getNumOperands(); operand++)
        {
            Instruction *newInst = inst;
            if (auto test = dyn_cast<Instruction>(op))
            {
                newInst = test;
            }
            auto opi = op->getOperand(operand);
            if (opi != nullptr)
            {
                if (auto newGlob = dyn_cast<GlobalVariable>(opi))
                {
                    CopyOperand(newGlob, VMap);
                }
                else if (auto newOp = dyn_cast<Operator>(opi))
                {
                    if (remappedOperandSet.find(newOp) == remappedOperandSet.end())
                    {
                        remappedOperandSet.insert(newOp);
                        RemapOperands(newOp, newInst, VMap);
                    }
                }
            }
        }
    }

    void CartographerKernel::Remap(ValueToValueMapTy &VMap)
    {
        for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
        {
            auto BB = cast<BasicBlock>(fi);
            for (BasicBlock::iterator bi = BB->begin(); bi != BB->end(); bi++)
            {
                auto *inst = cast<Instruction>(bi);
                for (unsigned int arg = 0; arg < inst->getNumOperands(); arg++)
                {
                    Value *inputOp = inst->getOperand(arg);
                    if (auto op = dyn_cast<Operator>(inputOp))
                    {
                        RemapOperands(op, inst, VMap);
                    }
                }
                RemapInstruction(inst, VMap, llvm::RF_None);
            }
        }
    }

    void CartographerKernel::InlineFunctionsFromBlocks(std::set<int64_t> &blocks)
    {
        bool change = true;
        while (change)
        {
            change = false;
            for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
            {
                auto baseBlock = cast<BasicBlock>(fi);
                auto id = GetBlockID(baseBlock);
                if (blocks.find(id) == blocks.end())
                {
                    continue;
                }
                for (auto bi = fi->begin(); bi != fi->end(); bi++)
                {
                    if (auto ci = dyn_cast<CallBase>(bi))
                    {
                        if (auto debug = ci->getMetadata("KernelCall"))
                        {
                            continue;
                        }
                        auto id = GetBlockID(baseBlock);
                        auto info = InlineFunctionInfo();
                        auto r = InlineFunction(ci, info);
                        SetBlockID(baseBlock, id);
                        blocks.insert(id);
                        if (r)
                        {
                            change = true;
                        }
                        break;
                    }
                }
            }
        }
        //erase null blocks here
        auto blockList = &KernelFunction->getBasicBlockList();
        vector<Function::iterator> toRemove;

        for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
        {
            if (auto b = dyn_cast<BasicBlock>(fi))
            {
                //do nothing
            }
            else
            {
                toRemove.push_back(fi);
            }
        }

        for (auto r : toRemove)
        {
            blockList->erase(r);
        }

        //now that everything is inlined we need to remove invalid blocks
        //although some blocks are now an amalgamation of multiple,
        //as a rule we don't need to worry about those.
        //simple successors are enough
        vector<BasicBlock *> bToRemove;
        for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
        {
            auto block = cast<BasicBlock>(fi);
            int64_t id = GetBlockID(block);
            if (blocks.find(id) == blocks.end() && block != Exit && block != Init && block != Exception && id >= 0)
            {
                for (auto user : block->users())
                {
                    if (auto *phi = dyn_cast<PHINode>(user))
                    {
                        phi->removeIncomingValue(block);
                    }
                }
                bToRemove.push_back(block);
            }
        }
    }

    void CartographerKernel::CopyGlobals(llvm::ValueToValueMapTy &VMap)
    {
        for (auto &fi : *(KernelFunction))
        {
            for (auto bi = fi.begin(); bi != fi.end(); bi++)
            {
                auto inst = cast<Instruction>(bi);
                if (auto cv = dyn_cast<CallBase>(inst))
                {
                    for (auto i = cv->arg_begin(); i < cv->arg_end(); i++)
                    {
                        if (auto user = dyn_cast<User>(i))
                        {
                            CopyOperand(user, VMap);
                        }
                    }
                }
                else
                {
                    CopyOperand(inst, VMap);
                }
            }
        }
    }

    void CartographerKernel::PatchPhis(ValueToValueMapTy &VMap)
    {
        for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
        {
            auto b = cast<BasicBlock>(fi);

            vector<Instruction *> phisToRemove;
            for (auto &phi : b->phis())
            {
                vector<BasicBlock *> valuesToRemove;
                for (uint32_t i = 0; i < phi.getNumIncomingValues(); i++)
                {
                    auto block = phi.getIncomingBlock(i);
                    BasicBlock *rblock;
                    if (VMap.find(phi.getIncomingBlock(i)) != VMap.end())
                    {
                        rblock = cast<BasicBlock>(VMap[phi.getIncomingBlock(i)]);
                    }
                    else
                    {
                        rblock = block;
                    }
                    if (block->getParent() != KernelFunction && rblock->getParent() != KernelFunction)
                    {
                        valuesToRemove.push_back(block);
                    }
                    else
                    {
                        bool isPred = false;
                        for (auto pred : predecessors(b))
                        {
                            if (pred == block)
                            {
                                isPred = true;
                            }
                        }
                        if (!isPred)
                        {
                            valuesToRemove.push_back(block);
                            continue;
                        }
                    }
                }
                for (auto toR : valuesToRemove)
                {
                    phi.removeIncomingValue(toR, false);
                }
                if (phi.getNumIncomingValues() == 0)
                {
                    phisToRemove.push_back(&phi);
                    for (auto user : phi.users())
                    {
                        if (auto br = dyn_cast<BranchInst>(user))
                        {
                            if (br->isConditional())
                            {
                                auto b0 = br->getSuccessor(0);
                                auto b1 = br->getSuccessor(1);
                                if (b0 != b1)
                                {
                                    throw AtlasException("Phi successors don't match");
                                }
                                IRBuilder<> ib(br);
                                ib.CreateBr(b0);
                                phisToRemove.push_back(br);
                            }
                            else
                            {
                                throw AtlasException("Malformed phi user");
                            }
                        }
                        else
                        {
                            throw AtlasException("Unexpected phi user");
                        }
                    }
                }
            }
            for (auto phi : phisToRemove)
            {
                phi->eraseFromParent();
            }
        }
    }

    void CartographerKernel::BuildExit()
    {
        IRBuilder<> exitBuilder(Exit);
        int exitId = 0;
        auto ex = GetExits(KernelFunction);
        map<BasicBlock *, BasicBlock *> exitMap;
        for (auto exit : ex)
        {
            Exits.insert(make_shared<KernelInterface>(exitId++, GetBlockID(exit)));
            BasicBlock *tmp = BasicBlock::Create(TikModule->getContext(), "", KernelFunction);
            IRBuilder<> builder(tmp);
            builder.CreateBr(Exit);
            exitMap[exit] = tmp;
        }

        for (auto fi = KernelFunction->begin(); fi != KernelFunction->end(); fi++)
        {
            auto block = cast<BasicBlock>(fi);
            auto term = block->getTerminator();
            if (term != nullptr)
            {
                for (uint32_t i = 0; i < term->getNumSuccessors(); i++)
                {
                    auto suc = term->getSuccessor(i);
                    if (suc->getParent() != KernelFunction)
                    {
                        //we have an exit
                        term->setSuccessor(i, exitMap[suc]);
                    }
                }
            }
        }

        auto phi = exitBuilder.CreatePHI(Type::getInt8Ty(TikModule->getContext()), (uint32_t)Exits.size());
        for (const auto &exit : Exits)
        {
            if (exitMap.find(IDToBlock[exit->Block]) == exitMap.end())
            {
                throw AtlasException("Block not found in Exit Map!");
            }
            phi->addIncoming(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), (uint64_t)exit->Index), exitMap[IDToBlock[exit->Block]]);
        }
        exitBuilder.CreateRet(phi);

        IRBuilder<> exceptionBuilder(Exception);
        exceptionBuilder.CreateRet(ConstantInt::get(Type::getInt8Ty(TikModule->getContext()), (uint64_t)IDState::Artificial));
    }

    void CartographerKernel::FixInvokes()
    {
        auto F = TikModule->getOrInsertFunction("__gxx_personality_v0", Type::getInt32Ty(TikModule->getContext()));
        for (auto &fi : *KernelFunction)
        {
            for (auto bi = fi.begin(); bi != fi.end(); bi++)
            {
                if (auto ii = dyn_cast<InvokeInst>(bi))
                {
                    auto a = ii->getLandingPadInst();
                    if (isa<BranchInst>(a))
                    {
                        throw AtlasException("Exception handling is not supported");
                        auto unwind = ii->getUnwindDest();
                        auto term = unwind->getTerminator();
                        IRBuilder<> builder(term);
                        auto landing = builder.CreateLandingPad(Type::getVoidTy(TikModule->getContext()), 0);
                        landing->addClause(ConstantPointerNull::get(PointerType::get(Type::getVoidTy(TikModule->getContext()), 0)));
                        KernelFunction->setPersonalityFn(cast<Constant>(F.getCallee()));
                        spdlog::warn("Adding landingpad for non-inlinable Invoke Instruction. May segfault if exception is thrown.");
                    }
                    else
                    {
                        // will cause a "personality function from another module" module error
                        throw AtlasException("Could not deduce personality.")
                    }
                }
            }
        }
    }

    void CartographerKernel::RemapNestedKernels(llvm::ValueToValueMapTy &VMap)
    {
        // Now find all calls to the embedded kernel functions in the body, if any, and change their arguments to the new ones
        std::map<Argument *, Value *> embeddedCallArgs;
        for (auto &bf : *(KernelFunction))
        {
            for (BasicBlock::iterator i = bf.begin(), BE = bf.end(); i != BE; ++i)
            {
                if (auto *callInst = dyn_cast<CallInst>(i))
                {
                    auto calledFunc = callInst->getCalledFunction();
                    auto subK = KfMap[calledFunc];
                    if (subK != nullptr)
                    {
                        for (auto eCArg = calledFunc->arg_begin(); eCArg < calledFunc->arg_end(); eCArg++)
                        {
                            auto argMapID = subK->ArgumentMap[eCArg];
                            for (auto it = KernelFunction->arg_begin(); it != KernelFunction->arg_end(); it++)
                            {
                                auto parArg = cast<Argument>(it);
                                if (GetValueID(parArg) == argMapID)
                                {
                                    embeddedCallArgs[eCArg] = parArg;
                                }
                                else
                                {
                                    for (auto &b : *(KernelFunction))
                                    {
                                        for (BasicBlock::iterator j = b.begin(), BE2 = b.end(); j != BE2; ++j)
                                        {
                                            auto inst = cast<Instruction>(j);
                                            if (argMapID == GetValueID(inst))
                                            {
                                                embeddedCallArgs[eCArg] = inst;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        auto limit = callInst->getNumArgOperands();
                        for (uint32_t k = 1; k < limit; k++)
                        {
                            Value *op = callInst->getArgOperand(k);
                            if (auto *arg = dyn_cast<Argument>(op))
                            {
                                if (embeddedCallArgs.find(arg) == embeddedCallArgs.end())
                                {
                                    throw AtlasException("Failed to find nested argument");
                                }
                                auto newArg = embeddedCallArgs[arg];
                                callInst->setArgOperand(k, newArg);
                            }
                            else
                            {
                                throw AtlasException("Unexpected value passed to function");
                            }
                        }
                    }
                }
            }
        }
    }

} // namespace TraceAtlas::tik