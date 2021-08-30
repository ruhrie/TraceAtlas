#include "Backend/CodeSection.h"

CodeSection::~CodeSection() = default;

std::vector<CodeInstance *> CodeSection::getInstances() const
{
    return instances;
}

CodeInstance *CodeSection::getInstance(unsigned int i) const
{
    return instances[i];
}

CodeInstance *CodeSection::getCurrentInstance() const
{
    if (instances.empty())
    {
        return nullptr;
    }
    return instances.back();
}