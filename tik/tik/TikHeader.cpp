#include "tik/TikHeader.h"
#include "AtlasUtil/Print.h"
#include "tik/Exceptions.h"
#include "tik/Kernel.h"
#include <iostream>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/Casting.h>
#include <spdlog/spdlog.h>
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
        char *memberName = new char[4];
        std::string a = structure->getName();
        std::string structureDefinition = "\nstruct " + a + " {\n";
        for (int i = 0; i < structure->getNumElements(); i++)
        {
            memset(memberName, 0, 4);
            memberName[0] = i % 26 + 97;
            std::string varDec;
            try
            {
                varDec = getCType(structure->getElementType(i), AllStructures);
            }
            catch (TikException &e)
            {
                spdlog::error(e.what());
                varDec = "TypeNotSupported";
            }
            // if we find a bang, we have an array and the variable name has to be inserted before the array sizes
            if (varDec.find("!") != std::string::npos)
            {
                varDec.erase(varDec.begin() + varDec.find("!"));
                // find all of our asterisks;
                int ast = 0;
                while (varDec.find("*") != std::string::npos)
                {
                    varDec.erase(varDec.begin() + varDec.find("*"));
                    ast++;
                }
                std::size_t whiteSpacePosition;
                // if we have a structure, find the first [] and insert the name before it
                if (varDec.find("struct") != std::string::npos)
                {
                    whiteSpacePosition = varDec.find("[") - 1;
                }
                else // we just have a type name and white space at the end
                {
                    whiteSpacePosition = varDec.find(" ");
                }
                int varLength = (int)(i / 26) + 1;
                for (int j = 0; j < varLength; j++)
                {
                    varDec.insert(whiteSpacePosition + 1, &memberName[0]);
                }
                for (int j = 0; j < ast; j++)
                {
                    varDec.insert(whiteSpacePosition, "*");
                }
                structureDefinition += "\t" + varDec + ";\n";
            }
            else if (varDec.find("@") != std::string::npos)
            {
                varDec.erase(varDec.begin() + varDec.find("@"));
                // find all of our asterisks;
                int ast = 0;
                while (varDec.find("#") != std::string::npos)
                {
                    varDec.erase(varDec.begin() + varDec.find("#"));
                    ast++;
                }
                std::size_t whiteSpacePosition = varDec.find("(") - 1;
                varDec.insert(whiteSpacePosition + 1, ") ");
                for (int j = 0; j < (int)(i / 26) + 1; j++)
                {
                    varDec.insert(whiteSpacePosition + 1, &memberName[0]);
                }
                std::string pointerString = "(";
                for (int j = 0; j < ast; j++)
                {
                    pointerString += "*";
                }
                varDec.insert(whiteSpacePosition + 1, pointerString);
            }
            else
            {
                structureDefinition += "\t" + varDec + " ";
                for (int j = 0; j < (int)(i / 26) + 1; j++)
                {
                    structureDefinition.insert(structureDefinition.size(), &memberName[0]);
                }
                structureDefinition += ";\n";
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
        std::size_t whiteSpacePosition;
        // if the type is a struct, set the name insert position to be the end
        if (type.find("struct") != std::string::npos)
        {
            whiteSpacePosition = type.size() - 1;
        }
        // else set the insert position to the end
        else
        {
            whiteSpacePosition = type.find(" ");
        }
        std::string arrayDec = "";
        // if there's no whitespace in the declaration string, or if we have a struct, the base case is right below us
        if (whiteSpacePosition == std::string::npos || whiteSpacePosition == type.size() - 1)
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
        try
        {
            return getCType(elem, AllStructures);
        }
        catch (TikException &e)
        {
            spdlog::error(e.what());
            return "TypeNotSupported";
        }
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
        // check if we had an array type, don't add the star
        if(a.find("@") != std::string::npos)
        {
            return a + "#";
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
            return getCArrayType(param, AllStructures);
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
            llvm::FunctionType* func = dyn_cast<llvm::FunctionType>(param);
            std::string type = "@";
            type  += getCType(func->getReturnType(), AllStructures);
            type += " (";
            for (int i = 0; i < func->getNumParams(); i++)
            {
                if (i > 0)
                {
                    type += ", ";
                }
                type += getCType(func->getParamType(i), AllStructures);
            }
            type += ")";
            return type;
        }
        else
        {
            throw TikException("Unrecognized argument type is not supported for header generation.");
        }
    }
}
