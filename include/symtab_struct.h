#ifndef SYMTAB_STRUCT_H
#define SYMTAB_STRUCT_H

#include "ast.h"
#include "symtab_variable.h"

struct StructField
{
    struct VariableEntry *variable;
    ssize_t offset;
};

struct StructEntry
{
    char *name;
    struct Scope *members;
    struct Stack *fieldLocations;
    size_t totalSize;
};

void struct_entry_free(struct StructEntry *theStruct);

// given a VariableEntry corresponding to a struct member which was just declared
// generate a StructField with the aligned location of the member within the struct
void struct_assign_offset_to_field(struct StructEntry *memberOf,
                                   struct VariableEntry *variable);

struct StructField *struct_lookup_field(struct StructEntry *theStruct,
                                        struct Ast *nameTree,
                                        struct Scope *scope);

struct StructField *struct_lookup_field_by_name(struct StructEntry *theStruct,
                                                char *name,
                                                struct Scope *scope);

struct FunctionEntry *struct_looup_method(struct StructEntry *theStruct,
                                          struct Ast *name,
                                          struct Scope *scope);

struct FunctionEntry *struct_lookup_associated_function(struct StructEntry *theStruct,
                                                        struct Ast *name,
                                                        struct Scope *scope);

// TODO: char *name vs AST *name (change to AST *nameTree?)
struct FunctionEntry *struct_lookup_method_by_string(struct StructEntry *theStruct,
                                                     char *name);

#endif
