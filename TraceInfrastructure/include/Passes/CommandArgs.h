#pragma once
#include <llvm/Support/CommandLine.h>
#include <string>

using namespace llvm;

/// <summary>
/// The kernel index upon which to work.
/// </summary>
extern cl::opt<int> KernelIndex;
/// <summary>
/// The name of the input kernel file
/// </summary>
extern cl::opt<std::string> KernelFilename;

extern cl::opt<bool> DumpLoads;

extern cl::opt<bool> DumpStores;

extern cl::opt<std::string> LibraryName;
