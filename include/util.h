#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "substratum_defs.h"

#include "mbcl/array.h"
#include "mbcl/list.h"

#pragma once

enum COMPILER_ERRORS
{
    ERROR_INVOCATION = 1, // user has made an error with arguments or other parameters
    ERROR_CODE,           // there is an error in the code which prevents a complete compilation
    ERROR_INTERNAL,
};

struct ParseProgress
{
    size_t curLine;
    size_t curCol;
    char *curFile;
    size_t curLineRaw;
    size_t curColRaw;
    FILE *f;
    struct Dictionary *dict;
    List *charsRemainingPerLine;
    size_t lastMatchLocation; // location of last parser match relative to pcc buffer
    char eofReceived;
};

u8 align_size(size_t nBytes);

size_t unalign_size(u8 nBits);

// directly compares dataA to dataB
ssize_t ssizet_compare(void *dataA, void *dataB);

ssize_t sizet_pointer_compare(void *dataA, void *dataB);

size_t parse_hex_constant(char *hexConstant);

/*
 *
 *
 */

struct HashTableEntry
{
    void *key;
    void *value;
    ssize_t (*compareFunction)(void *keyA, void *keyB);
    void (*keyFreeFunction)(void *key);
    void (*valueFreeFunction)(void *value);
};

struct HashTable
{
    struct Set **buckets;
    size_t nBuckets;
    size_t (*hashFunction)(void *key);
    ssize_t (*compareFunction)(void *keyA, void *keyB);
    void (*keyFreeFunction)(void *data);
    void (*valueFreeFunction)(void *data);
};

struct HashTable *hash_table_new(size_t nBuckets,
                                 size_t (*hashFunction)(void *key),
                                 ssize_t (*compareFunction)(void *keyA, void *keyB),
                                 void (*keyFreeFunction)(void *data),
                                 void (*valueFreeFunction)(void *data));

void *hash_table_lookup(struct HashTable *table, void *key);

void hash_table_insert(struct HashTable *table, void *key, void *value);

void hash_table_delete(struct HashTable *table, void *key);

void hash_table_free(struct HashTable *table);

/*
 * Dictionary for tracking strings
 * Economizes heap space by only storing strings once each
 * Uses a simple hash table which supports different bucket counts
 */
struct Dictionary
{
    struct HashTable *table;
    void *(*duplicateFunction)(void *data);
};

size_t hash_string(void *data);

struct Dictionary *dictionary_new(size_t nBuckets,
                                  void *(*duplicateFunction)(void *),
                                  size_t (*hashFunction)(void *data),
                                  ssize_t (*compareFunction)(void *dataA, void *dataB),
                                  void (*dataFreeFunction)(void *));

void *dictionary_insert(struct Dictionary *dict, void *value);

void *dictionary_lookup_or_insert(struct Dictionary *dict, void *value);

void dictionary_free(struct Dictionary *dict);

/*
 * Unordered List data structure
 *
 */

/*
 * TempList is a struct containing string names for TAC temps by number (eg t0, t1, t2, etc...)
 * _Get retrieves the string for the given number, or if it doesn't exist, generates it and then returns it
 *
 */

struct TempList
{
    Array temps;
};

// get the string for a given temp num
char *temp_list_get(struct TempList *tempList, size_t tempNum);

// create a new templist
struct TempList *temp_list_new();

// free the templist
void temp_list_free(struct TempList *toFree);
