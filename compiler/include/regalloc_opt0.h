#include "regalloc.h"

// return the heuristic for how good a given lifetime is to spill - higher is better
int lifetimeHeuristic_0(struct Lifetime *lt);

int allocateRegisters_0(struct CodegenMetadata *metadata);
