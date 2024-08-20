#ifndef SYMTAB_TRAIT_H
#define SYMTAB_TRAIT_H

#include "symtab_scope.h"

struct TraitEntry
{
    char *name;
    Set *functions;
};

struct TraitEntry *trait_new(char *name, struct Scope *parentScope);

void trait_free(struct TraitEntry *trait);

void trait_entry_print(struct TraitEntry *trait, size_t depth, FILE *outFile);

ssize_t trait_entry_compare(void *traitDataA, void *traitDataB);

#endif
