#include "tikSwap/tikSwap.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
#include "AtlasUtil/Print.h"
#include "tik/Kernel.h"
#include "tik/TikKernel.h"
#include "tik/Util.h"
#include <fstream>
#include <iostream>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/AssemblyAnnotationWriter.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_os_ostream.h>
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
cl::opt<bool> ASCIIFormat("S", cl::desc("output json as human-readable ASCII text"));

int main(int argc, char *argv[])
{
    cl::ParseCommandLineOptions(argc, argv);
    spdlog::info("Starting tikSwap.");
    //load the original bitcode
    LLVMContext OContext;
    SMDiagnostic Osmerror;
    unique_ptr<Module> sourceBitcode;
    try
    {
        sourceBitcode = parseIRFile(OriginalBitcode, Osmerror, OContext);
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
    Annotate(sourceBitcode.get());
    // change all non-external function definitions to default linkage type
    for (auto &F : *sourceBitcode)
    {
        if (!F.hasExternalLinkage())
        {
            F.setLinkage(GlobalValue::ExternalLinkage);
        }
    }
    /// Initialize our IDtoX maps
    InitializeIDMaps(sourceBitcode.get());

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
    vector<TikKernel *> kernels;
    for (auto &func : *tikBitcode)
    {
        if (func.hasMetadata("Boundaries"))
        {
            auto kern = new TikKernel(tikBitcode->getFunction(func.getName()));
            if (kern->Valid)
            {
                kernels.push_back(kern);
            }
            else
            {
                delete kern;
            }
        }
    }
    spdlog::info("Found " + to_string(kernels.size()) + " valid kernels in the tik file.");

    // set that holds any branch instructions that need to be removed
    std::set<Instruction *> toRemove;
    // tikSwap
    for (auto &kernel : kernels)
    {
        for (auto &e : kernel->Entrances)
        {
            try
            {
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
                // get our entrance block and swap tik
                if (IDToBlock.find(e->Block) != IDToBlock.end())
                {
                    auto block = IDToBlock[e->Block];
                    // create the call instruction to kernel
                    IRBuilder iBuilder(block->getTerminator());
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
                    toRemove.insert(block->getTerminator());
                    // now alloc export pointers in the entrance context and insert loads wherever they're used
                    for (auto key : kernel->ArgumentMap)
                    {
                        string name = key.first->getName();
                        if (name[0] == 'e')
                        {
                            auto alloc = iBuilder.CreateAlloca(IDToValue[key.second]->getType());
                            auto insertion = cast<Instruction>(block->getFirstInsertionPt());
                            alloc->moveBefore(insertion);
                            set<pair<Value *, Instruction *>> toReplace;
                            for (auto use : IDToValue[key.second]->users())
                            {
                                if (auto inst = dyn_cast<Instruction>(use))
                                {
                                    if (auto calli = dyn_cast<CallInst>(inst))
                                    {
                                        if (calli->getCalledFunction() == baseFuncInst)
                                        {
                                            continue;
                                        }
                                    }
                                    toReplace.insert(pair(IDToValue[key.second], inst));
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
                                        }
                                    }
                                    auto term = predBlock->getTerminator();
                                    IRBuilder<> ldBuilder(predBlock);
                                    auto ld = ldBuilder.CreateLoad(alloc);
                                    ld->moveBefore(term);
                                    phi->replaceUsesOfWith(pa.first, ld);
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
    // remove blocks if any
    for (auto ind : toRemove)
    {
        ind->eraseFromParent();
    }

    //verify the module
    std::string str;
    llvm::raw_string_ostream rso(str);
    bool broken = verifyModule(*sourceBitcode, &rso);
    if (broken)
    {
        auto err = rso.str();
        spdlog::critical("Tik Module Corrupted: \n" + err);
    }

    // writing part
    try
    {
        if (ASCIIFormat)
        {
            // print human readable tik module to file
            auto *write = new llvm::AssemblyAnnotationWriter();
            std::string str;
            llvm::raw_string_ostream rso(str);
            std::filebuf f0;
            f0.open(OutputFile, std::ios::out);
            sourceBitcode->print(rso, write);
            std::ostream readableStream(&f0);
            readableStream << str;
            f0.close();
        }
        else
        {
            // non-human readable IR
            std::filebuf f;
            f.open(OutputFile, std::ios::out);
            std::ostream rawStream(&f);
            raw_os_ostream raw_stream(rawStream);
            WriteBitcodeToFile(*sourceBitcode, raw_stream);
        }
        spdlog::info("Successfully wrote tikSwap to file");
    }
    catch (exception &e)
    {
        spdlog::critical("Failed to open output file: " + OutputFile + ":\n" + e.what() + "\n");
        return EXIT_FAILURE;
    }
    return 0;
}
