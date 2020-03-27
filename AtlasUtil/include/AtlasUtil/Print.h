#pragma once
#include <iostream>
#include <llvm/IR/AssemblyAnnotationWriter.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

inline void PrintVal(llvm::Value *val, bool print = true)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    val->print(rso);
    if (print)
    {
        std::cout << str << "\n";
    }
}

inline void PrintVal(llvm::Metadata *val)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    val->print(rso);
    std::cout << str << "\n";
}

inline void PrintVal(llvm::NamedMDNode *val)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    val->print(rso);
    std::cout << str << "\n";
}

inline void PrintVal(llvm::Module *mod)
{
    llvm::AssemblyAnnotationWriter *write = new llvm::AssemblyAnnotationWriter();
    std::string str;
    llvm::raw_string_ostream rso(str);
    mod->print(rso, write);
    std::cout << str << "\n";
}

inline void PrintVal(llvm::Type *val)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    val->print(rso);
    std::cout << str << "\n";
}
