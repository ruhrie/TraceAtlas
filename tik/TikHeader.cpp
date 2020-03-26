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

bool VectorsUsed = false;

void ProcessArrayArgument(std::string &type, std::string argname)
{
    type.erase(type.begin() + type.find("!"));
    // find all of our asterisks;
    int ast = 0;
    while (type.find("*") != std::string::npos)
    {
        type.erase(type.begin() + type.find("*"));
        ast++;
    }
    std::size_t whiteSpacePosition;
    // if we have a structure, find the first [] and insert the name before it
    if (type.find("struct") != std::string::npos)
    {
        whiteSpacePosition = type.find("[") - 1;
    }
    else // just find the white space between type and []
    {
        whiteSpacePosition = type.find(" ");
    }
    type.insert(whiteSpacePosition + 1, argname);
    for (int j = 0; j < ast; j++)
    {
        type.insert(whiteSpacePosition, "*");
    }
}

void ProcessFunctionArgument(std::string &type, std::string argname)
{
    type.erase(type.begin() + type.find("@"));
    // find all of our asterisks;
    int ast = 0;
    while (type.find("#") != std::string::npos)
    {
        type.erase(type.begin() + type.find("#"));
        ast++;
    }
    std::size_t whiteSpacePosition = type.find("(") - 1;
    type.insert(whiteSpacePosition + 1, ") ");
    std::string pointerString = " (";
    for (int j = 0; j < ast; j++)
    {
        pointerString += "*";
    }
    std::string funcName = pointerString + argname;
    type.insert(whiteSpacePosition, funcName);
}

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
    else if (input->isArrayTy())
    {
        llvm::ArrayType *array = cast<ArrayType>(input);
        for (int i = 0; i < array->getNumElements(); i++)
        {
            RecurseForStructs(array->getTypeAtIndex(i), AllStructures);
        }
    }
    else if (input->isVectorTy())
    {
        RecurseForStructs(cast<VectorType>(input)->getElementType(), AllStructures);
    }
    else if (input->isFunctionTy())
    {
        llvm::FunctionType *func = cast<llvm::FunctionType>(input);
        for (int i = 0; i < func->getNumParams(); i++)
        {
            RecurseForStructs(func->getParamType(i), AllStructures);
        }
        RecurseForStructs(func->getReturnType(), AllStructures);
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
            char memberChar = i % 26 + 97;
            std::string memberName = "";
            for (int j = 0; j < (int)(i / 26) + 1; j++)
            {
                memberName += static_cast<char>(memberChar);
            }
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
            if (varDec.find("!") != std::string::npos)
            {
                ProcessArrayArgument(varDec, memberName);
                structureDefinition += "\t" + varDec + ";\n";
            }
            else if (varDec.find("@") != std::string::npos)
            {
                ProcessFunctionArgument(varDec, memberName);
                structureDefinition += "\t" + varDec + ";\n";
            }
            else
            {
                structureDefinition += "\t" + varDec + " ";
                for (int j = 0; j < (int)(i / 26) + 1; j++)
                {
                    structureDefinition.insert(structureDefinition.size(), memberName);
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

std::string getCVectorType(llvm::Type *elem, std::set<llvm::StructType *> &AllStructures)
{
    llvm::VectorType *vecArg = dyn_cast<llvm::VectorType>(elem);
    VectorsUsed = true;
    unsigned int elemCount = vecArg->getElementCount().Min;
    std::string type = getCType(vecArg->getElementType(), AllStructures);
    if (type == "float" && elemCount == 2)
    {
        return "__m128";
    }
    else if (type == "float" && elemCount == 4)
    {
        return "__m128";
    }
    else if (type == "float" && elemCount == 8)
    {
        return "__m256";
    }
    else if (type == "double" && elemCount == 2)
    {
        return "__m128d";
    }
    else if (type == "double" && elemCount == 4)
    {
        return "__m256d";
    }
    else if (type == "int" && elemCount == 4)
    {
        return "__m128i";
    }
    else if (type == "int" && elemCount == 8)
    {
        return "__m256i";
    }
    else if (type == "uint8_t" && elemCount == 8)
    {
        return "__m64";
    }
    else if (type == "uint8_t" && elemCount == 16)
    {
        return "__m128i";
    }
    else if (type == "uint8_t" && elemCount == 32)
    {
        return "__m256i";
    }
    else if (type == "uint8_t" && elemCount == 64)
    {
        return "__m512i";
    }
    else if (type == "uint16_t" && elemCount == 4)
    {
        return "__m64";
    }
    else if (type == "uint16_t" && elemCount == 8)
    {
        return "__m128i";
    }
    else if (type == "uint16_t" && elemCount == 16)
    {
        return "__m256i";
    }
    else if (type == "uint16_t" && elemCount == 32)
    {
        return "__m512i";
    }
    else if (type == "uint32_t" && elemCount == 2)
    {
        return "__m64";
    }
    else if (type == "uint32_t" && elemCount == 4)
    {
        return "__m128i";
    }
    else if (type == "uint32_t" && elemCount == 8)
    {
        return "__m256i";
    }
    else if (type == "uint32_t" && elemCount == 16)
    {
        return "__m512i";
    }
    else if (type == "uint64_t" && elemCount == 2)
    {
        return "__m128i";
    }
    else if (type == "uint64_t" && elemCount == 4)
    {
        return "__m256i";
    }
    else if (type == "uint64_t" && elemCount == 8)
    {
        return "__m512i";
    }
    else
    {
        throw TikException("Vector type bitwidth not supported.");
        return "VectorSizeNotSupported";
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
    else if (param->isPPC_FP128Ty())
    {
        throw TikException("PPC_FP128Ty is not supported.")
    }
    else if (param->isFloatingPointTy())
    {
        throw TikException("This floating point type is not supported.")
    }
    else if (param->isX86_MMXTy())
    {
        throw TikException("This MMX type is not supported.")
    }
    else if (param->isFPOrFPVectorTy())
    {
        return getCVectorType(param, AllStructures);
    }
    else if (param->isIntOrIntVectorTy())
    {
        if (param->isIntegerTy())
        {
            llvm::IntegerType *intArg = dyn_cast<llvm::IntegerType>(param);
            if (intArg->getBitWidth() == 1)
            {
                return "bool";
            }
            else
            {
                return "uint" + std::to_string(intArg->getBitWidth()) + "_t";
            }
        }
        else // must be a vector
        {
            return getCVectorType(param, AllStructures);
        }
    }
    else if (param->isPointerTy())
    {
        llvm::PointerType *newType = dyn_cast<llvm::PointerType>(param);
        llvm::Type *memberType = newType->getElementType();
        std::string a = getCType(memberType, AllStructures);
        // check if we had an array type, don't add the star
        if (a.find("@") != std::string::npos)
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
            return getCVectorType(param, AllStructures);
        }
        else if (param->isStructTy())
        {
            auto structureArg = AllStructures.find(cast<StructType>(param));
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
            llvm::FunctionType *func = dyn_cast<llvm::FunctionType>(param);
            std::string type = "@";
            type += getCType(func->getReturnType(), AllStructures);
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
