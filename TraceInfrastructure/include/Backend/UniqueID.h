#pragma once
#include <cstdint>

class UniqueID
{
public:
    /// Unique identifier
    uint64_t IID;
    UniqueID() = default;
    virtual ~UniqueID() = default;
    uint64_t getNextIID();

protected:
    void setNextIID(uint64_t next);

private:
    /// Counter for the next unique idenfitfier
    static uint64_t nextIID;
};

struct p_UIDCompare
{
    using is_transparent = void;
    bool operator()(const UniqueID *lhs, const UniqueID *rhs) const
    {
        return lhs->IID < rhs->IID;
    }
    bool operator()(const UniqueID *lhs, uint64_t rhs) const
    {
        return lhs->IID < rhs;
    }
    bool operator()(uint64_t lhs, const UniqueID *rhs) const
    {
        return lhs < rhs->IID;
    }
};