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
 * Dictionary for tracking strings
 * Economizes heap space by only storing strings once each
 * Uses a simple hash table which supports different bucket counts
 */
struct Dictionary
{
    struct LinkedList **buckets;
    size_t nBuckets;
};

unsigned int hash(char *str);

struct Dictionary *Dictionary_New(size_t nBuckets);

char *Dictionary_Insert(struct Dictionary *dict, char *value);

char *Dictionary_Lookup(struct Dictionary *dict, char *value);

char *Dictionary_LookupOrInsert(struct Dictionary *dict, char *value);

void Dictionary_Free(struct Dictionary *dict);

/*
 * Stack data structure
 *
 */

#define STACK_DEFAULT_ALLOCATION 20
#define STACK_SCALE_FACTOR 1.5

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
