#ifndef REGALLOC_H
#define REGALLOC_H
#include "substratum_defs.h"

struct Lifetime;
struct RegallocMetadata;
struct MachineInfo;
struct SymbolTable;

// return the heuristic for how good a given lifetime is to spill - higher is better
size_t lifetime_heuristic(struct Lifetime *lifetime);

void allocate_registers(struct RegallocMetadata *metadata, struct MachineInfo *info);

void allocate_registers_for_program(struct SymbolTable *theTable, struct MachineInfo *info);

#endif
