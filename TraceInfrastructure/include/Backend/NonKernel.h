#pragma once
#include "Backend/CodeSection.h"

class NonKernel : public CodeSection
{
public:
    NonKernel();
    void addInstance(class NonKernelInstance *newInst);
    class NonKernelInstance *getInstance(unsigned int i) const;
    std::vector<class NonKernelInstance *> getInstances() const;
    NonKernelInstance *getCurrentInstance() const;
};
