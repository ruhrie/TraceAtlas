#ifndef DASHHASHTABLE_H
#define DASHHASHTABLE_H

#include <stdint.h>

// size of the array for each array element
// determines the default maximum number of edges a source node is allowed to have before a collision occurs
#define TUPLE_SIZE 15

// defines how many 4 byte integers we need to hash, as well as other things
#define MARKOV_ORDER 2

// output binary file name
#define MARKOV_FILE "markov.bin"

#ifdef __cplusplus
extern "C"
{
#endif

    // always make the size of my structs a power of 2 because this will fit the struct on a single page and cache line
    // we need both the source and the sink because we want a hash, not a hash of hashes
    typedef struct edgeTuple
    {
        uint32_t source;
        uint32_t sink;
        // edge count
        uint64_t frequency;
    } __TA_edgeTuple;

    typedef struct arrayElem
    {
        // number of neighbors an element can have
        __TA_edgeTuple tuple[TUPLE_SIZE];
        // number of active entries in the element
        uint32_t popCount;
        // place holders to align struct to 256 bytes
        uint32_t hold0;
        uint32_t hold1;
        uint32_t hold2;
    } __TA_arrayElem;

    // we can expand either the arrayElem tuple or the HashTable size to resolve clashes
    // we want to keep it shallow (keep arrayElem tuple the same, expand HashTable) because we want constant lookup within the elements
    typedef struct HashTable
    {
        __TA_arrayElem *array;
        // the array size will always be a power of 2, so this size is the exponent of that power
        uint32_t size;
        uint32_t (*getFullSize)(struct HashTable *self);
    } __TA_HashTable;

    /// @brief Converts the size parameter of an __TA_HashTable
    ///
    /// By default __TA_HashTable.size is the power base 2 of the size of the hash table
    /// This function will convert this power to an integer that indicates the base-10 size of the __TA_HashTable
    uint32_t __TA_getFullSize(__TA_HashTable *self);

    // needs an initializer function, and a function to return the next value and increment this thing
    // Motivation
    // 1. On a conflict, we need to double the memory so we need to use this to get the next legal tuple out
    // 2. Dumping all the data to a file, because the data file is in a different format
    // here read is supposed to mean the last valid index I touched (as in, the last index to be read, incremented or written)
    typedef struct HashTable_iterator
    {
        uint32_t kvIndex;
        __TA_arrayElem *lastRead;
    } __TA_HashTable_iterator;

    /// @brief Long index hashing function
    ///
    /// Inspired by the python hashing function, it uses all node IDs involved in an edge to compute the long index
    uint32_t __TA_hash(uint32_t x[MARKOV_ORDER]);

    /// @brief Short index hashing function
    ///
    /// Uses __TA_hash() and the size of the hash table to compute the short index for this entry.
    /// x is an array of all node IDs involved in an edge
    uint32_t __TA_hash_source(uint32_t x[MARKOV_ORDER], uint32_t size);

    /// @brief Find a specified entry within a hash table element
    ///
    /// Each hash table entry is made of TUPLE_SIZE entries
    /// This function searches through all valid entries in a hash table index for an entry matching the sink node ID
    __TA_edgeTuple *__TA_tupleLookup(__TA_arrayElem *entry, __TA_edgeTuple *index);

    /// @brief Find a specified entry in the hash table
    ///
    /// This function hashes the source and sink parameters to index the hash table a.
    __TA_arrayElem *__TA_arrayLookup(__TA_HashTable *a, __TA_edgeTuple *index);

    /// @brief Read from the hash table
    ///
    /// Uses the source and sink members in b to read the given entry from the hash table
    /// If the entry does not exist, this function returns NULL
    __TA_edgeTuple *__TA_HashTable_read(__TA_HashTable *a, __TA_edgeTuple *b);

    /// @brief Write to the hash table
    ///
    /// Hashes the source and sink members in b to write to the corresponding entry in the hash table a
    /// If the element already exists, its frequency count will be overwritten with b->frequency
    /// Otherwise a new entry will be made for b.
    uint8_t __TA_HashTable_write(__TA_HashTable *a, __TA_edgeTuple *b);

    /// @brief Increment an element in the hash table
    ///
    /// Increments the frequency count of the entry corresponding to (b->source,b->sink) by one
    /// If the entry b does not exist, a new entry will be made and its frequency count will be set to 1.
    uint8_t __TA_HashTable_increment(__TA_HashTable *a, __TA_edgeTuple *b);

    /// @brief Resolves clashing in the hash table
    ///
    /// Each hash table entry has a finite set of elements, and it is possible for this buffer to fill up
    /// When this happens, a clash is detected and resolved by doubling the size of the hash table
    void __TA_resolveClash(__TA_HashTable *hashTable);

    /// @brief Write the hash table to a file
    ///
    /// The output file format is binary
    /// The default name for this file is set by the MARKOV_FILE macro
    /// For a custom name, set the MARKOV_FILE environment variable
    void __TA_WriteHashTable(__TA_HashTable *a, uint32_t blockCount);

    /// Read the hash table from the file in path to the hash table a
    /// The file in path must be written in the same format and semantic as described in __TA_WriteHashTable()
    void __TA_ReadHashTable(__TA_HashTable *a, char *path);

#ifdef __cplusplus
}
#endif

#endif