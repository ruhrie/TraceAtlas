#include "tikSwap/tikSwap.h"
#include "AtlasUtil/Exceptions.h"
#include "AtlasUtil/Annotate.h"
#include "tik/Kernel.h"
#include "tik/TikKernel.h"
#include <iostream>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/IRBuilder.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <string>

using namespace std;
//using namespace Kernel;
using namespace llvm;
using namespace TraceAtlas::tik;

cl::opt<string> InputFile("t", cl::Required, cl::desc("<input tik bitcode>"), cl::init("tik.bc"));
cl::opt<string> OriginalBitcode("b", cl::Required, cl::desc("<input original bitcode>"), cl::init("a.bc"));
cl::opt<string> OutputFile("o", cl::desc("Specify output filename"), cl::value_desc("output filename"), cl::init("tikSwap.bc"));

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
    for (auto &kernel : kernels)
    {
        // if we put this kernel into the source, this flips to signify the need to remap args
        bool placed = false;
        for (auto &F : *base)
        {
            for (auto BB = F.begin(), BBe = F.end(); BB != BBe; BB++)
            {
                auto block = cast<BasicBlock>(BB);
                // get the block ID
                MDNode* md = block->getFirstInsertionPt()->getMetadata("BlockID");
                if (md == nullptr)
                {
                    AtlasException("Could not find BlockID metadata for basic block in source bitcode.");
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
                        cout << "Found block " << BBID << " that enters " << kernel->Name << endl;
                        for (auto in = block->begin(), ine = block->end(); in != ine; in++)
                        {
                            if (auto inst = dyn_cast<Instruction>(in))
                            {
                                if (auto brInst = dyn_cast<BranchInst>(inst))
                                {
                                    // get rid of the branch and insert a CallInst
                                    //inst->removeFromParent();
                                    IRBuilder iBuilder(block->getTerminator());
                                    vector<Value*> inargs;
                                    for ( auto argi = kernel->KernelFunction->arg_begin(); argi < kernel->KernelFunction->arg_end(); argi++)
                                    {
                                        if (argi == kernel->KernelFunction->arg_begin())
                                        {
                                            inargs.push_back(ConstantInt::get(Type::getInt8Ty(base->getContext()), e->Index));
                                        }
                                        else
                                        {
                                            inargs.push_back(cast<Value>(argi));
                                        }   
                                    }
                                    CallInst* KInst = iBuilder.CreateCall(kernel->KernelFunction, inargs);
                                    // need to insert branches here to valid exits

                                    // finally, remove old branch inst
                                    brInst->removeFromParent();
                                    placed = true;
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
        if (placed)
        {
            for (auto &F : *base)
            {
                for (auto BB = F.begin(), BBe = F.end(); BB != BBe; BB++)
                {
                    auto block = cast<BasicBlock>(BB);
                    for (auto in = block->begin(), ine = block->end(); in != ine; in++)
                    {
                        auto inst = cast<Instruction>(in);
                        // get the value ID
                        MDNode* md = inst->getMetadata("ValueID");
                        if (md == nullptr)
                        {
                            AtlasException("Could not find ValueID metadata for value in source bitcode.");
                        }
                        int64_t ValueID = 0;
                        if (md->getNumOperands() > 1)
                        {
                            spdlog::warn("Value in source bitcode has more than one ID. Only looking at the first.");
                        }
                        if (auto ID = mdconst::dyn_extract<ConstantInt>(md->getOperand(0)))
                        {
                            ValueID = ID->getSExtValue();
                        }
                        else
                        {
                            ValueID = -1;
                            spdlog::warn("Couldn't extract ValueID from source bitcode. Skipping...");
                        }
                        for (auto &a : kernel->ArgumentMap)
                        {
                            if (a.second == ValueID)
                            {
                                VMap[cast<Value>(a.first)] = cast<Value>(inst);
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}
