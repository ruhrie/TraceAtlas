#include "Backend/BackendTrace.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

void StoreInstructionDump(void *address, uint64_t bbID, uint64_t datasize)
{

    printf("Address:%#lX, BBid: %lu, datasize:%lu", (uint64_t)address, bbID, datasize);
}

void LoadInstructionDump(void *address, uint64_t bbID, uint64_t datasize)
{

    printf("Address:%#lX, BBid: %lu, datasize:%lu", (uint64_t)address, bbID, datasize);
}
void MemProfInitialization()
{

    printf("Start \n");
}

void MemProfDestroy()
{

    printf("End \n");
}