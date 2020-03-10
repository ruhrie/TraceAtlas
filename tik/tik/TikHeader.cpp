#include "tik/TikHeader.h"
#include "AtlasUtil/Print.h"
#include "tik/Exceptions.h"
#include "tik/Kernel.h"
#include <iostream>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/Casting.h>
#include <string.h>

using namespace llvm;

void RecurseForStructs(llvm::Type *input, std::set<llvm::StructType *> &AllStructures)
{
    if (input->isStructTy())
    {
        llvm::StructType *newStruct = dyn_cast<llvm::StructType>(input);
        auto structMember = AllStructures.find(newStruct);
        if (structMember != AllStructures.end())
        {
            return;
        }
        const std::string newStringName = "tikStruct" + std::to_string(AllStructures.size());
        llvm::StringRef newName = llvm::StringRef(newStringName);
        newStruct->setName(newName);
        AllStructures.insert(newStruct);
        for (int i = 0; i < newStruct->getNumElements(); i++)
        {
            RecurseForStructs(newStruct->getElementType(i), AllStructures);
        }
    }
    else if (input->isPointerTy())
    {
        llvm::PointerType *newType = dyn_cast<llvm::PointerType>(input);
        llvm::Type *memberType = newType->getElementType();
        RecurseForStructs(memberType, AllStructures);
    }
}

std::string GetTikStructures(std::vector<Kernel *> kernels, std::set<llvm::StructType *> &AllStructures)
{
    for (auto kernel : kernels)
    {
        for (auto ai = kernel->KernelFunction->arg_begin(); ai < kernel->KernelFunction->arg_end(); ai++)
        {
            RecurseForStructs(ai->getType(), AllStructures);
        }
    }
    std::string AllStructureDefinitions;
    for (auto structure : AllStructures)
    {
        std::string a = structure->getName();
        std::string structureDefinition = "\nstruct " + a + " {\n";
        for (int i = 0; i < structure->getNumElements(); i++)
        {
            char *memberName = new char[4];
            memset(memberName, 0, 4);
            memberName[0] = i + 97;
            if (int(memberName[0]) < 123)
            {
                std::cout << std::to_string(int(memberName[0])) << std::endl;
                std::string varDec = getCType(structure->getElementType(i), AllStructures);
                // if we find a bang, the variable name has to be inserted after the whitespace and before the array sizes
                if (varDec.find("!") != std::string::npos)
                {
                    varDec.erase(varDec.begin() + varDec.find("!"));
                    std::size_t whiteSpacePosition = varDec.find(" ");
                    varDec.insert(whiteSpacePosition + 1, &memberName[0]);
                    structureDefinition += "\t" + varDec + ";\n";
                }
                else
                {
                    structureDefinition += "\t" + varDec + " " + memberName[0] + ";\n";
                }
            }
            else
            {
                memberName[0] = memberName[0] - 26;
                std::string varDec = getCType(structure->getElementType(i), AllStructures);
                // if we a bang, the variable name has to be inserted after the whitespace and before the array sizes
                if (varDec.find("!") != std::string::npos)
                {
                    varDec.erase(varDec.begin() + varDec.find("!"));
                    std::size_t whiteSpacePosition = varDec.find(" ");
                    varDec.insert(whiteSpacePosition + 1, &memberName[0]);
                    varDec.insert(whiteSpacePosition + 1, &memberName[0]);
                    structureDefinition += "\t" + varDec + ";\n";
                }
                else
                {
                    structureDefinition += "\t" + varDec + " " + memberName[0] + memberName[0] + ";\n";
                }
            }
        }
        structureDefinition += "};\n";
        AllStructureDefinitions += structureDefinition;
    }
    return AllStructureDefinitions;
}

std::string getCArrayType(llvm::Type *elem, std::set<llvm::StructType *> &AllStructures, int *size)
{
    std::string type = "";
    // if this is the first time calling, size will be null, so allocate for it
    if (!size)
    {
        size = new int;
    }
    // if our elements are arrays, recurse
    if (elem->isArrayTy())
    {
        type = getCArrayType(dyn_cast<llvm::ArrayType>(elem)->getArrayElementType(), AllStructures, size);
        std::size_t whiteSpacePosition = type.find(" ");
        std::string arrayDec = "";
        // if there's no whitespace in the declaration string, the base case is right below us
        if (whiteSpacePosition == std::string::npos)
        {
            *size = elem->getArrayNumElements();
            arrayDec = "!" + type + " [" + std::to_string(*size) + "]";
        }
        // else we just need to insert the size of our array
        else
        {
            *size = dyn_cast<llvm::ArrayType>(elem)->getArrayNumElements();
            type.insert(whiteSpacePosition + 1, "[" + std::to_string(*size) + "]");
            arrayDec = type;
        }
        return arrayDec;
    }
    // else return the type we have
    else
    {
        return getCType(elem, AllStructures);
    }
}

std::string getCType(llvm::Type *param, std::set<llvm::StructType *> &AllStructures)
{
    if (param->isVoidTy())
    {
        return "void";
    }
    else if (param->isHalfTy())
    {
        return "half";
    }
    else if (param->isFloatTy())
    {
        return "float";
    }
    else if (param->isDoubleTy())
    {
        return "double";
    }
    else if (param->isX86_FP80Ty())
    {
        return "long double";
    }
    else if (param->isFP128Ty())
    {
        return "__float128";
    }
    else if (param->isIntegerTy(8))
    {
        return "uint8_t";
    }
    else if (param->isIntegerTy(16))
    {
        return "uint16_t";
    }
    else if (param->isIntegerTy(32))
    {
        return "uint32_t";
    }
    else if (param->isIntegerTy(64))
    {
        return "uint64_t";
    }
    else if (param->isPointerTy())
    {
        llvm::PointerType *newType = dyn_cast<llvm::PointerType>(param);
        llvm::Type *memberType = newType->getElementType();
        std::string a = getCType(memberType, AllStructures);
        // check if we had an array type
        if (a.find("!") != std::string::npos)
        {
            return a;
        }
        else
        {
            return a + "*";
        }
    }
    else
    {
        if (param->isArrayTy())
        {
            std::string a = getCArrayType(param, AllStructures);
            return a;
        }
        else if (param->isVectorTy())
        {
            throw TikException("Vector argument types are not supported for header generation.");
        }
        else if (param->isStructTy())
        {
            auto structureArg = AllStructures.find(dyn_cast<StructType>(param));
            if (structureArg != AllStructures.end())
            {
                std::string structName = (*structureArg)->getName();
                return "struct " + structName;
            }
            else
            {
                throw TikException("Could not find structure argument in AllStructures vector.");
            }
        }
        else if (param->isFunctionTy())
        {
            throw TikException("Function argument types are not supported for header generation.");
        }
        else
        {
            throw TikException("Unrecognized argument type is not supported for header generation.");
        }
    }
}
