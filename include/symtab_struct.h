#ifndef SYMTAB_STRUCT_H
#define SYMTAB_STRUCT_H

#include "ast.h"
#include "symtab_variable.h"

#include "mbcl/hash_table.h"
#include "mbcl/list.h"
#include "mbcl/stack.h"

struct StructField
{
    struct VariableEntry *variable;
    ssize_t offset;
};

struct StructEntry
{
    char *name;
    List *genericParameters;
    HashTable *genericInstantiations;
    struct Scope *members;
    Stack *fieldLocations;
    size_t totalSize;
};

struct StructEntry *struct_entry_new(struct Scope *parentScope,
                                     char *name,
                                     List *genericParams);

void struct_entry_free(struct StructEntry *theStruct);

struct StructEntry *struct_entry_clone(struct StructEntry *toClone, char *name);

// given a VariableEntry corresponding to a struct member which was just declared
// generate a StructField with the aligned location of the member within the struct
void struct_add_field(struct StructEntry *memberOf,
                      struct VariableEntry *variable);

void struct_assign_offsets_to_fields(struct StructEntry *theStruct);

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

struct FunctionEntry *struct_lookup_associated_function_by_string(struct StructEntry *theStruct,
                                                                  char *name);
struct StructEntry *struct_get_or_create_generic_instantiation(struct StructEntry *theStruct, List *paramsList);
#endif
