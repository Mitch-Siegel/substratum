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

struct EnumMember *add_enum_member(struct EnumEntry *the_enum,
                                 struct AST *name)
{
    struct EnumMember *newMember = malloc(sizeof(struct EnumMember));

    struct EnumEntry *existingEnumWithMember = lookup_enum_by_member_name(the_enum->parentScope, name->value);
    if (existingEnumWithMember != NULL)
    {
        log_tree(LOG_FATAL, name, "Enum %s already has a member named %s", existingEnumWithMember->name, name->value);
    }

    newMember->name = name->value;
    newMember->numerical = the_enum->members->elements->size;
    set_insert(the_enum->members, newMember);

    return newMember;
}

struct EnumMember *lookup_enum_member(struct EnumEntry *the_enum,
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

struct EnumEntry *lookup_enum(struct Scope *scope,
                             struct AST *name)
{
    struct ScopeMember *lookedUp = scope_lookup(scope, name->value);
    if (lookedUp == NULL)
    {
        log_tree(LOG_FATAL, name, "Use of undeclared enum '%s'", name->value);
    }
    switch (lookedUp->type)
    {
    case E_ENUM:
        return lookedUp->entry;

    default:
        log_tree(LOG_FATAL, name, "%s is not an enum!", name->value);
    }

    return NULL;
}

struct EnumEntry *lookup_enum_by_type(struct Scope *scope,
                                   struct Type *type)
{
    if (type->basicType != VT_ENUM || type->nonArray.complexType.name == NULL)
    {
        InternalError("Non-enum type or enum type with null name passed to lookupEnumByType!");
    }

    struct ScopeMember *lookedUp = scope_lookup(scope, type->nonArray.complexType.name);
    if (lookedUp == NULL)
    {
        log(LOG_FATAL, "Use of undeclared enum '%s'", type->nonArray.complexType.name);
    }

    switch (lookedUp->type)
    {
    case E_ENUM:
        return lookedUp->entry;

    default:
        InternalError("lookupEnumByType for %s lookup got a non-struct ScopeMember!", type->nonArray.complexType.name);
    }
}

struct EnumEntry *lookup_enum_by_member_name(struct Scope *scope,
                                         char *name)
{
    struct EnumMember dummyMember = {0};
    dummyMember.name = name;

    while (scope != NULL)
    {
        for (size_t memberIndex = 0; memberIndex < scope->entries->size; memberIndex++)
        {
            struct ScopeMember *member = (struct ScopeMember *)scope->entries->data[memberIndex];
            if (member->type == E_ENUM)
            {
                struct EnumEntry *scannedEnum = member->entry;
                if (set_find(scannedEnum->members, &dummyMember) != NULL)
                {
                    return scannedEnum;
                }
            }
        }
        scope = scope->parentScope;
    }

    return NULL;
}
