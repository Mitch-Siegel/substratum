#ifndef SYMTAB_VARIABLE_H
#define SYMTAB_VARIABLE_H

#include "type.h"

#include "ast.h"
#include "symtab_scope.h"

struct VariableEntry
{
	int stackOffset;
	char *name; // duplicate pointer from ScopeMember for ease of use
	struct Type type;
	// TODO: do these 3 variables need to be deleted?
	// keeping them in could allow for more checks at linearization time
	// but do these checks make sense to do at register allocation time?
	int assignedAt;
	int declaredAt;
	char isAssigned;
	// if this variable has the address-of operator used on it or is a global variable
	// we need to denote that it *must* live in memory so it isn't lost
	// and can have an address
	char mustSpill;
	char isGlobal;
	char isExtern;
	char isStringLiteral;
};

struct VariableEntry *Scope_createVariable(struct Scope *scope,
										   struct AST *name,
										   struct Type *type,
										   u8 isGlobal,
										   size_t declaredAt,
										   u8 isArgument);

struct VariableEntry *Scope_lookupVarByString(struct Scope *scope,
											  char *name);

struct VariableEntry *Scope_lookupVar(struct Scope *scope,
									  struct AST *name);
#endif
