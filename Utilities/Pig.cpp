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
    map<string, map<string, map<string, int>>> TikCrossProductsTypePerOp;
    map<string, map<string, map<string, int>>> TikCrossProductsOpPerType;
    for (auto k : kernels)
    {
        TikCounters[k->getName()] = GetRatios(k);
        TikCrossProductsTypePerOp[k->getName()] = GetCrossProductTypePerOp(k);
        TikCrossProductsOpPerType[k->getName()] = GetCrossProductOpPerType(k);
    }

    nlohmann::json JsonTikCounters;
    JsonTikCounters["Counters"] = TikCounters;
    JsonTikCounters["TypePerOp"]= TikCrossProductsTypePerOp;
    JsonTikCounters["OpPerType"]= TikCrossProductsOpPerType;
    ofstream oStream(igFile);
    oStream << JsonTikCounters;
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

map<string, map<string, int>> GetCrossProductTypePerOp(Function *F)
{
    map<string, map<string, int>> result;
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
            result[name]["Count"]++;
            //now check the type
            Type *t = i->getType();
            if (t->isVoidTy())
            {
                result[name]["typeVoid"]++;
            }
            else if (t->isFloatingPointTy())
            {
                result[name]["typeFloat"]++;
            }
            else if (t->isIntegerTy())
            {
                result[name]["typeInt"]++;
            }
            else if (t->isArrayTy())
            {
                result[name]["typeArray"]++;
            }
            else if (t->isVectorTy())
            {
                result[name]["typeVector"]++;
            }
            else if (t->isPointerTy())
            {
                result[name]["typePointer"]++;
            }
            else
            {
                std::string str;
                llvm::raw_string_ostream rso(str);
                t->print(rso);
                cerr << "Unrecognized type: " + str + "\n";
            }
            result["instruction"]["Count"]++;
        }
    }

    return result;
}

map<string, map<string, int>> GetCrossProductOpPerType(Function *F)
{
    map<string, map<string, int>> result;
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
            
            //now check the type
            Type *t = i->getType();
            if (t->isVoidTy())
            {
                result["typeVoid"]["Count"]++;
                result["typeVoid"][name]++;
            }
            else if (t->isFloatingPointTy())
            {
                result["typeFloat"]["Count"]++;
                result["typeFloat"][name]++;
            }
            else if (t->isIntegerTy())
            {
                result["typeInt"]["Count"]++;
                result["typeInt"][name]++;
            }
            else if (t->isArrayTy())
            {
                result["typeArray"]["Count"]++;
                result["typeArray"][name]++;
            }
            else if (t->isVectorTy())
            {
                result["typeVector"]["Count"]++;
                result["typeVector"][name]++;
            }
            else if (t->isPointerTy())
            {
                result["typePointer"]["Count"]++;
                result["typePointer"][name]++;
            }
            else
            {
                std::string str;
                llvm::raw_string_ostream rso(str);
                t->print(rso);
                cerr << "Unrecognized type: " + str + "\n";
            }
            result["instruction"]["Count"]++;
        }
    }

    return result;
}