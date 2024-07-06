#include "symtab_enum.h"

#include "log.h"
#include "symtab_function.h"
#include "symtab_scope.h"
#include "util.h"

ssize_t enum_member_compare(void *enumMemberA, void *enumMemberB)
{
    return strcmp(((struct EnumMember *)enumMemberA)->name, ((struct EnumMember *)enumMemberB)->name);
}

void enum_entry_free(struct EnumEntry *the_enum)
{
    set_free(the_enum->members);

    free(the_enum);
}

struct EnumMember *enum_add_member(struct EnumEntry *the_enum,
                                   struct Ast *memberName,
                                   struct Type *memberType)
{
    struct EnumMember *newMember = malloc(sizeof(struct EnumMember));

    struct EnumEntry *existingEnumWithMember = scope_lookup_enum_by_member_name(the_enum->parentScope, memberName->value);
    if (existingEnumWithMember != NULL)
    {
        log_tree(LOG_FATAL, memberName, "Enum %s already has a member named %s", existingEnumWithMember->name, memberName->value);
    }

    newMember->name = memberName->value;
    newMember->numerical = the_enum->members->elements->size;
    newMember->type = *memberType;

    set_insert(the_enum->members, newMember);

    return newMember;
}

struct EnumMember *enum_lookup_member(struct EnumEntry *the_enum,
                                      struct Ast *name)
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
