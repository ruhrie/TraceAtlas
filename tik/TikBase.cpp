#include "TikBase.h"
#include <set>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include "Util.h"
using namespace llvm;
using namespace std;

nlohmann::json TikBase::GetJson()
{
    nlohmann::json j;
    if (Body != NULL)
    {
        j["Body"] = GetStrings(Body);
    }
    if (Exit != NULL)
    {
        j["Exit"] = GetStrings(Exit);
    }
    if (MemoryRead != NULL)
    {
        j["MemoryRead"] = GetStrings(MemoryRead);
    }
    if (MemoryWrite != NULL)
    {
        j["MemoryWrite"] = GetStrings(MemoryWrite);
    }
    return j;
}

TikBase::TikBase()
{
    MemoryRead = NULL;
    MemoryWrite = NULL;
    Body = NULL;
    Exit = NULL;
}

TikBase::~TikBase()
{
    if (MemoryRead != NULL)
    {
        delete MemoryRead;
    }
    if (MemoryWrite != NULL)
    {
        delete MemoryWrite;
    }
    if (Body != NULL)
    {
        delete Body;
    }
    if (Exit != NULL)
    {
        delete Exit;
    }
}
