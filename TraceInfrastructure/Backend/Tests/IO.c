#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "Backend/DashHashTable.h"

// 2^20 basic blocks is about the worst case scenario we can expect
#define HASHTABLESIZE   1048576
// when the array size and entries is 2^20, 8 is the first number where we start seeing clashes
#define AVG_NEIGHBORS   15
// when the array size and entries is 2^20 and the average neighbor size is 8, 2^19 is the first number where we start seeing clashes
#define HIGHLY_CONNECTED_NEIGHBORS  10000
// This program tests the IO of the hashtable program
// 1. It pushes a ton of entries into the hash table 
// 2. Checks if all entries have been preserved

void CheckFileAccuracy(__TA_HashTable* a, __TA_HashTable* b)
{
    for( uint32_t i = 0; i < a->getFullSize(a); i++ )
    {
        for( uint32_t j = 0; j < a->array[i].popCount; j++ )
        {
            __TA_kvTuple old = a->array[i].tuple[j];
            __TA_kvTuple new = b->array[i].tuple[j];
            if( old.source != new.source || old.sink != new.sink || old.frequency != new.frequency )
            {
                printf("Found an entry that did not match! old: (%d,%d,%lu), new: (%d,%d,%lu)\n", old.source,old.sink,old.frequency,new.source,new.sink,new.frequency);
            }
        }
    }
}

void checkAccuracy(__TA_HashTable* a, int i, int l)
{
    __TA_kvTuple entry;
    // check the first for loop which should already be in there
    for( int j = 0; j <= i; j++ )
    {
        entry.source = j;
        entry.frequency = 1; // this should be the value of the frequency because every entry is pushed by the incrementer
        for( int k = 0; k < AVG_NEIGHBORS; k++ )
        {
            // case where j is on the last entry and we fail on neighbor l
            if( j == i )
            {
                if( k == l )
                {
                    break;
                }
            }
            entry.sink = j+k;
            __TA_kvTuple* read = __TA_HashTable_read(a, &entry);
            if( !read )
            {
                printf("Failed to recover an entry of nodes (%d,%d) that should exist!\n", entry.source, entry.sink);
            }
            else if( read->frequency != entry.frequency )
            {
                printf("The frequency value for entry (%d,%d) was %lu and the correct answer was %lu!\n", read->source, read->sink, read->frequency, entry.frequency);
            }

        }
    }
}

void checkAccuracy2(__TA_HashTable* a, int i)
{
    __TA_kvTuple entry;
    entry.source = HASHTABLESIZE-1;
    entry.frequency = 1; // because each neighbor is pushed with an increment
    for( int k = 0; k < i; k++ )
    {
        entry.sink = k;
        __TA_kvTuple* read = __TA_HashTable_read(a, &entry);
        if( !read )
        {
            printf("Failed to recover an entry of nodes (%d,%d) that should exist!\n", entry.source, entry.sink);
        }
        else if( read->frequency != entry.frequency )
        {
                printf("The frequency value for entry (%d,%d) was %lu and the correct answer was %lu!\n", read->source, read->sink, read->frequency, entry.frequency);
        }
    }
}

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
            entry0.sink = i+j;
            while( __TA_HashTable_increment(hashTable, &entry0) )
            {
                __TA_resolveClash(hashTable);
            }
        }
    }

    printf("Now starting the highly connected node\n");
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

    // now print the resulting hash table to a binary file
    __TA_WriteHashTable(hashTable);

    // now make a new hash table and read the output file in
    __TA_HashTable* hashTable2 = (__TA_HashTable*)malloc( sizeof(__TA_HashTable) );
    hashTable2->getFullSize = getFullSize;
    __TA_ReadHashTable(hashTable2, MARKOV_FILE);
    CheckFileAccuracy(hashTable, hashTable2);

    free(hashTable->array);
    free(hashTable);
    free(hashTable2->array);
    free(hashTable2);
    return 0;
}
