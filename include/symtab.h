#include <stdio.h>
#include <string.h>

#include "util.h"
#include "ast.h"
#include "tac.h"
#include "parser.h"
#include "tac.h"

#pragma once

enum ScopeMemberType
{
	e_variable,
	e_function,
	e_argument,
	e_class,
	e_scope,
	e_basicblock,
};

struct ScopeMember
{
	char *name;
	enum ScopeMemberType type;
	void *entry;
};

struct FunctionEntry;

struct Scope
{
	struct Scope *parentScope;
	struct FunctionEntry *parentFunction;
	struct Stack *entries;
	unsigned char subScopeCount;
	char *name; // duplicate pointer from ScopeMember for ease of use
};

struct FunctionEntry
{
	int argStackSize;
	struct Type returnType;
	struct Scope *mainScope;
	struct Stack *arguments; // stack of VariableEntry pointers corresponding by index to arguments
	char *name;				 // duplicate pointer from ScopeMember for ease of use
	struct LinkedList *BasicBlockList;
	struct AST correspondingTree;
	char isDefined;
	char isAsmFun;
	char callsOtherFunction; // is it possible this function calls another function? (need to store return address on stack)
};

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

struct ClassMemberOffset
{
	struct VariableEntry *variable;
	int offset;
};

struct ClassEntry
{
	char *name;
	struct Scope *members;
	struct Stack *memberLocations;
	int totalSize;
};

struct SymbolTable
{
	char *name;
	struct Scope *globalScope;
};
/*
 * the create/lookup functions that use an AST (with simpler names) are the primary functions which should be used
 * these provide higher verbosity to error messages (source line/col number associated with errors)
 * the lookup functions with ByString name suffixes should be used only when manipulating pre-existing TAC
 * in this case,
only string names are available and any bad lookups should be caused by internal error cases
 */

// create an argument engty in the provided function entry,which is named by the provided AST node
struct FunctionEntry *FunctionEntry_new(struct Scope *parentScope,
										struct AST *nameTree,
										struct Type *returnType);

struct VariableEntry *FunctionEntry_createArgument(struct FunctionEntry *func,
												   struct AST *name,
												   enum basicTypes type,
												   int arraySize);

void FunctionEntry_free(struct FunctionEntry *f);

// symbol table functions
struct SymbolTable *SymbolTable_new(char *name);

// scope functions
struct Scope *Scope_new(struct Scope *parentScope,
						char *name,
						struct FunctionEntry *parentFunction);

void Scope_free(struct Scope *scope);

void Scope_print(struct Scope *it,
				 int depth,
				 char printTAC);

void Scope_insert(struct Scope *scope,
				  char *name,
				  void *newEntry,
				  enum ScopeMemberType type);

struct VariableEntry *Scope_createVariable(struct Scope *scope,
										   struct AST *name,
										   struct Type *type,
										   char isGlobal,
										   int declaredAt,
										   char isArgument);

struct FunctionEntry *Scope_createFunction(struct Scope *parentScope,
										   struct AST *nameTree,
										   struct Type *returnType);

struct Scope *Scope_createSubScope(struct Scope *scope);

// this represents the definition of a class itself, instantiation falls under variableEntry
struct ClassEntry *Scope_createClass(struct Scope *scope,
									 char *name);

// given a scope, a type, and a current integer byte offset
/// compute and return how many bytes of padding is necessary to create the first offset at which the type would be aligned if stored
int Scope_ComputePaddingForAlignment(struct Scope *scope, struct Type *alignedType, int currentOffset);

// given a VariableEntry corresponding to a class member which was just declared
// generate a ClassMemberOffset with the aligned location of the member within the class
void Class_assignOffsetToMemberVariable(struct ClassEntry *class,
										struct VariableEntry *v);

struct ClassMemberOffset *Class_lookupMemberVariable(struct ClassEntry *class,
													 struct AST *name);

// scope lookup functions
char Scope_contains(struct Scope *scope,
					char *name);

struct ScopeMember *Scope_lookup(struct Scope *scope,
								 char *name);

struct VariableEntry *Scope_lookupVarByString(struct Scope *scope,
											  char *name);

struct VariableEntry *Scope_lookupVar(struct Scope *scope,
									  struct AST *name);

struct FunctionEntry *Scope_lookupFunByString(struct Scope *scope,
											  char *name);

struct FunctionEntry *Scope_lookupFun(struct Scope *scope,
									  struct AST *name);

struct ClassEntry *Scope_lookupClass(struct Scope *scope,
									 struct AST *name);

struct ClassEntry *Scope_lookupClassByType(struct Scope *scope,
										   struct Type *type);

// gets the integer size (not aligned) of a given type
int Scope_getSizeOfType(struct Scope *scope, struct Type *t);

// gets the integer size (not aligned) of a given type, but based on the dereference level as (t->indirectionLevel - 1)
int Scope_getSizeOfDereferencedType(struct Scope *scope, struct Type *t);

int Scope_getSizeOfArrayElement(struct Scope *scope, struct VariableEntry *v);

// calculate the power of 2 to which a given type needs to be aligned
int Scope_getAlignmentOfType(struct Scope *scope, struct Type *t);

// scope linearization functions

// adds an entry in the given scope denoting that the block is from that scope
void Scope_addBasicBlock(struct Scope *scope,
						 struct BasicBlock *b);

void SymbolTable_print(struct SymbolTable *it,
					   char printTAC);

void SymbolTable_collapseScopesRec(struct Scope *scope,
								   struct Dictionary *dict,
								   int depth);

void SymbolTable_collapseScopes(struct SymbolTable *it,
								struct Dictionary *dict);

void SymbolTable_free(struct SymbolTable *it);

// AST walk functions

// scrape down a chain of nested child star tokens, expecting something at the bottom
int scrapePointers(struct AST *pointerAST,
				   struct AST **resultDestination);
