#pragma once
#include "Backend/UniqueID.h"

class CodeInstance : public UniqueID
{
public:
    /// Position of this instance in the timeline of kernel instances
    uint32_t position;
    /// Counter for the number of times this kernel instance has occurred
    uint32_t iterations;
    virtual ~CodeInstance();
};