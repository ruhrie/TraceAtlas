#pragma once
#include <llvm/IR/Module.h>
#include <set>
#include <string>
namespace TypeTwo
{
    extern std::set<std::set<int64_t>> kernels;
    void Setup();
    void Process(std::vector<std::string> &value);
    std::set<std::set<int64_t>> Get();
    void Reset();
} // namespace TypeTwo