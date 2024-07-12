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

enum IDFA_ANALYSIS_DIRECTION
{
    D_FORWARDS,
    D_BACKWARDS,
};

struct IdfaContext *idfa_context_create(struct LinkedList *blocks);

void idfa_context_free(struct IdfaContext *context);

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

    ssize_t (*compare_facts)(void *factA, void *factB);
    char *(*sprintFact)(void *factData);
    struct IdfaFacts facts;
    enum IDFA_ANALYSIS_DIRECTION direction;

    // pointer to function returning a struct Set
    struct Set *(*fTransfer)(struct Idfa *idfa, struct BasicBlock *block, struct Set *facts);
    // pointer to function to find the gen and kill set for all basic blocks
    void (*findGenKills)(struct Idfa *idfa);
    // pionter to function taking 2 sets and returning a new set with the meet of them (union or intersection)
    struct Set *(*fMeet)(struct Set *factsA, struct Set *factsB);
};

struct Idfa *idfa_create(struct IdfaContext *context,
                         struct Set *(*fTransfer)(struct Idfa *idfa, struct BasicBlock *block, struct Set *facts), // transfer function
                         void (*findGenKills)(struct Idfa *idfa),                                                  // findGenKills function
                         enum IDFA_ANALYSIS_DIRECTION direction,                                                   // direction which data flows in the analysis
                         ssize_t (*compareFacts)(void *factA, void *factB),                                        // compare function for facts in the domain of the analysis
                         char *(*sprintFact)(void *factData),                                                      // print function for facts in the domain of the analysis, returning a string of the printed data
                         struct Set *(*fMeet)(struct Set *factsA, struct Set *factsB));                            // set operation used to collect data from predecessor/successor blocks

void idfa_print_facts(struct Idfa *idfa);

void idfa_analyze_forwards(struct Idfa *idfa);

void idfa_analyze_backwards(struct Idfa *idfa);

void idfa_analyze(struct Idfa *idfa);

void idfa_redo(struct Idfa *idfa);

void idfa_free(struct Idfa *idfa);

#endif
