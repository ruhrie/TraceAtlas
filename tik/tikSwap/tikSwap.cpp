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
    catch (exception &e)
    {
        std::cerr << "Couldn't open input bitcode file: " << OriginalBitcode << "\n";
        std::cerr << e.what() << '\n';
        spdlog::critical("Failed to open source bitcode: " + OriginalBitcode);
        return EXIT_FAILURE;
    }
    if (sourceBitcode == nullptr)
    {
        std::cerr << "Couldn't open input bitcode file: " << OriginalBitcode << "\n";
        spdlog::critical("Failed to open source bitcode: " + OriginalBitcode);
        return EXIT_FAILURE;
    }
    // Annotate its bitcodes and values
    Module *base = sourceBitcode.get();
    CleanModule(base);
    Annotate(base);
    // create a map for its block and value IDs
    map<int64_t, BasicBlock *> baseBlockMap;
    for (auto &F : *base)
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
    catch (exception &e)
    {
        std::cerr << "Couldn't open input bitcode file: " << InputFile << "\n";
        std::cerr << e.what() << '\n';
        spdlog::critical("Failed to open source bitcode: " + InputFile);
        return EXIT_FAILURE;
    }
    if (tikBitcode == nullptr)
    {
        std::cerr << "Couldn't open input bitcode file: " << InputFile << "\n";
        spdlog::critical("Failed to open source bitcode: " + InputFile);
        return EXIT_FAILURE;
    }
    Module *tikModule = tikBitcode.get();

    // grab all kernel functions in the tik bitcode and construct objects from them
    vector<Function *> kernelFuncs;
    for (auto &func : *tikModule)
    {
        string funcName = func.getName();
        if (funcName == "K" + to_string(kernelFuncs.size()))
        {
            kernelFuncs.push_back(tikModule->getFunction(funcName));
        }
    }
    vector<TikKernel *> kernels;
    for (auto func : kernelFuncs)
    {
        auto kern = new TikKernel(func);
        if (kern->Valid)
        {
            kernels.push_back(kern);
        }
        else
        {
            delete kern;
        }
    }

    // set that holds any branch instructions that need to be removed
    std::set<Instruction *> toRemove;
    // tikSwap
    for (auto &kernel : kernels)
    {
        for (auto &e : kernel->Entrances)
        {
            // make a copy of the kernel function for the base module context
            // we have to get the arg types from the source values first before we can make the function signature (to align context)
            vector<Value *> newArgs;
            for (auto &a : kernel->ArgumentMap)
            {
                // set the first arg to the entrance index
                if (a.second == 0)
                {
                    newArgs.push_back(ConstantInt::get(Type::getInt8Ty(base->getContext()), (uint64_t)e->Index));
                    continue;
                }
                for (auto &F : *base)
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
                                ValueID = -1;
                                spdlog::warn("Couldn't extract ValueID from source bitcode. Skipping...");
                            }
                            // now see if this value matches, and if so add it (in order)
                            if (a.second == ValueID)
                            {
                                newArgs.push_back(cast<Value>(inst));
                            }
                        }
                    }
                }
            }
            vector<Type *> argTypes;
            argTypes.reserve(newArgs.size());
            for (auto arg : newArgs)
            {
                argTypes.push_back(arg->getType());
            }
            auto FuTy = FunctionType::get(Type::getInt8Ty(base->getContext()), argTypes, false);
            auto newFunc = Function::Create(FuTy, kernel->KernelFunction->getLinkage(), kernel->KernelFunction->getAddressSpace(), "", base);
            newFunc->setName(kernel->KernelFunction->getName());
            for (auto &F : *base)
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
                        BBID = -1;
                        spdlog::warn("Couldn't extract BBID from source bitcode. Skipping...");
                    }
                    // if this is our entrance block, swap tik
                    if (BBID == e->Block)
                    {
                        IRBuilder iBuilder(block->getTerminator());
                        auto baseFuncInst = cast<Function>(base->getOrInsertFunction(newFunc->getName(), newFunc->getFunctionType()).getCallee());
                        CallInst *KInst = iBuilder.CreateCall(baseFuncInst, newArgs);
                        for (int i = 0; i < (int)KInst->getNumArgOperands(); i++)
                        {
                            if (e->Index != 0)
                            {
                                newArgs[0] = ConstantInt::get(Type::getInt8Ty(base->getContext()), (uint64_t)e->Index);
                            }
                            KInst->setArgOperand((unsigned int)i, newArgs[(size_t)i]);
                        }
                        KernelInterface a(-1, -1);
                        for (auto &j : kernel->Exits)
                        {
                            if (j->Index == 0)
                            {
                                a.Block = j->Block;
                                a.Index = j->Index;
                            }
                        }
                        auto sw = iBuilder.CreateSwitch(cast<Value>(KInst), baseBlockMap[a.Block], (unsigned int)kernel->Exits.size());
                        for (auto &j : kernel->Exits)
                        {
                            if (j->Index != 0)
                            {
                                sw->addCase(ConstantInt::get(Type::getInt8Ty(base->getContext()), (uint64_t)j->Index), baseBlockMap[j->Block]);
                            }
                        }
                        // remember to remove the old terminator
                        toRemove.insert(block->getTerminator());
                    }
                }
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
    bool broken = verifyModule(*base, &rso);
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
            base->print(rso, write);
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
            WriteBitcodeToFile(*base, raw_stream);
        }
        spdlog::info("Successfully wrote tik to file");
    }
    catch (exception &e)
    {
        spdlog::critical("Failed to open output file: " + OutputFile + ":\n" + e.what() + "\n");
        return EXIT_FAILURE;
    }
    return 0;
}
