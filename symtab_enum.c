#include "symtab_enum.h"

#include "log.h"
#include "symtab_function.h"
#include "symtab_scope.h"
#include "util.h"

ssize_t enum_member_compare(void *enumMemberA, void *enumMemberB)
{
    return strcmp(((struct EnumMember *)enumMemberA)->name, ((struct EnumMember *)enumMemberB)->name);
}

struct EnumEntry *create_enum(struct Scope *scope,
                              char *name)
{
    struct EnumEntry *wipEnum = malloc(sizeof(struct EnumEntry));
    wipEnum->name = name;
    wipEnum->parentScope = scope;
    wipEnum->members = set_new(enum_member_compare, free);

    scope_insert(scope, name, wipEnum, E_ENUM, A_PUBLIC);
    return wipEnum;
}

void enum_entry_free(struct EnumEntry *the_enum)
{
    set_free(the_enum->members);

    free(the_enum);
}

struct EnumMember *enum_add_member(struct EnumEntry *the_enum,
                                   struct AST *name)
{
    struct EnumMember *newMember = malloc(sizeof(struct EnumMember));

    struct EnumEntry *existingEnumWithMember = scope_lookup_enum_by_member_name(the_enum->parentScope, name->value);
    if (existingEnumWithMember != NULL)
    {
        log_tree(LOG_FATAL, name, "Enum %s already has a member named %s", existingEnumWithMember->name, name->value);
    }

    newMember->name = name->value;
    newMember->numerical = the_enum->members->elements->size;
    set_insert(the_enum->members, newMember);

    return newMember;
}

struct EnumMember *enum_lookup_member(struct EnumEntry *the_enum,
                                      struct AST *name)
{
    struct EnumMember dummyMember = {0};
    dummyMember.name = name->value;
    struct EnumMember *lookedUp = set_find(the_enum->members, &dummyMember);

    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, name, "Use of undeclared member %s in enum %s", name->value, the_enum->name);
    }

    return lookedUp;
}
