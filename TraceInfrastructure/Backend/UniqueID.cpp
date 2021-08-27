#include "Backend/UniqueID.h"

uint64_t UniqueID::getNextIID()
{
    return nextIID++;
}

void UniqueID::setNextIID(uint64_t next)
{
    if (next > nextIID)
    {
        nextIID = next + 1;
    }
}

uint64_t UniqueID::nextIID = 0;