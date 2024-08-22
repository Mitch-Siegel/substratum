#ifndef STRUCT_DESC_H
#define STRUCT_DESC_H

#include "ast.h"
#include "symtab_variable.h"

#include "mbcl/deque.h"
#include "mbcl/hash_table.h"
#include "mbcl/list.h"
#include "mbcl/stack.h"

struct StructField
{
    struct VariableEntry *variable;
    ssize_t offset;
};

enum GENERIC_TYPE
{
    G_NONE,     // not a generic type
    G_BASE,     // a generic type which is a base type (contains code and variables with VT_GENERIC_PARAM type)
    G_INSTANCE, // a generic type which is an instance of a base type (VT_GENERIC_PARAM types resolved to actual types)
};

struct StructDesc
{
    char *name;
    struct Scope *members;
    Deque *fieldLocations;
    size_t totalSize;
};

struct StructDesc *struct_desc_new(struct Scope *parentScope,
                                   char *name);

void struct_desc_free(struct StructDesc *theStruct);

struct StructDesc *struct_desc_clone(struct StructDesc *toClone, char *name);

// given a VariableEntry corresponding to a struct member which was just declared
// generate a StructField with the aligned location of the member within the struct
void struct_add_field(struct StructDesc *memberOf,
                      struct VariableEntry *variable);

void struct_assign_offsets_to_fields(struct StructDesc *theStruct);

struct StructField *struct_lookup_field(struct StructDesc *theStruct,
                                        struct Ast *nameTree,
                                        struct Scope *scope);

struct StructField *struct_lookup_field_by_name(struct StructDesc *theStruct,
                                                char *name,
                                                struct Scope *scope);

struct FunctionEntry *struct_looup_method(struct StructDesc *theStruct,
                                          struct Ast *name,
                                          struct Scope *scope);

// TODO: char *name vs AST *name (change to AST *nameTree?)
struct FunctionEntry *struct_lookup_method_by_string(struct StructDesc *theStruct,
                                                     char *name);

struct FunctionEntry *struct_lookup_associated_function_by_string(struct StructDesc *theStruct,
                                                                  char *name);

char *struct_name(struct StructDesc *theStruct);

void struct_desc_print(struct StructDesc *theStruct, size_t depth, FILE *outFile);

void struct_desc_resolve_generics(struct StructDesc *theStruct, HashTable *paramsMap, char *name, List *params);

#endif
