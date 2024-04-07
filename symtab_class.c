#include "symtab_class.h"

#include "util.h"
#include "symtab_scope.h"

struct ClassEntry *createClass(struct Scope *scope,
									 char *name)
{
	struct ClassEntry *wipClass = malloc(sizeof(struct ClassEntry));
	wipClass->name = name;
	wipClass->members = Scope_new(scope, "CLASS", NULL);
	wipClass->memberLocations = Stack_New();
	wipClass->totalSize = 0;

	Scope_insert(scope, name, wipClass, e_class);
	return wipClass;
}

void assignOffsetToMemberVariable(struct ClassEntry *class,
										struct VariableEntry *variable)
{

	struct ClassMemberOffset *newMemberLocation = malloc(sizeof(struct ClassMemberOffset));

	// add the padding to the total size of the class
	class->totalSize += Scope_ComputePaddingForAlignment(class->members, &variable->type, class->totalSize);

	// place the new member at the (now aligned) current max size of the class
	newMemberLocation->offset = class->totalSize;
	newMemberLocation->variable = variable;

	// add the size of the member we just added to the total size of the class
	class->totalSize += getSizeOfType(class->members, &variable->type);

	Stack_Push(class->memberLocations, newMemberLocation);
}

struct ClassMemberOffset *lookupMemberVariable(struct ClassEntry *class,
													 struct AST *name)
{
	if (name->type != t_identifier)
	{
		ErrorWithAST(ERROR_INTERNAL,
					 name,
					 "Non-identifier tree %s (%s) passed to Class_lookupOffsetOfMemberVariable!\n",
					 name->value,
					 getTokenName(name->type));
	}

	for (size_t memberIndex = 0; memberIndex < class->memberLocations->size; memberIndex++)
	{
		struct ClassMemberOffset *member = class->memberLocations->data[memberIndex];
		if (!strcmp(member->variable->name, name->value))
		{
			return member;
		}
	}

	ErrorWithAST(ERROR_CODE, name, "Use of nonexistent member variable %s in class %s\n", name->value, class->name);
}

struct ClassEntry *lookupClass(struct Scope *scope,
									 struct AST *name)
{
	struct ScopeMember *lookedUp = Scope_lookup(scope, name->value);
	if (lookedUp == NULL)
	{
		ErrorWithAST(ERROR_CODE, name, "Use of undeclared class '%s'\n", name->value);
	}
	switch (lookedUp->type)
	{
	case e_class:
		return lookedUp->entry;

	default:
		ErrorWithAST(ERROR_INTERNAL, name, "%s is not a class!\n", name->value);
	}
}

struct ClassEntry *lookupClassByType(struct Scope *scope,
										   struct Type *type)
{
	if (type->classType.name == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Type with null classType name passed to lookupClassByType!\n");
	}

	struct ScopeMember *lookedUp = Scope_lookup(scope, type->classType.name);
	if (lookedUp == NULL)
	{
		ErrorAndExit(ERROR_CODE, "Use of undeclared class '%s'\n", type->classType.name);
	}

	switch (lookedUp->type)
	{
	case e_class:
		return lookedUp->entry;

	default:
		ErrorAndExit(ERROR_INTERNAL, "lookupClassByType for %s lookup got a non-class ScopeMember!\n", type->classType.name);
	}
}
