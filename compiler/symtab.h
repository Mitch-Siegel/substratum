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
	e_scope,
	e_basicblock,
	e_stackobj,
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
	int localStackSize;
	int argStackSize;
	enum variableTypes returnType;
	// struct SymbolTable *table;
	struct Scope *mainScope;
	char *name; // duplicate pointer from ScopeMember for ease of use
	struct LinkedList *BasicBlockList;
};

struct VariableEntry
{
	int stackOffset;
	struct StackObjectEntry *localPointerTo;
	char *name; // duplicate pointer from ScopeMember for ease of use
	enum variableTypes type;
	int indirectionLevel;
	int assignedAt;
	int declaredAt;
	char isAssigned;
	// if this variable has the address-of operator used on it
	// we need to denote that it *must* live on the stack so it isn't lost
	// and can have an address
	char mustSpill;
};

struct StackObjectEntry
{
	int size;
	int arraySize;
	int stackOffset;
	struct VariableEntry *localPointer;
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
 * in this case, only string names are available and any bad lookups should be caused by internal error cases
 */

// create an argument engty in the provided function entry, which is named by the provided AST node
void FunctionEntry_createArgument(struct FunctionEntry *func, struct AST *name, enum variableTypes type, int indirectionLevel, int arraySize);

// symbol table functions
struct SymbolTable *SymbolTable_new(char *name);

// scope functions
struct Scope *Scope_new(struct Scope *parentScope, char *name);

void Scope_free(struct Scope *scope, int depth);

void Scope_print(struct Scope *it, int depth, char printTAC);

void Scope_insert(struct Scope *scope, char *name, void *newEntry, enum ScopeMemberType type);

void Scope_createVariable(struct Scope *scope, struct AST *name, enum variableTypes type, int indirectionLevel, int arraySize);

struct FunctionEntry *Scope_createFunction(struct Scope *scope, char *name);

struct Scope *Scope_createSubScope(struct Scope *scope);

// scope lookup functions
char Scope_contains(struct Scope *scope, char *name);

struct ScopeMember *Scope_lookup(struct Scope *scope, char *name);

struct VariableEntry *Scope_lookupVarByString(struct Scope *scope, char *name);

struct VariableEntry *Scope_lookupVar(struct Scope *scope, struct AST *name);

struct FunctionEntry *Scope_lookupFun(struct Scope *scope, struct AST *name);

struct Scope *Scope_lookupSubScope(struct Scope *scope, char *name);

struct Scope *Scope_lookupSubScopeByNumber(struct Scope *scope, unsigned char subScopeNumber);

int GetSizeOfPrimitive(enum variableTypes type);

int Scope_getSizeOfVariableByString(struct Scope *scope, char *name);

int Scope_getSizeOfVariable(struct Scope *scope, struct AST *name);

// scope linearization functions

// adds an entry in the given scope denoting that the block is from that scope
void Scope_addBasicBlock(struct Scope *scope, struct BasicBlock *b);

// add the basic block to the linkedlist for the parent function
void Function_addBasicBlock(struct FunctionEntry *function, struct BasicBlock *b);

void SymbolTable_print(struct SymbolTable *it, char printTAC);

void SymbolTable_free(struct SymbolTable *it);

// AST walk functions
void walkStatement(struct AST *it, struct Scope *wip);

void walkScope(struct AST *it, struct Scope *wip, char isMainScope);

void walkFunction(struct AST *it, struct Scope *parentScope);

struct SymbolTable *walkAST(struct AST *it);
