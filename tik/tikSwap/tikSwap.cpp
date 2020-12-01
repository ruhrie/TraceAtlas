#include "tikSwap/tikSwap.h"
#include "AtlasUtil/Exceptions.h"
#include "AtlasUtil/Format.h"
#include "AtlasUtil/Print.h"
#include "tik/Kernel.h"
#include "tik/TikKernel.h"
#include "tik/Util.h"
#include <fstream>
#include <iostream>
#include <llvm/Analysis/CFG.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/AssemblyAnnotationWriter.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <string>

using namespace std;
using namespace llvm;
using namespace TraceAtlas::tik;

cl::opt<string> InputFile("t", cl::Required, cl::desc("<input tik bitcode>"), cl::init("tik.bc"));
cl::opt<string> OriginalBitcode("b", cl::Required, cl::desc("<input original bitcode>"), cl::init("a.bc"));
cl::opt<string> OutputFile("o", cl::desc("Specify output filename"), cl::value_desc("output filename"), cl::init("tikSwap.bc"));
cl::opt<bool> ASCIIFormat("S", cl::desc("Output json as human-readable ASCII text"));
cl::opt<bool> Debug("g", cl::desc("Inject debugging symbols into output bitcode"));
cl::opt<bool> Preformat("pf", llvm::cl::desc("Split and annotate source bitcode blocks and values before processing."));

