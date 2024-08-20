#ifndef SYMTAB_ENUM_H
#define SYMTAB_ENUM_H

#include "ast.h"
#include "type.h"

#include "mbcl/set.h"

struct Scope;

struct EnumMember
{
    char *name;
    struct Type type;
    size_t numerical;
};

ssize_t enum_member_compare(void *enumMemberA, void *enumMemberB);

struct EnumEntry
{
    char *name;
    struct Scope *parentScope;
    Set *members;
    size_t unionSize; // size of the largest type contained within the union represented by this enum
};

struct EnumEntry *enum_entry_new(char *name, struct Scope *parentScope);

void enum_entry_free(struct EnumEntry *theEnum);

struct EnumMember *enum_add_member(struct EnumEntry *theEnum,
                                   struct Ast *memberName,
                                   struct Type *memberType);

struct EnumMember *enum_lookup_member(struct EnumEntry *theEnum,
                                      struct Ast *name);

#endif
