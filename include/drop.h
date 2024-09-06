#ifndef DROP_H
#define DROP_H

#include "symtab.h"

#define DROP_TRAIT_NAME "Drop"
#define DROP_TRAIT_FUNCTION_NAME "drop"

void add_drops(struct SymbolTable *table);

struct FunctionEntry *drop_create_function_prototype(struct Scope *scope);

#endif
