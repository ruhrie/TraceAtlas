#include "Backend/NonKernel.h"
#include "Backend/NonKernelInstance.h"

using namespace std;

NonKernel::NonKernel()
{
    IID = getNextIID();
}

NonKernelInstance *NonKernel::getInstance(unsigned int i) const
{
    return static_cast<class NonKernelInstance *>(instances[i]);
}

NonKernelInstance *NonKernel::getCurrentInstance() const
{
    if (instances.empty())
    {
        return nullptr;
    }
    return static_cast<class NonKernelInstance *>(instances.back());
}

vector<NonKernelInstance *> NonKernel::getInstances() const
{
    vector<NonKernelInstance *> NonKernelInstances;
    for (const auto &i : instances)
    {
        NonKernelInstances.push_back(static_cast<NonKernelInstance *>(i));
    }
    return NonKernelInstances;
}

void NonKernel::addInstance(NonKernelInstance *newInst)
{
    instances.push_back(newInst);
}