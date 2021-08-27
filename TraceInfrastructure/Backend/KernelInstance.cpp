#include "Backend/KernelInstance.h"

using namespace std;

KernelInstance::KernelInstance(Kernel *kern)
{
    IID = getNextIID();
    k = kern;
    iterations = 0;
    position = (uint32_t)kern->getInstances().size();
    kern->addInstance(this);
    children = set<KernelInstance *>();
}