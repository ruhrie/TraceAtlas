#include "AtlasUtil/Print.h"
#include <iostream>
#include <llvm/IR/AssemblyAnnotationWriter.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

void PrintVal(llvm::Value *val)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    val->print(rso);
    std::cout << str << "\n";
}

void PrintVal(const llvm::Value *val)
{
    PrintVal(val);
}

void PrintVal(llvm::Module *mod)
{
    AssemblyAnnotationWriter *write = new llvm::AssemblyAnnotationWriter();
    std::string str;
    llvm::raw_string_ostream rso(str);
    mod->print(rso, write);
    std::cout << str << "\n";
}

void PrintVal(llvm::Type *val)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    val->print(rso);
    std::cout << str << "\n";
}