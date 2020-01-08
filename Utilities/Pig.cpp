#include "Pig.h"
#include "tik/Exceptions.h"
#include "tik/Metadata.h"
#include <fstream>
#include <iostream>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/Constants.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <nlohmann/json.hpp>
#include <string>
using namespace llvm;
using namespace std;

cl::opt<string> InputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);
llvm::cl::opt<string> igFile("o", llvm::cl::desc("Specify output json name"), llvm::cl::value_desc("Counter Json"), llvm::cl::init("ig.json"));

void PrintVal(llvm::Value *val)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    val->print(rso);
    std::cout << str << "\n";
}

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
    //first identify the kernels
    vector<Function *> kernels;
    Module *M = sourceBitcode.get();
    for (auto mi = M->begin(); mi != M->end(); mi++)
    {
        Function *F = cast<Function>(mi);
        if (F->hasMetadata("TikFunction"))
        {
            auto meta = F->getMetadata("TikFunction");
            auto cInt = cast<ConstantInt>(cast<ConstantAsMetadata>(meta->getOperand(0))->getValue());
            auto tf = (TikMetadata)cInt->getSExtValue();
            if (tf == TikMetadata::KernelFunction)
            {
                kernels.push_back(F);
            }
        }
    }

    map<string, map<string, int>> TikCounters;

    for (auto k : kernels)
    {
        TikCounters[k->getName()] = GetRatios(k);
    }

    nlohmann::json outputJson = TikCounters;

    ofstream oStream(igFile);
    oStream << outputJson;
    oStream.close();
}

map<string, int> GetRatios(Function *F)
{
    map<string, int> result;

    for (auto fi = F->begin(); fi != F->end(); fi++)
    {
        for (auto bi = fi->begin(); bi != fi->end(); bi++)
        {
            Instruction *i = cast<Instruction>(bi);
            if (i->getMetadata("TikSynthetic"))
            {
                continue;
            }
            //start with the opcodes
            string name = string(i->getOpcodeName());
            result[name + "Count"]++;
            //now check the type
            Type *t = i->getType();
            if (t->isVoidTy())
            {
                result["typeVoid"]++;
            }
            else if (t->isFloatingPointTy())
            {
                result["typeFloat"]++;
            }
            else if (t->isIntegerTy())
            {
                result["typeInt"]++;
            }
            else if (t->isArrayTy())
            {
                result["typeArray"]++;
            }
            else if (t->isVectorTy())
            {
                result["typeVector"]++;
            }
            else if (t->isPointerTy())
            {
                result["typePointer"]++;
            }
            else
            {
                std::string str;
                llvm::raw_string_ostream rso(str);
                t->print(rso);
                cerr << "Unrecognized type: " + str + "\n";
            }
            result["instructionCount"]++;
        }
    }

    return result;
}

int GetCrossProduct(Function *F)
{
    unsigned crossProduct = 0;
    for (auto fi = F->begin(); fi != F->end(); fi++)
    {
        for (auto bi = fi->begin(); bi != fi->end(); bi++)
        {
            Instruction *i = cast<Instruction>(bi);
            if (i->getMetadata("TikSynthetic"))
            {
                continue;
            }
            unsigned ops = i->getOpcode();

            crossProduct += ops * i->getType()->getTypeID();
        }
    }
    return crossProduct;
}