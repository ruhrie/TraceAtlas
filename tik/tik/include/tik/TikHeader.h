#pragma once
#include "tik/Kernel.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <string>

void RecurseForStructs(llvm::Type *input, std::set<llvm::StructType *> &AllStructures);

std::string GetTikStructures(std::vector<Kernel *> kernels, std::set<llvm::StructType *> &AllStructures);

std::string getCType(llvm::Type *param, std::set<llvm::StructType *> &AllStructures);

std::string getCArrayType(llvm::Type *arrayElem, std::set<llvm::StructType *> &AllStructures, int *size = NULL);
