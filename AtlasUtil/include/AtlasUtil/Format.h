#pragma once
#include "AtlasUtil/Split.h"
#include "AtlasUtil/Annotate.h"

inline void Format(llvm::Module *M)
{
    Split(M);
    Annotate(M);
}