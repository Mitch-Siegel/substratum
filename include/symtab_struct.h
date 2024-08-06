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

enum StructGenericType
{
    G_NONE,     // not a generic type
    G_BASE,     // a generic type which is a base type (contains code and variables with VT_GENERIC_PARAM type)
    G_INSTANCE, // a generic type which is an instance of a base type (VT_GENERIC_PARAM types resolved to actual types)
};

struct StructEntry
{
    char *name;
    enum StructGenericType genericType;
    union
    {
        struct
        {
            List *paramNames;     // list of string names of generic parameters
            HashTable *instances; // hash table mapping from list of generic parameters (types) to specific instances
        } base;
        struct
        {
            List *parameters; // list of types which are the actual types of the generic parameters
        } instance;
    } generic;
    struct Scope *members;
    Stack *fieldLocations;
    size_t totalSize;
};

struct StructEntry *struct_entry_new(struct Scope *parentScope,
                                     char *name,
                                     enum StructGenericType genericType,
                                     List *genericParamNames);

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

void struct_resolve_generics(struct StructEntry *genericBase, struct StructEntry *instance, List *params);

char *sprint_generic_param_names(List *params);

char *sprint_generic_params(List *paramNames);

void struct_resolve_capital_self(struct StructEntry *theStruct);

#endif
