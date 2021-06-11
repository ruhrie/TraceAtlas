#include "Backend/DashHashTable.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define HASH_MULTIPLIER 1000003UL
#define HASH_MULTIPLIER_OFFSET 82520UL
#define HASH_INITIAL 0x12345678UL
#define HASH_OFFSET 97531UL

#ifdef __cplusplus
extern "C"
{
#endif
    // used for expanding the size exponent of a HashTable struct
    uint32_t __TA_getFullSize(__TA_HashTable *self)
    {
        return 0x1 << self->size;
    }

    // used for hashing an integer
    uint32_t __TA_hash(uint32_t x[MARKOV_ORDER])
    {
        uint32_t m = HASH_MULTIPLIER;
        uint32_t y = HASH_INITIAL;
        for (int i = 0; i < MARKOV_ORDER; i++)
        {
            y += (x[i] >> 16) ^ x[i] * m + HASH_OFFSET;
            m += HASH_MULTIPLIER_OFFSET + MARKOV_ORDER + MARKOV_ORDER;
        }

        return y;
    };

    // here size is ceil( log2(arraySize) )
    uint32_t __TA_hash_source(uint32_t x[MARKOV_ORDER], uint32_t size)
    {
        // instead of doing a mask, I should do a right shift
        // so instead of modulo, we want to do a right shift by (32-arraysize)
        uint32_t shortHash = 0;
        uint32_t longHash = __TA_hash(x);
        uint32_t mask = 0x1;
        for (uint32_t i = 0; i < size; i++)
        {
            shortHash += longHash & mask;
            mask = mask << 1;
        }
        return shortHash;
    }

    __TA_kvTuple *__TA_tupleLookup(__TA_arrayElem *entry, __TA_kvTuple *index)
    {
        for (uint32_t i = 0; i < entry->popCount; i++)
        {
            if ((entry->tuple[i].source == index->source) && (entry->tuple[i].sink == index->sink))
            {
                return &entry->tuple[i];
            }
        }
        return NULL;
    }

    __TA_arrayElem *__TA_arrayLookup(__TA_HashTable *a, __TA_kvTuple *index)
    {
        uint32_t x[MARKOV_ORDER] = {index->source, index->sink};
#if __TA_DEBUG
        __TA_arrayElem *index = &a->array[__TA_hash_source(x, a->size)];
        // check to see if there was a clash in the hashing function
        if (index->popCount)
        {
            if (index->tuple[0].source != source)
            {
                // we have a clash, not sure how to resolve it yet
                printf("We have a clash between existing entry %d and incoming entry %d!\n", index->tuple[0].source, source);
            }
        }
        return index;
#else
    return &a->array[__TA_hash_source(x, a->size)];
#endif
    }

    __TA_kvTuple *__TA_HashTable_read(__TA_HashTable *a, __TA_kvTuple *b)
    {
        __TA_arrayElem *entry = __TA_arrayLookup(a, b);
        return __TA_tupleLookup(entry, b);
    }

    uint8_t __TA_HashTable_write(__TA_HashTable *a, __TA_kvTuple *b)
    {
        __TA_arrayElem *index = __TA_arrayLookup(a, b);
        __TA_kvTuple *entry = __TA_tupleLookup(index, b);
        if (entry)
        {
            entry->frequency = b->frequency;
        }
        else
        {
            // check to see if we have a collision, and if we do return error message
            if (index->popCount == TUPLE_SIZE)
            {
                return 1;
            }
            // we just have to make a new entry
            else
            {
                index->tuple[index->popCount].source = b->source;
                index->tuple[index->popCount].sink = b->sink;
                index->tuple[index->popCount].frequency = b->frequency;
                index->popCount++;
            }
        }
        return 0;
    }

    uint8_t __TA_HashTable_increment(__TA_HashTable *a, __TA_kvTuple *b)
    {
        __TA_arrayElem *index = __TA_arrayLookup(a, b);
        __TA_kvTuple *entry = __TA_tupleLookup(index, b);
        if (entry)
        {
            (entry->frequency)++;
        }
        else
        {
            // check to see if we have a collision, and if we do return error message
            if (index->popCount == TUPLE_SIZE)
            {
                return 1;
            }
            // we just have to make a new entry
            else
            {
                index->tuple[index->popCount].source = b->source;
                index->tuple[index->popCount].sink = b->sink;
                index->tuple[index->popCount].frequency = b->frequency;
                index->tuple[index->popCount].frequency++;
                index->popCount++;
            }
        }
        return 0;
    }

    void __TA_resolveClash(__TA_HashTable *hashTable)
    {
        printf("Found a clash!\n");
        // local copy of the old hashTable
        __TA_HashTable old;
        old.size = hashTable->size;
        old.array = hashTable->array;
        old.getFullSize = hashTable->getFullSize;
        // we double the size of the array each time there is a clash
        hashTable->size++;
        // reallocate a new array that has double the current entries
        hashTable->array = (__TA_arrayElem *)malloc(hashTable->getFullSize(hashTable) * sizeof(__TA_arrayElem));
        // put in everything from the old array
        for (uint32_t i = 0; i < old.getFullSize(&old); i++)
        {
            for (uint32_t j = 0; j < old.array[i].popCount; j++)
            {
                __TA_HashTable_write(hashTable, &old.array[i].tuple[j]);
            }
        }
        free(old.array);
        printf("Resolved the clash!\n");
    }

    void __TA_WriteHashTable(__TA_HashTable *a, uint32_t blockCount)
    {
        char *p = getenv("MARKOV_FILE");
        FILE *f;
        if (p)
        {
            f = fopen(p, "wb");
        }
        else
        {
            f = fopen(MARKOV_FILE, "wb");
        }
        // first write the number of nodes in the graph
        fwrite(&blockCount, sizeof(uint32_t), 1, f);
        // second write the number of edges in the file
        uint32_t edges = 0;
        uint32_t liveArrayEntries = 0;
        uint32_t maxPopCount = 0;
        for (uint32_t i = 0; i < a->getFullSize(a); i++)
        {
            if (a->array[i].popCount)
            {
                liveArrayEntries++;
                if (a->array[i].popCount > maxPopCount)
                {
                    maxPopCount = a->array[i].popCount;
                }
            }
            edges += a->array[i].popCount;
        }
        fwrite(&edges, sizeof(uint32_t), 1, f);

        // third, write all the edges
        for (uint32_t i = 0; i < a->getFullSize(a); i++)
        {
            for (uint32_t j = 0; j < a->array[i].popCount; j++)
            {
                fwrite(&a->array[i].tuple[j], sizeof(__TA_kvTuple), 1, f);
            }
        }
        fclose(f);
        // calculate some statistics about our hash table
        // number of nodes
        printf("\nHASHTABLESIZE: %d\n", blockCount);
        // number of edges
        printf("\nHASHTABLEEDGES: %d\n", edges);
        // live array entries
        printf("\nHASHTABLELIVEARRAYENTRIES: %d\n", liveArrayEntries);
        // maximum occupancy of an array element
        printf("\nHASHTABLEMAXPOPCOUNT: %d\n", maxPopCount);
    }

    void __TA_ReadHashTable(__TA_HashTable *a, char *path)
    {
        FILE *f = fopen(path, "rb");
        // the first 4 bytes is a uint32_t of how many nodes there are in the graph
        uint32_t nodes;
        fread(&nodes, sizeof(uint32_t), 1, f);
        a->size = (uint32_t)(ceil(log((double)nodes) / log(2.0)));
        a->array = (__TA_arrayElem *)malloc(a->getFullSize(a) * sizeof(__TA_arrayElem));

        // the second 4 bytes is a uint32_t of how many edges there are in the file
        uint32_t edges;
        fread(&edges, sizeof(uint32_t), 1, f);

        // read all the edges
        __TA_kvTuple newEntry;
        for (uint32_t i = 0; i < edges; i++)
        {
            fread(&newEntry, sizeof(__TA_kvTuple), 1, f);
            __TA_HashTable_write(a, &newEntry);
        }
        fclose(f);
    }

#ifdef __cplusplus
}
#endif