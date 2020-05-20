#include "tikSwap/tikSwap.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
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

/*
llvm::Type* getArgType(Type* ty, Module* base)
{
    if (ty->isVectorTy())
    {
        spdlog::warn("Argument type unimplemented.");
    }
    else if (ty->isArrayTy())
    {
        spdlog::warn("Argument type unimplemented.");
    }
    else if (ty->isFunctionTy())
    {
        spdlog::warn("Argument type unimplemented.");
    }
    else if (ty->isIntegerTy())
    {
        auto width = ty->getIntegerBitWidth();
        return Type::getIntNTy(base->getContext(), width);
    }
    else if (ty->isPointerTy())
    {
        int stars = 0;
        Type* newTy = ty;
        while (newTy->isPointerTy())
        {
            newTy = cast<PointerType
            newTy = newTy->getElementType();
            stars++;
        }
        PointerType* retType;
        for (int i = 0; i < recursions; i++)
        {
            retType = 
        }
        return getArgType(_p->getElementType(), base);
    }
    else if (ty->isStructTy())
    {
        spdlog::warn("Argument type unimplemented.");
    }
    else
    {
        spdlog::error("Argument type is not recognized.");
    }
}*/

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

    // swap in kernel functions to original bitcode
    llvm::ValueToValueMapTy VMap;
    // set that holds any branch instructions that need to be removed
    std::set<BranchInst*> toRemove;
    for (auto &kernel : kernels)
    {
        // make a copy of the kernel function in the base module context
        // we know ahead of time the kernel function will return i8
        // we have to get the arg types from the source values first before we can make the function signature
        // get the arg values first and put them in a vector
        vector<Value*> newArgs;
        for (auto &a : kernel->ArgumentMap)
        {
            // set the first arg to the constant index
            if (a.second == 0)
            {
                newArgs.push_back(ConstantInt::get(Type::getInt8Ty(base->getContext()), 0));//(uint64_t)e->Index));
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
                        // now see this value matches, and if it does, replace the argument in the callinst with the value here                
                        if (a.second == ValueID)
                        {
                            newArgs.push_back(cast<Value>(inst));
                        }
                    }
                }
            }
        }
        vector<Type*> argTypes;
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
                for (auto &e : kernel->Entrances)
                {
                    if (BBID == e->Block)
                    {
                        for (auto in = block->begin(), ine = block->end(); in != ine; in++)
                        {
                            if (auto inst = dyn_cast<Instruction>(in))
                            {
                                if (auto brInst = dyn_cast<BranchInst>(inst))
                                {
                                    // get rid of the branch and insert a CallInst
                                    //inst->removeFromParent();
                                    IRBuilder iBuilder(block->getTerminator());
                                    vector<Value *> inargs;
                                    for (auto argi = kernel->KernelFunction->arg_begin(); argi < kernel->KernelFunction->arg_end(); argi++)
                                    {
                                        if (argi == kernel->KernelFunction->arg_begin())
                                        {
                                            inargs.push_back(ConstantInt::get(Type::getInt8Ty(base->getContext()), (uint64_t)e->Index));
                                        }
                                        else
                                        {
                                            inargs.push_back(cast<Value>(argi));
                                        }
                                    }
                                    SmallVector<AttributeSet, 4> NewArgAttrs(kernel->KernelFunction->arg_size());
                                    // from CloneFunction.cpp, line 111                        
                                    // Clone any argument attributes that are present in the VMap.
                                    /*for (const Argument &OldArg : kernel->KernelFunction->args()) 
                                    {
                                        if (Argument *NewArg = dyn_cast<Argument>(VMap[&OldArg])) 
                                        {
                                            NewArgAttrs[NewArg->getArgNo()] = kernel->KernelFunction->getAttributes().getParamAttributes(OldArg.getArgNo());
                                        }
                                    }*/
                                    auto baseFuncInst = cast<Function>(base->getOrInsertFunction(newFunc->getName(), newFunc->getFunctionType()).getCallee());
                                    //baseFuncInst->setAttributes(AttributeList::get(base->getContext(), newFunc->getAttributes().getFnAttributes(), newFunc->getAttributes().getRetAttributes(), NewArgAttrs));
                                    CallInst *KInst = iBuilder.CreateCall(baseFuncInst, inargs);
                                    for (int i = 0; i < (int)KInst->getNumArgOperands(); i++)
                                    {
                                        KInst->setArgOperand((unsigned int)i, newArgs[i]);
                                    }
                                    /* insert correct values for each argument in the callinst from the bitcode */
                                    /*for (auto &a : kernel->ArgumentMap)
                                    {
                                        // set the first arg to the constant index
                                        cout << a.second << endl;
                                        if (a.second == 0)
                                        {
                                            KInst->setArgOperand(0, ConstantInt::get(Type::getInt8Ty(base->getContext()), (uint64_t)e->Index));
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
                                                    // now see this value matches, and if it does, replace the argument in the callinst with the value here                
                                                    if (a.second == ValueID)
                                                    {
                                                        for (int i = 0; i < (int)KInst->getNumArgOperands(); i++)
                                                        {
                                                            if (auto InstArg = dyn_cast<Argument>(KInst->getArgOperand((unsigned int)i)))
                                                            {
                                                                if (a.first == InstArg)
                                                                {
                                                                    KInst->setArgOperand((unsigned int)i, cast<Value>(inst));
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }*/
                                    Value *correctExit = KInst;
                                    // loop over exit indices in the phi
                                    for (unsigned int i = 0; i < kernel->Exits.size(); i++)
                                    {
                                        // find the blockID of our index
                                        int64_t blockID = -1;
                                        for (auto &j : kernel->Exits)
                                        {
                                            if (j->Index == (int)i)
                                            {
                                                blockID = j->Block;
                                            }
                                        }
                                        // if this is the first index, just assume its the right answer
                                        if (i == 0)
                                        {
                                            correctExit = baseBlockMap[blockID];
                                            continue;
                                        }
                                        auto cmpInst = cast<ICmpInst>(iBuilder.CreateICmpEQ(baseBlockMap[blockID], KInst));
                                        auto sInst = cast<SelectInst>(iBuilder.CreateSelect(cmpInst, baseBlockMap[blockID], correctExit));
                                        correctExit = sInst;
                                    }
                                    iBuilder.CreateBr(cast<BasicBlock>(correctExit));
                                    // inline
                                    /*auto info = InlineFunctionInfo();
                                    auto r = InlineFunction(KInst, info);
                                    string Name = newFunc->getName();
                                    if (r)
                                    {
                                        spdlog::info("Successfully placed "+Name+" into source bitcode.");
                                    }
                                    else
                                    {
                                        AtlasException("Failed to inline function "+Name);
                                    }
                                    // now resolve the exit
                                    // take the return of the inlined function and put it into a phi to choose the correct exit
                                    // finally, remove old branch inst*/
                                    toRemove.insert(brInst);
                                }
                            }
                            else
                            {
                                spdlog::warn("Found a null instruction in a source block. Skipping...");
                                continue;
                            }
                        }
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
    for (auto &F : *base)
    {
        for (auto fi = F.begin(); fi != F.end(); fi++)
        {
            auto BB = cast<BasicBlock>(fi);
            for (BasicBlock::iterator bi = BB->begin(); bi != BB->end(); bi++)
            {
                auto *inst = cast<Instruction>(bi);
                RemapInstruction(inst, VMap, llvm::RF_None);
            }
        }
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
        if (false)
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
        std::cerr << "Failed to open output file: " << OutputFile << "\n";
        std::cerr << e.what() << '\n';
        spdlog::critical("Failed to write tik to output file: " + OutputFile);
        return EXIT_FAILURE;
    }
    return 0;
}
