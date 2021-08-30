#include "Backend/NonKernelInstance.h"

NonKernelInstance::NonKernelInstance(uint64_t firstBlock)
{
    IID = getNextIID();
    blocks.insert(firstBlock);
}