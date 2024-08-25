#ifndef ENUM_DESC_H
#define ENUM_DESC_H

#include "ast.h"
#include "type.h"

#include "mbcl/hash_table.h"
#include "mbcl/set.h"

struct Scope;

struct EnumMember
{
    char *name;
    struct Type type;
    size_t numerical;
};

ssize_t enum_member_compare(void *enumMemberA, void *enumMemberB);

struct EnumDesc
{
    char *name;
    struct Scope *parentScope;
    Set *members;
    size_t unionSize; // size of the largest type contained within the union represented by this enum
};

struct EnumDesc *enum_desc_new(char *name, struct Scope *parentScope);

void enum_desc_free(struct EnumDesc *theEnum);

struct EnumMember *enum_add_member(struct EnumDesc *theEnum,
                                   struct Ast *memberName,
                                   struct Type *memberType);

struct EnumMember *enum_lookup_member(struct EnumDesc *theEnum,
                                      struct Ast *name);

struct EnumDesc *enum_desc_clone(struct EnumDesc *toClone, char *name);

// TODO: rename all these "depth" arguments to "indent"?
void enum_desc_print(struct EnumDesc *theEnum, size_t depth, FILE *outFile);

void enum_desc_resolve_generics(struct EnumDesc *theEnum, HashTable *paramsMap, char *name, List *params);

#endif
