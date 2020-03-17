#pragma once
#include <llvm/IR/Module.h>
#include <set>
#include <string>
namespace TypeTwo
{
    void Setup(llvm::Module *bitcode, std::set<std::set<int>> k);
    void Process(std::string &key, std::string &value);
    std::set<std::set<int>> Get();
} // namespace TypeTwo