#ifndef SYMTAB_ENUM_H
#define SYMTAB_ENUM_H

#include "ast.h"

struct Scope;
struct Type;

struct EnumMember
{
    char *name;
    ssize_t numerical;
};

struct EnumEntry
{
    char *name;
    struct Scope *parentScope;
    struct Set *members;
};

struct EnumEntry *createEnum(struct Scope *scope,
                             char *name);

void EnumEntry_free(struct EnumEntry *theEnum);

struct EnumMember *addEnumMember(struct EnumEntry *theEnum,
                                 struct AST *name);

struct EnumMember *lookupEnumMember(struct EnumEntry *theEnum,
                                    struct AST *name);

struct EnumEntry *lookupEnum(struct Scope *scope,
                             struct AST *name);

struct EnumEntry *lookupEnumByType(struct Scope *scope,
                                   struct Type *type);

struct EnumEntry *lookupEnumByMemberName(struct Scope *scope,
                                         char *name);

#endif
