#include "Backend/BackendTrace.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>



void MemInstructionDump(void *address, uint64_t bbID, uint64_t datasize, uint64_t type)
{
    
    printf("Address:%#lX, BBid: %lu, datasize:%lu, type: %lu\n ", (uint64_t)address, bbID,datasize,type);
}

