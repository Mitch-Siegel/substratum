#include "regalloc_generic.h"

// return the heuristic for how good a given lifetime is to spill - higher is better
int lifetimeHeuristic(struct Lifetime *lifetime);

// populate the localStackSize field, aligning and placing any lifetimes which require stack space
void assignStackSpace(struct CodegenMetadata *metadata);

void allocateRegisters(struct CodegenMetadata *metadata);
