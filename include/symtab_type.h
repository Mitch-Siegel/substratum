#ifndef SYMTAB_TYPE_H
#define SYMTAB_TYPE_H

#include "mbcl/hash_table.h"
#include "mbcl/set.h"
#include "symtab_struct.h"
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
        struct StructEntry *asStruct;
        struct EnumEntry *asEnum;
    } data;
    Set *traits;
    Set *implemented;
    HashTable *implementedByName;
};

struct TypeEntry *type_entry_new_primitive(enum BASIC_TYPES basicType);

struct TypeEntry *type_entry_new_struct(char *name, struct Scope *parentScope, enum STRUCT_GENERIC_TYPE genericType, List *genericParamNames);

struct TypeEntry *type_entry_new_enum(char *name, struct Scope *parentScope);

void type_entry_free(struct TypeEntry *entry);

void type_entry_add_implemented(struct TypeEntry *entry, struct FunctionEntry *implemented);

struct TraitEntry *type_entry_lookup_trait(struct TypeEntry *typeEntry, char *name);

struct FunctionEntry *type_entry_lookup_implemented_by_name(struct TypeEntry *typeEntry, char *name);

void type_entry_resolve_capital_self(struct TypeEntry *theType);

#endif
