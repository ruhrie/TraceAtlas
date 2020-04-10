#include "tik/TikHeader.h"
#include "AtlasUtil/Exceptions.h"
#include "AtlasUtil/Print.h"
#include "tik/Kernel.h"
#include <cstring>
#include <iostream>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/Casting.h>
#include <spdlog/spdlog.h>

using namespace llvm;

bool VectorsUsed = false;

void ProcessArrayArgument(std::string &type, const std::string &argname)
{
    type.erase(type.begin() + (long)type.find('!'));
    // find all of our asterisks;
    int ast = 0;
    while (type.find('*') != std::string::npos)
    {
        type.erase(type.begin() + (long)type.find('*'));
        ast++;
    }
    std::size_t whiteSpacePosition;
    // if we have a structure as the element type, find the first [] and insert the name before it
    // struct tikStruct2 []
    if (type.find("struct") != std::string::npos)
    {
        whiteSpacePosition = type.find('[') - 1;
    }
    else // just find the white space between type and []
    {
        whiteSpacePosition = type.find(' ');
    }
    type.insert(whiteSpacePosition + 1, argname);
    // struct tikStruct2 argname[]
    if (ast > 0)
    {
        std::size_t argNameStart = whiteSpacePosition + 1;
        for (int j = 0; j < ast; j++)
        {
            type.insert(argNameStart, "*");
        }
        type.insert(argNameStart, "(");
        whiteSpacePosition = type.find('[');
        type.insert(whiteSpacePosition, ")");
    }
}

void ProcessFunctionArgument(std::string &type, const std::string &argname)
{
    type.erase(type.begin() + (long)type.find('@'));
    // find all of our asterisks;
    int ast = 0;
    while (type.find('#') != std::string::npos)
    {
        type.erase(type.begin() + (long)type.find('#'));
        ast++;
    }
    std::size_t whiteSpacePosition = type.find('(') - 1;
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
        auto *newStruct = dyn_cast<llvm::StructType>(input);
        auto structMember = AllStructures.find(newStruct);
        if (structMember != AllStructures.end())
        {
            return;
        }
        const std::string newStringName = "tikStruct" + std::to_string(AllStructures.size());
        auto newName = llvm::StringRef(newStringName);
        newStruct->setName(newName);
        AllStructures.insert(newStruct);
        for (uint32_t i = 0; i < newStruct->getNumElements(); i++)
        {
            RecurseForStructs(newStruct->getElementType(i), AllStructures);
        }
    }
    else if (input->isPointerTy())
    {
        auto *newType = cast<llvm::PointerType>(input);
        llvm::Type *memberType = newType->getElementType();
        RecurseForStructs(memberType, AllStructures);
    }
    else if (input->isArrayTy())
    {
        auto *array = cast<ArrayType>(input);
        for (uint32_t i = 0; i < array->getNumElements(); i++)
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
        auto *func = cast<llvm::FunctionType>(input);
        for (uint32_t i = 0; i < func->getNumParams(); i++)
        {
            RecurseForStructs(func->getParamType(i), AllStructures);
        }
        RecurseForStructs(func->getReturnType(), AllStructures);
    }
}

