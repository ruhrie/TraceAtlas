#include "tikSwap/tikSwap.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
#include "AtlasUtil/Print.h"
#include "tik/Kernel.h"
#include "tik/TikKernel.h"
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
    // create a map for its block and value IDs
    map<int64_t, BasicBlock *> baseBlockMap;
    for (auto &F : *sourceBitcode)
    {
        for (auto BB = F.begin(), BBe = F.end(); BB != BBe; BB++)
        {
            auto block = cast<BasicBlock>(BB);
            MDNode *md;
            if (block->getFirstInsertionPt()->hasMetadataOtherThanDebugLoc())
            {
                md = block->getFirstInsertionPt()->getMetadata("BlockID");
                if (md->getNumOperands() > 0)
                {
                    if (auto con = mdconst::dyn_extract<ConstantInt>(md->getOperand(0)))
                    {
                        int64_t blockID = con->getSExtValue();
                        baseBlockMap[blockID] = block;
                    }
                }
            }
        }
    }

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
                // make a copy of the kernel function for the sourceBitcode module context
                // we have to get the arg types from the source values first before we can make the function signature (to align context)
                vector<Value *> mappedVals;
                for (auto &a : kernel->ArgumentMap)
                {
                    // flag for seeing if we actually find a value to map to this kernel function
                    bool found = false;
                    // set the first arg to the entrance index
                    if (a.second == IDState::Artificial)
                    {
                        mappedVals.push_back(ConstantInt::get(Type::getInt8Ty(sourceBitcode->getContext()), (uint64_t)e->Index));
                        continue;
                    }
                    for (auto &F : *sourceBitcode)
                    {
                        for (auto BB = F.begin(), BBe = F.end(); BB != BBe; BB++)
                        {
                            auto block = cast<BasicBlock>(BB);
                            // get our value from the source bitcode
                            for (auto in = block->begin(), ine = block->end(); in != ine; in++)
                            {
                                auto inst = cast<Instruction>(in);
                                MDNode *mv = nullptr;
                                if (inst->hasMetadataOtherThanDebugLoc())
                                {
                                    mv = inst->getMetadata("ValueID");
                                }
                                else
                                {
                                    PrintVal(inst);
                                    spdlog::warn("Value in source bitcode has no ValueID.");
                                    continue;
                                }
                                int64_t ValueID = 0;
                                if (mv->getNumOperands() > 1)
                                {
                                    spdlog::warn("Value in source bitcode has more than one ID. Only looking at the first.");
                                }
                                else if (mv->getNumOperands() == 0)
                                {
                                    continue;
                                }
                                if (auto ID = mdconst::dyn_extract<ConstantInt>(mv->getOperand(0)))
                                {
                                    ValueID = ID->getSExtValue();
                                }
                                else
                                {
                                    ValueID = IDState::Artificial;
                                    spdlog::warn("Couldn't extract ValueID from source bitcode. Skipping...");
                                }
                                // now see if this value matches, and if so add it (in order)
                                if (a.second == ValueID)
                                {
                                    found = true;
                                    mappedVals.push_back(cast<Value>(inst));
                                }
                            }
                        }
                    }
                    if (!found)
                    {
                        throw AtlasException("Could not map function argument for entrance " + to_string(e->Index) + " of kernel " + kernel->Name + " to source bitcode.");
                    }
                }
                vector<Type *> argTypes;
                argTypes.reserve(mappedVals.size());
                for (auto arg : mappedVals)
                {
                    argTypes.push_back(arg->getType());
                }
                // create function signature to swap for this entrance
                auto FuTy = FunctionType::get(Type::getInt8Ty(sourceBitcode->getContext()), argTypes, false);
                auto newFunc = Function::Create(FuTy, kernel->KernelFunction->getLinkage(), kernel->KernelFunction->getAddressSpace(), "", sourceBitcode.get());
                newFunc->setName(kernel->KernelFunction->getName());
                // keep track of whether or not we actually swap this entrance
                bool swapped = false;
                for (auto &F : *sourceBitcode)
                {
                    for (auto BB = F.begin(), BBe = F.end(); BB != BBe; BB++)
                    {
                        auto block = cast<BasicBlock>(BB);
                        // get the block ID
                        MDNode *md = nullptr;
                        if (block->getFirstInsertionPt()->hasMetadataOtherThanDebugLoc())
                        {
                            md = block->getFirstInsertionPt()->getMetadata("BlockID");
                        }
                        else
                        {
                            spdlog::warn("Source code basic block has no metadata.");
                            continue;
                        }
                        int64_t BBID = 0;
                        if (md->getNumOperands() > 1)
                        {
                            spdlog::warn("BB in source bitcode has more than one ID. Only looking at the first.");
                        }
                        if (auto ID = mdconst::dyn_extract<ConstantInt>(md->getOperand(0)))
                        {
                            BBID = ID->getSExtValue();
                        }
                        else
                        {
                            BBID = IDState::Artificial;
                            spdlog::warn("Couldn't extract BBID from source bitcode. Skipping...");
                        }
                        // if this is our entrance block, swap tik
                        if (BBID == e->Block)
                        {
                            // create the call instruction to kernel
                            IRBuilder iBuilder(block->getTerminator());
                            auto baseFuncInst = cast<Function>(sourceBitcode->getOrInsertFunction(newFunc->getName(), newFunc->getFunctionType()).getCallee());
                            CallInst *KInst = iBuilder.CreateCall(baseFuncInst, mappedVals);
                            MDNode *callNode = MDNode::get(KInst->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt1Ty(KInst->getContext()), IDState::Artificial)));
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
                            auto sw = iBuilder.CreateSwitch(cast<Value>(KInst), baseBlockMap[a.Block], (unsigned int)kernel->Exits.size());
                            MDNode *swNode = MDNode::get(sw->getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt1Ty(sw->getContext()), IDState::Artificial)));
                            sw->setMetadata("ValueID", swNode);
                            for (auto &j : kernel->Exits)
                            {
                                if (j->Index != 0)
                                {
                                    sw->addCase(ConstantInt::get(Type::getInt8Ty(sourceBitcode->getContext()), (uint64_t)j->Index), baseBlockMap[j->Block]);
                                }
                            }
                            swapped = true;
                            // remove the old BB terminator
                            toRemove.insert(block->getTerminator());
                        }
                    }
                }
                if (!swapped)
                {
                    throw AtlasException("Could not swap entrance " + to_string(e->Index) + " of kernel " + kernel->Name + " to source bitcode.");
                }
                else
                {
                    spdlog::info("Successfully swapped entrance " + to_string(e->Index) + " of kernel: " + kernel->Name);
                }
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
