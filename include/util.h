#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "substratum_defs.h"

#pragma once

enum CompilerErrors
{
    ERROR_INVOCATION = 1, // user has made an error with arguments or other parameters
    ERROR_CODE,           // there is an error in the code which prevents a complete compilation
    ERROR_INTERNAL,
};

#define ErrorAndExit(code, fmt, ...)                           \
    printf(fmt, ##__VA_ARGS__);                                \
    printf("Bailing from file %s:%d\n\n", __FILE__, __LINE__); \
    exit(code)

#define ErrorWithAST(code, astPtr, fmt, ...)                                                \
    printf("%s:%d:%d:\n", (astPtr)->sourceFile, (astPtr)->sourceLine, (astPtr)->sourceCol); \
    ErrorAndExit(code, fmt, ##__VA_ARGS__)

#define STAGE_PARSE 0
#define STAGE_LINEARIZE 1
#define STAGE_REGALLOC 2
#define STAGE_CODEGEN 3
#define STAGE_MAX 4

#define VERBOSITY_SILENT 0
#define VERBOSITY_MINIMAL 1
#define VERBOSITY_MAX 2
struct Config
{
    u8 stageVerbosities[STAGE_MAX];
};

extern u8 currentVerbosity;

struct ParseProgress
{
    size_t curLine;
    size_t curCol;
    char *curFile;
    size_t curLineRaw;
    size_t curColRaw;
    FILE *f;
    struct Dictionary *dict;
    struct LinkedList *charsRemainingPerLine;
    size_t lastMatchLocation; // location of last parser match relative to pcc buffer
    char eofReceived;
};

u8 alignSize(size_t nBytes);

size_t unalignSize(u8 nBits);

/*
 *
 *
 */

struct HashTableEntry
{
    void *key;
    void *value;
    int (*compareFunction)(void *keyA, void *keyB);
};

struct HashTable
{
    struct Set **buckets;
    size_t nBuckets;
    size_t (*hashFunction)(void *key);
    int (*compareFunction)(void *keyA, void *keyB);
    void (*keyFreeFunction)(void *data);
    void (*valueFreeFunction)(void *data);
};

struct HashTable *HashTable_New(size_t nBuckets,
                                size_t (*hashFunction)(void *key),
                                int (*compareFunction)(void *keyA, void *keyB),
                                void (*keyFreeFunction)(void *data),
                                void (*valueFreeFunction)(void *data));

void *HashTable_Lookup(struct HashTable *table, void *key);

void HashTable_Insert(struct HashTable *table, void *key, void *value);

void *HashTable_Delete(struct HashTable *table, void *key);

void HashTable_Free(struct HashTable *table);

/*
 * Dictionary for tracking strings
 * Economizes heap space by only storing strings once each
 * Uses a simple hash table which supports different bucket counts
 */
struct Dictionary
{
    struct HashTable *table;
};

size_t hashString(void *data);

struct Dictionary *Dictionary_New(size_t nBuckets);

char *Dictionary_Insert(struct Dictionary *dict, char *value);

char *Dictionary_LookupOrInsert(struct Dictionary *dict, char *value);

void Dictionary_Free(struct Dictionary *dict);

/*
 * Stack data structure
 *
 */

#define STACK_DEFAULT_ALLOCATION 20
#define STACK_SCALE_FACTOR 2

struct Stack
{
    void **data;
    size_t size;
    size_t allocated;
};

struct Stack *Stack_New();

void Stack_Free(struct Stack *stack);

void Stack_Push(struct Stack *stack, void *data);

void *Stack_Pop(struct Stack *stack);

void *Stack_Peek(struct Stack *stack);

/*
 * Unordered List data structure
 *
 */

struct LinkedListNode
{
    struct LinkedListNode *next;
    struct LinkedListNode *prev;
    void *data;
};

struct LinkedList
{
    struct LinkedListNode *head;
    struct LinkedListNode *tail;
    size_t size;
};

struct LinkedList *LinkedList_New();

void LinkedList_Free(struct LinkedList *list, void (*dataFreeFunction)());

void LinkedList_Append(struct LinkedList *list, void *element);

void LinkedList_Prepend(struct LinkedList *list, void *element);

// join all elements of list 'after' after those of list 'before' in list 'before'
void LinkedList_Join(struct LinkedList *before, struct LinkedList *after);

void *LinkedList_Delete(struct LinkedList *list, int (*compareFunction)(), void *element);

void *LinkedList_Find(struct LinkedList *list, int (*compareFunction)(), void *element);

void *LinkedList_PopFront(struct LinkedList *list);

void *LinkedList_PopBack(struct LinkedList *list);

/*
 * Set data structure
 */

struct Set
{
    struct LinkedList *elements;
    int (*compareFunction)(void *elementA, void *elementB);
};

struct Set *Set_New(int (*compareFunction)(void *elementA, void *elementB));

void Set_Insert(struct Set *set, void *element);

void Set_Delete(struct Set *set, void *element);

void *Set_Find(struct Set *set, void *element);

void Set_Merge(struct Set *into, struct Set *from);

struct Set *Set_Copy(struct Set *set);

// given two input sets, construct and return a third set containing data from the union of the two
struct Set *Set_Union(struct Set *setA, struct Set *setB);

// given two input sets, construct and return a third set containing data from the intersection of the two
struct Set *Set_Intersection(struct Set *setA, struct Set *setB);

void Set_Free(struct Set *set);

/*
 * TempList is a struct containing string names for TAC temps by number (eg t0, t1, t2, etc...)
 * _Get retrieves the string for the given number, or if it doesn't exist, generates it and then returns it
 *
 */

struct TempList
{
    struct Stack *temps;
};

// get the string for a given temp num
char *TempList_Get(struct TempList *tempList, size_t tempNum);

// create a new templist
struct TempList *TempList_New();

// free the templist
void TempList_Free(struct TempList *toFree);
