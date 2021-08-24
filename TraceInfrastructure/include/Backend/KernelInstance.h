#pragma once
#include "Backend/CodeInstance.h"
#include "Backend/Kernel.h"

class KernelInstance : public CodeInstance
{
public:
    /// Points to the kernel this iteration maps to
    const Kernel *k;
    /// Array of child kernels that are called by this kernel.
    /// These children are known to be called at runtime by this kernel in this order
    /// Noting that this structure is the parent structure, the children array can encode arbitrary hierarchical depths of child kernels (ie this array can contain arrays of children)
    std::set<KernelInstance *> children;
    KernelInstance(Kernel *kern);
};