#pragma once
#include "tik/Kernel.h"

extern bool VectorsUsed;

void ProcessFunctionArgument(std::string &type, const std::string &argname);

void ProcessArrayArgument(std::string &type, const std::string &argname);

void RecurseForStructs(llvm::Type *input, std::set<llvm::StructType *> &AllStructures);

std::string GetTikStructures(const std::vector<Kernel *> &kernels, std::set<llvm::StructType *> &AllStructures);

std::string getCType(llvm::Type *param, std::set<llvm::StructType *> &AllStructures);

std::string getCArrayType(llvm::Type *arrayElem, std::set<llvm::StructType *> &AllStructures, uint64_t *size = NULL);

std::string getVectorType(llvm::Type *elem, std::set<llvm::StructType *> &AllStructures);
