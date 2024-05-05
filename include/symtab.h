#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "parser.h"
#include "tac.h"
#include "util.h"

#include "symtab_basicblock.h"
#include "symtab_struct.h"
#include "symtab_function.h"
#include "symtab_scope.h"
#include "symtab_variable.h"

#pragma once

struct SymbolTable
{
    char *name;
    struct Scope *globalScope;
};
/*
 * the create/lookup functions that use an AST (with simpler names) are the primary functions which should be used
 * these provide higher verbosity to error messages (source line/col number associated with errors)
 * the lookup functions with ByString name suffixes should be used only when manipulating pre-existing TAC
 * in this case,
only string names are available and any bad lookups should be caused by internal error cases
 */

// symbol table functions
struct SymbolTable *SymbolTable_new(char *name);

void SymbolTable_print(struct SymbolTable *table,
                       FILE *outFile,
                       char printTAC);

void SymbolTable_collapseScopesRec(struct Scope *scope,
                                   struct Dictionary *dict,
                                   size_t depth);

void SymbolTable_collapseScopes(struct SymbolTable *table,
                                struct Dictionary *dict);

void SymbolTable_free(struct SymbolTable *table);

// AST walk functions

// scrape down a chain of nested child star tokens, expecting something at the bottom
size_t scrapePointers(struct AST *pointerAST,
                      struct AST **resultDestination);
