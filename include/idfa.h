#ifndef Idfa_H
#define Idfa_H

#include "util.h"

struct IdfaContext
{
    size_t nBlocks;             // number of basic blocks
    struct BasicBlock **blocks; // array of pointers to basic blocks, indexed by block label number
    // indexed by block label number - successors[i] contains blocks which blocks[i] sends control flow directly to
    struct Set **successors;
    // indexed by block label number - predecessors[i] contains blocks which send control flow directly to blocks[i]
    struct Set **predecessors;
};

struct IdfaContext *IdfaContext_Create(struct LinkedList *blocks);

struct IdfaFacts
{
    struct Set **in;
    struct Set **out;
    struct Set **gen;
    struct Set **kill;
};

struct Idfa
{
    struct IdfaContext *context;

    int (*compareFacts)(void *factA, void *factB);
    struct IdfaFacts facts;

    // pointer to function returning a struct Set
    struct Set *(*fTransfer)(struct Idfa *idfa, struct BasicBlock *block, struct Set *facts);
    void (*findGenKills)(struct Idfa *idfa);
};

struct Idfa *Idfa_Create(struct IdfaContext *context,
                         struct Set *(*fTransfer)(struct Idfa *idfa, struct BasicBlock *block, struct Set *facts), // transfer function
                         void (*findGenKills)(struct Idfa *idfa),                                                  // findGenKills function
                         int (*compareFacts)(void *factA, void *factB));                                           // compare function for facts in the domain of the analysis

void Idfa_AnalyzeForwards(struct Idfa *idfa);

void Idfa_AnalyzeBackwards(struct Idfa *idfa);

#endif
