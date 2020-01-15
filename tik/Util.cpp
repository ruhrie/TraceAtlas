#include "tik/Util.h"
#include <iostream>
#include <llvm/IR/AssemblyAnnotationWriter.h>
#include <llvm/Support/raw_ostream.h>
#include <regex>

using namespace std;
using namespace llvm;

string GetString(Value *v)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    v->print(rso);
    str = std::regex_replace(str, std::regex("^\\s+"), std::string(""));
    str = std::regex_replace(str, std::regex("\\s+$"), std::string(""));
    return str;
}

std::vector<std::string> GetStrings(BasicBlock *bb)
{
    std::vector<std::string> result;
    for (BasicBlock::iterator BI = bb->begin(), BE = bb->end(); BI != BE; ++BI)
    {
        Instruction *inst = cast<Instruction>(BI);
        result.push_back(GetString(inst));
    }
    return result;
}

std::map<std::string, vector<string>> GetStrings(Function *f)
{
    map<string, vector<string>> result;
    result["Body"] = GetStrings(&f->getEntryBlock());
    vector<string> args;
    for (Argument *i = f->arg_begin(); i < f->arg_end(); i++)
    {
        args.push_back(GetString(i));
    }
    if (args.size() != 0)
    {
        result["Inputs"] = args;
    }
    return result;
}

std::vector<std::string> GetStrings(std::set<Instruction *> instructions)
{
    std::vector<std::string> result;
    for (Instruction *inst : instructions)
    {
        result.push_back(GetString(inst));
    }
    return result;
}

std::vector<std::string> GetStrings(std::vector<Instruction *> instructions)
{
    std::vector<std::string> result;
    for (Instruction *inst : instructions)
    {
        std::string str;
        llvm::raw_string_ostream rso(str);
        inst->print(rso);
        result.push_back(GetString(inst));
    }
    return result;
}
