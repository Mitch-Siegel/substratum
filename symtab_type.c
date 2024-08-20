#include "symtab_type.h"
#include "log.h"
#include "symtab_enum.h"
#include "symtab_function.h"
#include "symtab_struct.h"
#include "symtab_trait.h"
#include "util.h"

struct TypeEntry *type_entry_new(enum TYPE_PERMUTATION permutation, struct Type type)
{
    struct TypeEntry *wipType = malloc(sizeof(struct TypeEntry));
    memset(wipType, 0, sizeof(struct TypeEntry));
    wipType->permutation = permutation;
    wipType->type = type;
    wipType->traits = set_new(NULL, trait_entry_compare);
    wipType->implemented = set_new((void (*)(void *))function_entry_free, function_entry_compare);
    wipType->implementedByName = hash_table_new(free, NULL, (ssize_t(*)(void *, void *))strcmp, hash_string, 10);

    return wipType;
}

struct TypeEntry *type_entry_new_primitive(enum BASIC_TYPES basicType)
{
    struct Type primitiveType = {0};
    type_init(&primitiveType);
    type_set_basic_type(&primitiveType, basicType, NULL, 0);
    struct TypeEntry *wipPrimitive = type_entry_new(TP_PRIMITIVE, primitiveType);
    return wipPrimitive;
}

struct TypeEntry *type_entry_new_struct(char *name, struct Scope *parentScope, enum STRUCT_GENERIC_TYPE genericType, List *genericParamNames)
{
    struct Type primitiveType = {0};
    type_init(&primitiveType);
    type_set_basic_type(&primitiveType, VT_STRUCT, name, 0);
    struct TypeEntry *wipPrimitive = type_entry_new(TP_ENUM, primitiveType);
    wipPrimitive->data.asStruct = struct_entry_new(parentScope, name, genericType, genericParamNames);
    return wipPrimitive;
}

struct TypeEntry *type_entry_new_enum(char *name, struct Scope *parentScope)
{
    struct Type primitiveType = {0};
    type_init(&primitiveType);
    type_set_basic_type(&primitiveType, VT_ENUM, name, 0);
    struct TypeEntry *wipPrimitive = type_entry_new(TP_ENUM, primitiveType);
    wipPrimitive->data.asEnum = enum_entry_new(name, parentScope);
    return wipPrimitive;
}

void type_entry_free(struct TypeEntry *entry)
{
    set_free(entry->traits);
    set_free(entry->implemented);
    hash_table_free(entry->implementedByName);

    switch (entry->permutation)
    {
    case TP_PRIMITIVE:
        break;

    case TP_STRUCT:
        struct_entry_free(entry->data.asStruct);
        break;

    case TP_ENUM:
        enum_entry_free(entry->data.asEnum);
        break;
    }

    free(entry);
}

void type_entry_add_implemented(struct TypeEntry *entry, struct FunctionEntry *implemented)
{
    char *signature = sprint_function_signature(implemented);
    if (set_find(entry->implemented, implemented) != NULL)
    {
        InternalError("Type %s already implements %s!", entry->name, signature);
    }
    set_insert(entry->implemented, implemented);
    hash_table_insert(entry->implementedByName, signature, implemented);
}

void type_entry_add_trait(struct TypeEntry *entry, struct TraitEntry *trait)
{
    // TODO: sanity check that all trait functions are implemented?
    set_insert(entry->traits, trait);
}

struct TraitEntry *type_entry_lookup_trait(struct TypeEntry *typeEntry, char *name)
{
    struct TraitEntry dummyTrait = {0};
    dummyTrait.name = name;
    return set_find(typeEntry->traits, &dummyTrait);
}

struct FunctionEntry *type_entry_lookup_implemented_by_name(struct TypeEntry *typeEntry, char *name)
{
    return hash_table_find(typeEntry->implementedByName, name);
}

void type_resolve_capital_self(struct TypeEntry *theType)
{
    InternalError("type_resolve_capital_self not yet implemented");
}
