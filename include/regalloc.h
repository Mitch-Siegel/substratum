#include "regalloc_generic.h"

// return the heuristic for how good a given lifetime is to spill - higher is better
int lifetimeHeuristic(struct Lifetime *lt);

// populate the localStackSize field, aligning and placing any lifetimes which require stack space
void assignStackSpace(struct CodegenMetadata *m);

void allocateRegisters(struct CodegenMetadata *metadata);
