#pragma once
#include "Backend/CodeSection.h"
#include <set>
#include <string>
#include <vector>

class Kernel : public CodeSection
{
public:
    std::string label;
    std::set<int> parents;
    std::set<int> children;
    Kernel();
    Kernel(int id);
    class KernelInstance *getInstance(unsigned int i) const;
    class KernelInstance *getCurrentInstance() const;
    std::vector<KernelInstance *> getInstances() const;
    void addInstance(KernelInstance *newInstance);
};