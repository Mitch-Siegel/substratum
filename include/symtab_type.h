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
    char *baseName;
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
    struct Scope *implemented;
    HashTable *implementedByName;
};

struct TypeEntry *type_entry_new_primitive(struct Scope *parentScope, enum BASIC_TYPES basicType);

struct TypeEntry *type_entry_new_struct(char *name, struct Scope *parentScope, enum GENERIC_TYPE genericType, List *genericParamNames);

struct TypeEntry *type_entry_new_enum(char *name, struct Scope *parentScope, enum GENERIC_TYPE genericType, List *genericParamNames);

void type_entry_free(struct TypeEntry *entry);

char *type_entry_name(struct TypeEntry *entry);

void type_entry_check_implemented_access(struct TypeEntry *theType,
                                         struct Ast *nameTree,
                                         struct Scope *accessedFromScope,
                                         char *whatAccessingCalled);

void type_entry_add_implemented(struct TypeEntry *entry, struct FunctionEntry *implemented, enum ACCESS accessibility);

void try_resolve_generic_for_type(struct Type *type, HashTable *paramsMap, char *resolvedStructName, List *resolvedParams);

struct TraitEntry *type_entry_lookup_trait(struct TypeEntry *typeEntry, char *name);

struct FunctionEntry *type_entry_lookup_implemented(struct TypeEntry *typeEntry, struct Scope *scope, struct Ast *nameTree);

struct FunctionEntry *type_entry_lookup_method(struct TypeEntry *typeEntry,
                                               struct Ast *nameTree,
                                               struct Scope *scope);

struct FunctionEntry *type_entry_lookup_associated_function(struct TypeEntry *typeEntry,
                                                            struct Ast *nameTree,
                                                            struct Scope *scope);

void type_entry_resolve_generics(struct TypeEntry *instance, List *paramNames, List *paramTypes);

struct TypeEntry *type_entry_get_or_create_generic_instantiation(struct TypeEntry *baseType, List *paramsList);

void type_entry_verify_trait(struct Ast *implTree,
                             struct TypeEntry *implementedFor,
                             struct TraitEntry *implementedTrait,
                             Set *implementedPrivate,
                             Set *implementedPublic);

char *sprint_generic_param_names(List *paramNames);

char *sprint_generic_params(List *params);

void type_entry_resolve_capital_self(struct TypeEntry *typeEntry);

void type_entry_print(struct TypeEntry *theType, bool printTac, size_t depth, FILE *outFile);

#endif
