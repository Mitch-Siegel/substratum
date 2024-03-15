#include "regalloc_generic.h"

// return the heuristic for how good a given lifetime is to spill - higher is better
int lifetimeHeuristic(struct Lifetime *lt);

void allocateRegisters(struct CodegenMetadata *metadata);
