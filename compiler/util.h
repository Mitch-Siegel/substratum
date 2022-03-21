#include "string.h"
#include "stdlib.h"
#include "stdio.h"

#pragma once

/*
 * Dictionary for tracking strings
 * Economizes heap space by only storing strings once each
 * Uses a simple hash table which supports different bucket counts
 */
struct DictionaryEntry
{
    char *data;
    struct DictionaryEntry *next;
};

struct Dictionary
{
    struct DictionaryEntry **buckets;
    int nBuckets;
};

unsigned int hash(char *str);

struct Dictionary *newDictionary(int nBuckets);

char *DictionaryInsert(struct Dictionary *dict, char *value);

char *DictionaryLookup(struct Dictionary *dict, char *value);

char *DictionaryLookupOrInsert(struct Dictionary *dict, char *value);

void freeDictionary(struct Dictionary *dict);

/*
 * Stack data structure
 *
 */

struct Stack
{
    void **data;
    int size;
    int allocated;
};

struct Stack *newStack();

void freeStack(struct Stack *s);

void StackPush(struct Stack *s, void *data);

void *StackPop(struct Stack *s);

void *StackPeek(struct Stack *s);

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
    int size;
};

struct LinkedList *LinkedList_new();

void LinkedList_free(struct LinkedList *l, char freeData);

void LinkedList_insert(struct LinkedList *l, void *element);

void *LinkedList_delete(struct LinkedList *l, char (*compareFunction)(), void *element);

void *LinkedList_find(struct LinkedList *l, char (*compareFunction)(), void *element);