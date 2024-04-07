#ifndef REGALLOC_H
#define REGALLOC_H
#include "substratum_defs.h"

struct Lifetime;
struct CodegenMetadata;

// return the heuristic for how good a given lifetime is to spill - higher is better
i32 lifetimeHeuristic(struct Lifetime *lifetime);

// populate the localStackSize field, aligning and placing any lifetimes which require stack space
void assignStackSpace(struct CodegenMetadata *metadata);

void allocateRegisters(struct CodegenMetadata *metadata);

#endif
