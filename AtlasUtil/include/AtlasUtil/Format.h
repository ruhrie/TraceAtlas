#pragma once
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Split.h"

inline void Format(llvm::Module *M)
{
    Split(M);
    Annotate(M);
}