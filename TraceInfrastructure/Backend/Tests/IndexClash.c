#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "Backend/DashHashTable.h"

// 2^20 basic blocks is about the worst case scenario we can expect
#define HASHTABLESIZE   1048576
// when the array size and entries is 2^20, 8 is the first number where we start seeing clashes
#define AVG_NEIGHBORS   25
// when the array size and entries is 2^20 and the average neighbor size is 8, 2^19 is the first number where we start seeing clashes
#define HIGHLY_CONNECTED_NEIGHBORS  262144

int main()
{
    // initial allocation of all data structures
    __TA_HashTable* hashTable = (__TA_HashTable*)malloc( sizeof(__TA_HashTable) );
    // convert array size to power of 2, round to the ceiling
    hashTable->size = (uint32_t)( ceil( log((double)HASHTABLESIZE) / log(2.0) ) );
    hashTable->getFullSize = getFullSize;
    hashTable->array = (__TA_arrayElem*)malloc( (hashTable->getFullSize(hashTable))*sizeof(__TA_arrayElem) );
    __TA_kvTuple entry0;

    // this pushes AVG_NEIGHBORS * HASHTABLESIZE entries into the table
    for( int i = 0; i < HASHTABLESIZE-1; i++ )
    {
        entry0.source    = i;
        entry0.frequency = 0;
        for( int j = 0; j < AVG_NEIGHBORS; j++ )
        {
            entry0.sink      = i+j;
            while( __TA_HashTable_increment(hashTable, &entry0) )
            {
                __TA_resolveClash(hashTable);
            }
        }
    }

    // this pushes 2^18 entries into the table
    entry0.source = HASHTABLESIZE-1;
    entry0.frequency = 0;
    for( int i = 0; i < HIGHLY_CONNECTED_NEIGHBORS; i++ )
    {
        entry0.sink = i;
        while( __TA_HashTable_increment(hashTable, &entry0) )
        {
            __TA_resolveClash(hashTable);
        }
    }

    free(hashTable->array);
    free(hashTable);
    return 0;
}
