#ifndef Idfa_H
#define Idfa_H

#include "util.h"

#include "mbcl/array.h"
#include "mbcl/set.h"

struct BasicBlock;

struct IdfaContext
{
    char *name;
    size_t nBlocks; // number of basic blocks
    Array *blocks;  // array of pointers to basic blocks, indexed by block label number
    // indexed by block label number - successors[i] contains the Set of blocks which blocks[i] sends control flow directly to
    Array *successors;
    // indexed by block label number - predecessors[i] contains the Set of blocks which send control flow directly to blocks[i]
    Array *predecessors;
};

enum IDFA_ANALYSIS_DIRECTION
{
    D_FORWARDS,
    D_BACKWARDS,
};

struct IdfaContext *idfa_context_create(char *name, List *blocks);

void idfa_context_free(struct IdfaContext *context);

struct IdfaFacts
{
    // arrays of set pointers, indexed by block
    Array *in;
    Array *out;
    Array *gen;
    Array *kill;
};

struct Idfa
{
    struct IdfaContext *context;

    ssize_t (*compareFacts)(void *factA, void *factB);
    char *(*sprintFact)(void *factData);
    struct IdfaFacts facts;
    enum IDFA_ANALYSIS_DIRECTION direction;

    // pointer to function returning a Set
    Set *(*fTransfer)(struct Idfa *idfa, struct BasicBlock *block, Set *facts);
    // pointer to function to find the gen and kill set for all basic blocks
    void (*findGenKills)(struct Idfa *idfa);
    // pionter to function taking 2 sets and returning a new set with the meet of them (union or intersection)
    Set *(*fMeet)(Set *factsA, Set *factsB);
};

struct Idfa *idfa_create(struct IdfaContext *context,
                         Set *(*fTransfer)(struct Idfa *idfa, struct BasicBlock *block, Set *facts), // transfer function
                         void (*findGenKills)(struct Idfa *idfa),                                    // findGenKills function
                         enum IDFA_ANALYSIS_DIRECTION direction,                                     // direction which data flows in the analysis
                         ssize_t (*compareFacts)(void *factA, void *factB),                          // compare function for facts in the domain of the analysis
                         char *(*sprintFact)(void *factData),                                        // print function for facts in the domain of the analysis, returning a string of the printed data
                         Set *(*fMeet)(Set *factsA, Set *factsB));                                   // set operation used to collect data from predecessor/successor blocks

void idfa_print_facts(struct Idfa *idfa);

void idfa_analyze_forwards(struct Idfa *idfa);

void idfa_analyze_backwards(struct Idfa *idfa);

void idfa_analyze(struct Idfa *idfa);

void idfa_redo(struct Idfa *idfa);

void idfa_free(struct Idfa *idfa);

#endif