std::string GetTikStructures(const std::vector<Kernel *> &kernels, std::set<llvm::StructType *> &AllStructures)
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
        for (uint32_t i = 0; i < structure->getNumElements(); i++)
        {
            char memberChar = i % 26 + 97;
            std::string memberName;
            for (int j = 0; j < (int)(i / 26) + 1; j++)
            {
                memberName += static_cast<char>(memberChar);
            }
            std::string varDec;
            try
            {
                varDec = getCType(structure->getElementType(i), AllStructures);
            }
            catch (AtlasException &e)
            {
                spdlog::error(e.what());
                varDec = "TypeNotSupported";
            }
            if (varDec.find('!') != std::string::npos)
            {
                ProcessArrayArgument(varDec, memberName);
                structureDefinition += "\t" + varDec + ";\n";
            }
            else if (varDec.find('@') != std::string::npos)
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

std::string getCArrayType(llvm::Type *elem, std::set<llvm::StructType *> &AllStructures, uint64_t &size)
{
    std::string type;
    // if our elements are arrays, recurse
    if (elem->isArrayTy())
    {
        type = getCArrayType(cast<llvm::ArrayType>(elem)->getArrayElementType(), AllStructures, size);
        std::size_t whiteSpacePosition;
        // if the type is a struct, set the name insert position to be the end
        if (type.find("struct") != std::string::npos)
        {
            whiteSpacePosition = type.size() - 1;
        }
        // else set the insert position to the first whitespace
        else
        {
            whiteSpacePosition = type.find(' ');
        }
        std::string arrayDec;
        // if there's no whitespace in the declaration string, or if we have a struct, the base case is right below us
        if (whiteSpacePosition == std::string::npos || whiteSpacePosition == type.size() - 1)
        {
            size = elem->getArrayNumElements();
            arrayDec = "!" + type + " [" + std::to_string(size) + "]";
        }
        // else we just need to insert the size of our array
        else
        {
            size = cast<llvm::ArrayType>(elem)->getArrayNumElements();
            type.insert(whiteSpacePosition + 1, "[" + std::to_string(size) + "]");
            arrayDec = type;
        }
        return arrayDec;
    }
    // else return the type we have
    try
    {
        return getCType(elem, AllStructures);
    }
    catch (AtlasException &e)
    {
        spdlog::error(e.what());
        return "TypeNotSupported";
    }
}

std::string getCVectorType(llvm::Type *elem, std::set<llvm::StructType *> &AllStructures)
{
    auto *vecArg = cast<llvm::VectorType>(elem);
    VectorsUsed = true;
    unsigned int elemCount = vecArg->getElementCount().Min;
    std::string type = getCType(vecArg->getElementType(), AllStructures);
    if (type == "float" && elemCount == 2)
    {
        return "__m128";
    }
    if (type == "float" && elemCount == 4)
    {
        return "__m128";
    }
    if (type == "float" && elemCount == 8)
    {
        return "__m256";
    }
    if (type == "double" && elemCount == 2)
    {
        return "__m128d";
    }
    if (type == "double" && elemCount == 4)
    {
        return "__m256d";
    }
    if (type == "int" && elemCount == 4)
    {
        return "__m128i";
    }
    if (type == "int" && elemCount == 8)
    {
        return "__m256i";
    }
    if (type == "uint8_t" && elemCount == 8)
    {
        return "__m64";
    }
    if (type == "uint8_t" && elemCount == 16)
    {
        return "__m128i";
    }
    if (type == "uint8_t" && elemCount == 32)
    {
        return "__m256i";
    }
    if (type == "uint8_t" && elemCount == 64)
    {
        return "__m512i";
    }
    if (type == "uint16_t" && elemCount == 4)
    {
        return "__m64";
    }
    if (type == "uint16_t" && elemCount == 8)
    {
        return "__m128i";
    }
    if (type == "uint16_t" && elemCount == 16)
    {
        return "__m256i";
    }
    if (type == "uint16_t" && elemCount == 32)
    {
        return "__m512i";
    }
    if (type == "uint32_t" && elemCount == 2)
    {
        return "__m64";
    }
    if (type == "uint32_t" && elemCount == 4)
    {
        return "__m128i";
    }
    if (type == "uint32_t" && elemCount == 8)
    {
        return "__m256i";
    }
    if (type == "uint32_t" && elemCount == 16)
    {
        return "__m512i";
    }
    if (type == "uint64_t" && elemCount == 2)
    {
        return "__m128i";
    }
    if (type == "uint64_t" && elemCount == 4)
    {
        return "__m256i";
    }
    if (type == "uint64_t" && elemCount == 8)
    {
        return "__m512i";
    }

    throw AtlasException("Vector type bitwidth not supported.");
    return "VectorSizeNotSupported";
}

std::string getCType(llvm::Type *param, std::set<llvm::StructType *> &AllStructures)
{
    if (param->isVoidTy())
    {
        return "void";
    }
    if (param->isHalfTy())
    {
        return "half";
    }
    if (param->isFloatTy())
    {
        return "float";
    }
    if (param->isDoubleTy())
    {
        return "double";
    }
    if (param->isX86_FP80Ty())
    {
        return "long double";
    }
    if (param->isFP128Ty())
    {
        return "__float128";
    }
    if (param->isPPC_FP128Ty())
    {
        throw AtlasException("PPC_FP128Ty is not supported.")
    }
    if (param->isFloatingPointTy())
    {
        throw AtlasException("This floating point type is not supported.")
    }
    if (param->isX86_MMXTy())
    {
        throw AtlasException("This MMX type is not supported.")
    }
    if (param->isFPOrFPVectorTy())
    {
        return getCVectorType(param, AllStructures);
    }
    if (param->isIntOrIntVectorTy())
    {
        if (param->isIntegerTy())
        {
            auto *intArg = cast<llvm::IntegerType>(param);
            if (intArg->getBitWidth() == 1)
            {
                return "bool";
            }

            return "uint" + std::to_string(intArg->getBitWidth()) + "_t";
        }
        // must be a vector

        return getCVectorType(param, AllStructures);
    }
    if (param->isPointerTy())
    {
        auto *newType = cast<llvm::PointerType>(param);
        llvm::Type *memberType = newType->getElementType();
        std::string a = getCType(memberType, AllStructures);
        // check if we had an array type, don't add the star
        if (a.find('@') != std::string::npos)
        {
            return a + "#";
        }
        return a + "*";
    }
    if (param->isArrayTy())
    {
        uint64_t size;
        return getCArrayType(param, AllStructures, size);
    }
    if (param->isVectorTy())
    {
        return getCVectorType(param, AllStructures);
    }
    if (param->isStructTy())
    {
        auto structureArg = AllStructures.find(cast<StructType>(param));
        if (structureArg != AllStructures.end())
        {
            std::string structName = (*structureArg)->getName();
            return "struct " + structName;
        }
        throw AtlasException("Could not find structure argument in AllStructures vector.");
    }
    if (param->isFunctionTy())
    {
        auto *func = cast<llvm::FunctionType>(param);
        std::string type = "@";
        type += getCType(func->getReturnType(), AllStructures);
        type += " (";
        for (uint32_t i = 0; i < func->getNumParams(); i++)
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

    throw AtlasException("Unrecognized argument type is not supported for header generation.");
}
