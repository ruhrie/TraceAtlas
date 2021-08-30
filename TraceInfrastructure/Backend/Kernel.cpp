#include "Backend/Kernel.h"
#include "Backend/KernelInstance.h"

using namespace std;

Kernel::Kernel()
{
}

Kernel::Kernel(int id)
{
    IID = (uint64_t)id;
    setNextIID(IID);
}

KernelInstance *Kernel::getInstance(unsigned int i) const
{
    return static_cast<KernelInstance *>(instances[i]);
}

KernelInstance *Kernel::getCurrentInstance() const
{
    if (instances.empty())
    {
        return nullptr;
    }
    return static_cast<KernelInstance *>(instances.back());
}

vector<KernelInstance *> Kernel::getInstances() const
{
    vector<KernelInstance *> KernelInstances;
    for (const auto &i : instances)
    {
        KernelInstances.push_back(static_cast<KernelInstance *>(i));
    }
    return KernelInstances;
}

void Kernel::addInstance(KernelInstance *newInstance)
{
    instances.push_back(newInstance);
}