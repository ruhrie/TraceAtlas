#include "tik/Exceptions.h"
#include <iostream>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <nlohmann/json.hpp>
#include <string>
using namespace llvm;
using namespace std;

cl::opt<string> InputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);

//properties of intrinsics: pig
int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, argv);
    LLVMContext context;
    SMDiagnostic smerror;
    unique_ptr<Module> sourceBitcode;
    try
    {
        sourceBitcode = parseIRFile(InputFilename, smerror, context);
    }
    catch (TikException &e)
    {
        std::cerr << "Couldn't open input bitcode file: " << InputFilename << "\n";
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    
}