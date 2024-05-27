#ifndef REGALLOC_H
#define REGALLOC_H
#include "substratum_defs.h"

struct Lifetime;
struct RegallocMetadata;
struct MachineInfo;
struct SymbolTable;

// return the heuristic for how good a given lifetime is to spill - higher is better
size_t Lifetime_Heuristic(struct Lifetime *lifetime);

// populate the localStackSize field, aligning and placing any lifetimes which require stack space
void assignStackSpace(struct RegallocMetadata *metadata);

void allocateRegisters(struct RegallocMetadata *metadata, struct MachineInfo *info);

void allocateRegistersForProgram(struct SymbolTable *theTable, struct MachineInfo *info);

#endif