int main(int argc, char *argv[])
{
    cl::ParseCommandLineOptions(argc, argv);
    spdlog::info("Starting tikSwap.");
    // load the original bitcode
    LLVMContext OContext;
    SMDiagnostic Osmerror;
    unique_ptr<Module> sourceBitcode;
    try
    {
        sourceBitcode = parseIRFile(OriginalBitcode, Osmerror, OContext);
        sourceBitcode->setSourceFileName(OriginalBitcode);
    }
    // if the file was not found
    catch (exception &e)
    {
        spdlog::critical("Failed to open source bitcode file: " + OriginalBitcode);
        return EXIT_FAILURE;
    }
    // if the parsing failed
    if (sourceBitcode == nullptr)
    {
        spdlog::critical("Couldn't parse source bitcode: " + OriginalBitcode);
        return EXIT_FAILURE;
    }
    // Annotate its bitcodes and values
    CleanModule(sourceBitcode.get());
    if (!Preformat)
    {
        Format(sourceBitcode.get());
    }
    // create a map for its block and value IDs
    map<int64_t, BasicBlock *> baseBlockMap;
    // change all non-external function definitions to default linkage type
    for (auto &F : *sourceBitcode)
    {
        if (!F.hasExternalLinkage())
        {
            F.setLinkage(GlobalValue::ExternalLinkage);
        }
    }
    // Initialize our IDtoX maps
    InitializeIDMaps(sourceBitcode.get(), IDToBlock, IDToValue);

    // load the tik IR
    LLVMContext tikContext;
    SMDiagnostic tikSmerror;
    unique_ptr<Module> tikBitcode;
    try
    {
        tikBitcode = parseIRFile(InputFile, tikSmerror, tikContext);
    }
    // if the file was not found
    catch (exception &e)
    {
        spdlog::critical("Failed to open tik bitcode file: " + InputFile);
        return EXIT_FAILURE;
    }
    // if the parsing failed
    if (tikBitcode == nullptr)
    {
        string err;
        raw_string_ostream rso(err);
        tikSmerror.print(InputFile.data(), rso);
        spdlog::critical("Couldn't parse tik bitcode:\n" + rso.str());
        return EXIT_FAILURE;
    }

    // grab all kernel functions in the tik bitcode and construct objects from them
    vector<shared_ptr<TikKernel>> kernels;
    for (auto &func : *tikBitcode)
    {
        if (func.hasMetadata("Boundaries"))
        {
            auto kern = make_shared<TikKernel>(tikBitcode->getFunction(func.getName()));
            if (kern->Valid)
            {
                kernels.push_back(kern);
            }
            else
            {
                spdlog::error("Kernel function " + kern->Name + " was not a valid TikKernel.");
            }
        }
    }
    spdlog::info("Found " + to_string(kernels.size()) + " valid kernels in the tik file.");

    // set that holds any branch instructions that need to be removed
    set<Instruction *> toRemove;
    // container of all exports that have been mapped to a pointer
    map<Value *, AllocaInst *> mappedExports;
    // set of sites to store to alloc instructions (which are export pointers)
    set<pair<Instruction *, AllocaInst *>> storeSite;
    // tikSwap
    for (auto &kernel : kernels)
    {
        for (auto &e : kernel->Entrances)
        {
            try
            {
                // look for exits that are not expecting this predecessor
                auto enterBlock = IDToBlock[e->Block];
                for (const auto &exit : kernel->Exits)
                {
                    auto succ = IDToBlock[exit->Block];
                    if (find(predecessors(succ).begin(), predecessors(succ).end(), enterBlock) == predecessors(succ).end())
                    {
                        // have to evaluate if the block is dependent on its predecessors
                        // search for a phi
                        bool found = true;
                        if (succ->hasNPredecessorsOrMore(2))
                        {
                            for (auto it = succ->begin(); it != succ->end(); it++)
                            {
                                if (auto phi = dyn_cast<PHINode>(it))
                                {
                                    found = false;
                                    for (unsigned int k = 0; k < phi->getNumIncomingValues(); k++)
                                    {
                                        if (phi->getIncomingBlock(k) == succ)
                                        {
                                            found = true;
                                        }
                                    }
                                }
                            }
                        }
                        if (!found)
                        {
                            // we have a phi that is not expecting us as an incoming value. Which export should it receive?
                            throw AtlasException("Kernel exit is an unexpected phi predecessor!");
                        }
                    }
                }
                // list of values that directly map to the list of kernel function args
                vector<Value *> mappedVals;
                vector<Type *> argTypes;
                // initialize the entrance index arg
                mappedVals.push_back(ConstantInt::get(Type::getInt8Ty(sourceBitcode->getContext()), (uint64_t)e->Index));
                argTypes.push_back(mappedVals[0]->getType());
                for (auto it = next(kernel->ArgumentMap.begin()); it != kernel->ArgumentMap.end(); it++)
                {
                    if (IDToValue.find(it->second) == IDToValue.end())
                    {
                        throw AtlasException("Could not map arg back to source bitcode.");
                    }
                    mappedVals.push_back(IDToValue[it->second]);
                    if (it->first->getName()[0] == 'e')
                    {
                        auto ptr = IDToValue[it->second]->getType()->getPointerTo();
                        argTypes.push_back(cast<Type>(ptr));
                    }
                    else
                    {
                        argTypes.push_back(IDToValue[it->second]->getType());
                    }
                }
                // create function signature to swap for this entrance
                auto FuTy = FunctionType::get(Type::getInt8Ty(sourceBitcode->getContext()), argTypes, false);
                auto newFunc = Function::Create(FuTy, kernel->KernelFunction->getLinkage(), kernel->KernelFunction->getAddressSpace(), "", sourceBitcode.get());
                newFunc->setName(kernel->KernelFunction->getName());
                // get our entrance predecessor blocks and swap tik
                if (IDToBlock.find(e->Block) != IDToBlock.end())
                {
                    auto block = IDToBlock[e->Block];
                    // create the call instruction to kernel
                    auto origterm = block->getTerminator();
                    IRBuilder iBuilder(block);
                    auto baseFuncInst = cast<Function>(sourceBitcode->getOrInsertFunction(newFunc->getName(), newFunc->getFunctionType()).getCallee());
                    CallInst *KInst = iBuilder.CreateCall(baseFuncInst, mappedVals);
                    MDNode *callNode = MDNode::get(KInst->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt1Ty(KInst->getContext()), (uint64_t)IDState::Artificial)));
                    KInst->setMetadata("ValueID", callNode);
                    // create switch case to process retval from kernel callinst
                    KernelInterface a(IDState::Artificial, IDState::Artificial);
                    for (auto &j : kernel->Exits)
                    {
                        if (j->Index == 0)
                        {
                            a.Block = j->Block;
                            a.Index = j->Index;
                        }
                    }
                    auto sw = iBuilder.CreateSwitch(cast<Value>(KInst), IDToBlock[a.Block], (unsigned int)kernel->Exits.size());
                    MDNode *swNode = MDNode::get(sw->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt1Ty(sw->getContext()), (uint64_t)IDState::Artificial)));
                    sw->setMetadata("ValueID", swNode);
                    for (auto &j : kernel->Exits)
                    {
                        if (j->Index != 0)
                        {
                            sw->addCase(ConstantInt::get(Type::getInt8Ty(sourceBitcode->getContext()), (uint64_t)j->Index), IDToBlock[j->Block]);
                        }
                    }
                    // remove the old BB terminator
                    toRemove.insert(origterm);
                    // now alloc export pointers in the entrance context and insert loads wherever they're used, and stores wherever the original value appears in the bitcode
                    for (auto key : kernel->ArgumentMap)
                    {
                        string name = key.first->getName();
                        if (name[0] == 'e')
                        {
                            // first, generate alloca in the first basic block of the parent function
                            AllocaInst *alloc;
                            if (mappedExports.find(IDToValue[key.second]) == mappedExports.end())
                            {
                                auto insertion = cast<Instruction>(sw->getParent()->getParent()->getEntryBlock().getFirstInsertionPt()->getNextNode());
                                IRBuilder<> alBuilder(sw->getParent()->getParent()->getEntryBlock().getFirstInsertionPt()->getParent());
                                alloc = iBuilder.CreateAlloca(IDToValue[key.second]->getType());
                                SetIDAndMap(alloc, IDToValue, true);
                                alloc->moveBefore(insertion);
                                mappedExports[IDToValue[key.second]] = alloc;
                                if (auto originalInst = dyn_cast<Instruction>(IDToValue[key.second]))
                                {
                                    // if the original instruction is not within the kernel function parent, we can't do this
                                    if (originalInst->getParent()->getParent() == alloc->getParent()->getParent())
                                    {
                                        storeSite.insert(pair(originalInst, alloc));
                                    }
                                }
                            }
                            else
                            {
                                alloc = mappedExports[IDToValue[key.second]];
                            }
                            // next, inject a store to the export pointer right after the original value was used
                            vector<pair<Value *, Instruction *>> toReplace;
                            // finally, create a load and replace the old value for every use of this export
                            for (auto use : IDToValue[key.second]->users())
                            {
                                if (auto inst = dyn_cast<Instruction>(use))
                                {
                                    // skip the kernel calls
                                    if (auto calli = dyn_cast<CallInst>(inst))
                                    {
                                        if (calli->getCalledFunction() == baseFuncInst)
                                        {
                                            continue;
                                        }
                                    }
                                    // skip the exports that exist in different contexts
                                    if (inst->getParent()->getParent() != KInst->getParent()->getParent())
                                    {
                                        continue;
                                    }
                                    // do not do memory operations in the entrance block, unless the user is a phi
                                    if (inst->getParent() == IDToBlock[e->Block] && dyn_cast<PHINode>(inst) == nullptr)
                                    {
                                        continue;
                                    }
                                    toReplace.emplace_back(pair(IDToValue[key.second], inst));
                                }
                            }
                            KInst->replaceUsesOfWith(IDToValue[key.second], alloc);
                            for (auto pa : toReplace)
                            {
                                if (auto phi = dyn_cast<PHINode>(pa.second))
                                {
                                    BasicBlock *predBlock;
                                    for (unsigned int i = 0; i < phi->getNumIncomingValues(); i++)
                                    {
                                        if (phi->getIncomingValue(i) == pa.first)
                                        {
                                            predBlock = phi->getIncomingBlock(i);
                                            auto term = predBlock->getTerminator();
                                            IRBuilder<> ldBuilder(predBlock);
                                            auto ld = ldBuilder.CreateLoad(alloc);
                                            ld->moveBefore(term);
                                            phi->setIncomingValue(i, ld);
                                        }
                                    }
                                }
                                else if (auto inst = dyn_cast<Instruction>(pa.second))
                                {
                                    IRBuilder<> ldBuilder(inst->getParent());
                                    auto ld = ldBuilder.CreateLoad(alloc);
                                    ld->moveBefore(inst);
                                    inst->replaceUsesOfWith(pa.first, ld);
                                }
                            }
                        }
                    }
                }
                else
                {
                    throw AtlasException("Could not swap entrance " + to_string(e->Index) + " of kernel " + kernel->Name + " to source bitcode.");
                }
                spdlog::info("Successfully swapped entrance " + to_string(e->Index) + " of kernel: " + kernel->Name);
            }
            catch (AtlasException &e)
            {
                spdlog::error(e.what());
            }
        }
    }

    set<Instruction *> badInst;
    // remove blocks if any
    for (auto ind : toRemove)
    {
        // if the successors of this terminator only have the terminator parent as a pred, any uses of the successors values will be unresolved
        if (auto br = dyn_cast<BranchInst>(ind))
        {
            for (unsigned int i = 0; i < br->getNumSuccessors(); i++)
            {
                auto succ = br->getSuccessor(i);
                if (succ->hasNPredecessors(1))
                {
                    // for each inst in the now predecessor-less block
                    for (auto &it : *succ)
                    {
                        for (auto use : it.users())
                        {
                            if (auto useInst = dyn_cast<Instruction>(use))
                            {
                                if (!useInst->isTerminator())
                                {
                                    // if this unresolved use is not a terminator and has no uses, it can be trivially deleted
                                    if (useInst->hasNUses(0))
                                    {
                                        badInst.insert(useInst);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        ind->eraseFromParent();
    }
    for (auto inst : badInst)
    {
        for (auto pa : storeSite)
        {
            if (inst == pa.first)
            {
                storeSite.erase(pa);
            }
        }
        inst->eraseFromParent();
    }

    // insert stores at the original export sites
    for (auto pa : storeSite)
    {
        IRBuilder<> stBuilder(pa.first->getParent());
        auto st = stBuilder.CreateStore(pa.first, pa.second);
        auto insertion = pa.first;
        if (auto phi = dyn_cast<PHINode>(pa.first))
        {
            insertion = pa.first->getParent()->getFirstNonPHI();
        }
        st->moveAfter(insertion);
    }

    // now verify that phis only contain valid entries
    for (auto &fi : *sourceBitcode)
    {
        for (auto &bi : fi)
        {
            // very weird phenomenon here, but essentially we have to collect all phi entries to delete first, then delete them
            // when all phi entries are deleted, llvm deletes the phi, so to avoid segfault we can't remove until we're done with the block
            vector<pair<PHINode *, unsigned int>> remInd;
            for (auto it = bi.begin(); it != bi.end(); it++)
            {
                if (it.getNodePtr() != nullptr)
                {
                    if (auto phi = dyn_cast<PHINode>(it))
                    {
                        // each index to remove has to be the index that results after all previous entries to remove are out
                        unsigned int j = 0;
                        for (unsigned int i = 0; i < phi->getNumIncomingValues(); i++)
                        {
                            if (find(predecessors(phi->getParent()).begin(), predecessors(phi->getParent()).end(), phi->getIncomingBlock(i)) == predecessors(phi->getParent()).end())
                            {
                                remInd.emplace_back(pair(phi, j));
                            }
                            else
                            {
                                j++;
                            }
                        }
                    }
                }
            }
            for (auto ind : remInd)
            {
                ind.first->removeIncomingValue(ind.second);
            }
        }
    }

    // verify the module
    VerifyModule(sourceBitcode.get());

    // write bitcode to file
    return PrintFile(sourceBitcode.get(), OutputFile, ASCIIFormat, Debug);
}