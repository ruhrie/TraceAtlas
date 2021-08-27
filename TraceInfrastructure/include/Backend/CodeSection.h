#pragma once
#include "Backend/CodeInstance.h"
#include "Backend/UniqueID.h"
#include <set>
#include <vector>

class CodeSection : public UniqueID
{
public:
    std::set<uint64_t> blocks;
    std::set<uint64_t> entrances;
    std::set<uint64_t> exits;
    virtual ~CodeSection();
    std::vector<class CodeInstance *> getInstances() const;
    CodeInstance *getInstance(unsigned int i) const;
    CodeInstance *getCurrentInstance() const;

protected:
    std::vector<class CodeInstance *> instances;
};