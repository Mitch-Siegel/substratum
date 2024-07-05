#ifndef SYMTAB_ENUM_H
#define SYMTAB_ENUM_H

#include "ast.h"

struct Scope;
struct Type;

struct EnumMember
{
    char *name;
    size_t numerical;
};

ssize_t enum_member_compare(void *enumMemberA, void *enumMemberB);

struct EnumEntry
{
    char *name;
    struct Scope *parentScope;
    struct Set *members;
};

void enum_entry_free(struct EnumEntry *the_enum);

struct EnumMember *enum_add_member(struct EnumEntry *the_enum,
                                 struct Ast *name);

struct EnumMember *enum_lookup_member(struct EnumEntry *the_enum,
                                    struct Ast *name);

#endif
