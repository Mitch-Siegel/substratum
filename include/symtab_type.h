#ifndef SYMTAB_TYPE_H
#define SYMTAB_TYPE_H

#include "mbcl/hash_table.h"
#include "mbcl/set.h"
#include "struct_desc.h"
#include "type.h"

enum TYPE_PERMUTATION
{
    TP_PRIMITIVE,
    TP_STRUCT,
    TP_ENUM,
};

struct TypeEntry
{
    char *name;
    enum TYPE_PERMUTATION permutation;
    struct Type type;
    union
    {
        struct StructDesc *asStruct;
        struct EnumDesc *asEnum;
    } data;
    enum GENERIC_TYPE genericType;
    union
    {
        struct
        {
            List *paramNames;     // list of string names of generic parameters
            HashTable *instances; // hash table mapping from list of generic parameters (as types) to specific instances (as TypeEntry)
        } base;
        struct
        {
            List *parameters; // list of types which are the actual types of the generic parameters
        } instance;
    } generic;
    struct Scope *parentScope;
    Set *traits;
    Set *implemented;
    HashTable *implementedByName;
};

struct TypeEntry *type_entry_new_primitive(enum BASIC_TYPES basicType);

struct TypeEntry *type_entry_new_struct(char *name, struct Scope *parentScope, enum GENERIC_TYPE genericType, List *genericParamNames);

struct TypeEntry *type_entry_new_enum(char *name, struct Scope *parentScope, enum GENERIC_TYPE genericType, List *genericParamNames);

void type_entry_free(struct TypeEntry *entry);

void type_entry_add_implemented(struct TypeEntry *entry, struct FunctionEntry *implemented);

struct TraitEntry *type_entry_lookup_trait(struct TypeEntry *typeEntry, char *name);

struct FunctionEntry *type_entry_lookup_implemented_by_name(struct TypeEntry *typeEntry, char *name);

void type_entry_resolve_capital_self(struct TypeEntry *theType);

struct FunctionEntry *type_entry_lookup_associated_function(struct TypeEntry *theStruct,
                                                            struct Ast *nameTree,
                                                            struct Scope *scope);

void type_entry_resolve_generics(struct TypeEntry *instance, List *paramNames, List *paramTypes);

struct TypeEntry *type_entry_get_or_create_generic_instantiation(struct TypeEntry *baseType, List *paramsList);

char *sprint_generic_param_names(List *paramNames);

char *sprint_generic_params(List *params);

void type_entry_resolve_capital_self(struct TypeEntry *theStruct);

void type_entry_print(struct TypeEntry *theType, bool printTac, size_t depth, FILE *outFile);

#endif
