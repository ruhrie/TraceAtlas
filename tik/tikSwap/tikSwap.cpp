#include "tikSwap/tikSwap.h"
#include "AtlasUtil/Exceptions.h"
#include <iostream>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
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
    Module *base = sourceBitcode.get();
    for (auto &func : *base)
    {
        string funcName = func.getName();
        cout << funcName << endl;
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
    for (auto &func : *tikModule)
    {
        string funcName = func.getName();
        cout << funcName << endl;
    }

    // grab all kernel functions in the tik bitcode and construct objects from them
    vector<Function*> kernFuncs;
    if (Function* newFunc = tikModule->getFunction("K0"))
    {
        int i = 0;
        while( newFunc )
        {
            i++;
            kernFuncs.push_back(newFunc);
            newFunc = tikModule->getFunction("K"+to_string(i));
        }
    }
    else
    {
        throw AtlasException("The input tik module has no functions in it.");
    }
    vector<CartographerKernel*> kernels;
    for (auto func : kernFuncs)
    {
        CartographerKernel* kern = new CartographerKernel(func);
        if (kern->Valid)
        {
            kernels.push_back(kern);
        }
    }
    return 0;
}