#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "parser.h"
#include "tac.h"
#include "util.h"

#include "enum_desc.h"
#include "struct_desc.h"
#include "symtab_basicblock.h"
#include "symtab_function.h"
#include "symtab_scope.h"
#include "symtab_trait.h"
#include "symtab_type.h"
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
struct SymbolTable *symbol_table_new(char *name);

void symbol_table_print(struct SymbolTable *table,
                        FILE *outFile,
                        bool printTac);

void symbol_table_dump_dot(FILE *outFile,
                           struct SymbolTable *table,
                           bool printTAC);

void symbol_table_print_cfgs(struct SymbolTable *table, char *outDir);

Set *symbol_table_collapse_scopes_rec(struct Scope *scope,
                                      struct Dictionary *dict,
                                      size_t depth);

void symbol_table_collapse_scopes(struct SymbolTable *table,
                                  struct Dictionary *dict);

void symbol_table_free(struct SymbolTable *table);

// AST walk functions

// scrape down a chain of nested child star tokens, expecting something at the bottom
size_t scrape_pointers(struct Ast *pointerAst,
                       struct Ast **resultDestination);
