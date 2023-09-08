#include "symtab.h"

extern struct Dictionary *parseDict;

char *symbolNames[] = {
	"variable",
	"function"};

struct FunctionEntry *FunctionEntry_new(struct Scope *parentScope, char *name, struct Type *returnType)
{
	struct FunctionEntry *newFunction = malloc(sizeof(struct FunctionEntry));
	newFunction->arguments = Stack_New();
	newFunction->argStackSize = 0;
	newFunction->mainScope = Scope_new(parentScope, name, newFunction);
	newFunction->BasicBlockList = LinkedList_New();
	newFunction->mainScope->parentFunction = newFunction;
	newFunction->returnType = *returnType;
	newFunction->name = name;
	newFunction->isDefined = 0;
	return newFunction;
}

void FunctionEntry_free(struct FunctionEntry *f)
{
	Stack_Free(f->arguments);
	LinkedList_Free(f->BasicBlockList, NULL);
	Scope_free(f->mainScope);
	free(f);
}

struct SymbolTable *SymbolTable_new(char *name)
{
	struct SymbolTable *wip = malloc(sizeof(struct SymbolTable));
	wip->name = name;
	wip->globalScope = Scope_new(NULL, "Global", NULL);
	struct BasicBlock *globalBlock = BasicBlock_new(0);
	// char *globalBlockName = malloc(12);
	// sprintf(globalBlockName, "globalblock");
	// manually insert a basic block for global code so we can give it the custom name of "globalblock"
	Scope_insert(wip->globalScope, "globalblock", globalBlock, e_basicblock);

	return wip;
}

void SymbolTable_print(struct SymbolTable *it, char printTAC)
{
	printf("~~~~~~~~~~~~~\n");
	printf("Symbol Table For %s:\n\n", it->name);
	Scope_print(it->globalScope, 0, printTAC);
	printf("~~~~~~~~~~~~~\n\n");
}

char *SymbolTable_mangleName(struct Scope *scope, struct Dictionary *dict, char *toMangle)
{
	char *scopeName = scope->name;

	char *mangledName = malloc(strlen(toMangle) + strlen(scopeName) + 2);
	sprintf(mangledName, "%s_%s", scopeName, toMangle);
	char *newName = Dictionary_LookupOrInsert(dict, mangledName);
	free(mangledName);
	return newName;
}

void SymbolTable_moveMemberToParentScope(struct Scope *scope, struct ScopeMember *toMove, int *indexWithinCurrentScope)
{
	Scope_insert(scope->parentScope, toMove->name, toMove->entry, toMove->type);
	free(scope->entries->data[*indexWithinCurrentScope]);
	for (int j = *indexWithinCurrentScope; j < scope->entries->size - 1; j++)
	{
		scope->entries->data[j] = scope->entries->data[j + 1];
	}
	scope->entries->size--;

	// decrement this so that if we are using it as an iterator to entries in the original scope, it is still valid
	(*indexWithinCurrentScope)--;
}

void SymbolTable_collapseScopesRec(struct Scope *scope, struct Dictionary *dict, int depth)
{
	// first pass: recurse depth-first so everything we do at this call depth will be 100% correct
	for (int i = 0; i < scope->entries->size; i++)
	{
		struct ScopeMember *thisMember = scope->entries->data[i];
		switch (thisMember->type)
		{
		case e_scope: // recurse to subscopes
		{
			SymbolTable_collapseScopesRec(thisMember->entry, dict, depth + 1);
		}
		break;

		case e_function: // recurse to functions
		{
			if (depth > 0)
			{
				ErrorAndExit(ERROR_INTERNAL, "Saw function at depth > 0 when collapsing scopes!\n");
			}
			struct FunctionEntry *thisFunction = thisMember->entry;
			SymbolTable_collapseScopesRec(thisFunction->mainScope, dict, 0);
		}
		break;

		// skip everything else
		case e_variable:
		case e_argument:
		case e_basicblock:
			break;
		}
	}
	for (int i = 0; i < depth; i++)
	{
		printf("\t");
	}
	// second pass: rename basic block operands relevant to the current scope
	for (int i = 0; i < scope->entries->size; i++)
	{
		struct ScopeMember *thisMember = scope->entries->data[i];
		switch (thisMember->type)
		{
		case e_scope:
		case e_function:
			break;

		case e_basicblock:
		{
			// rename TAC lines if we are within a function
			if (scope->parentFunction != NULL)
			{
				// go through all TAC lines in this block
				struct BasicBlock *thisBlock = thisMember->entry;
				for (struct LinkedListNode *TACRunner = thisBlock->TACList->head; TACRunner != NULL; TACRunner = TACRunner->next)
				{
					struct TACLine *thisTAC = TACRunner->data;
					for (int j = 0; j < 4; j++)
					{
						// check only TAC operands that both exist and refer to a named variable from the source code (ignore temps etc)
						if ((thisTAC->operands[j].type.basicType != vt_null) &&
							((thisTAC->operands[j].permutation == vp_standard) || (thisTAC->operands[j].permutation == vp_objptr)))
						{
							char *originalName = thisTAC->operands[j].name.str;
							// if this operand refers to a variable declared at this scope
							if (Scope_contains(scope, originalName))
							{
								thisTAC->operands[j].name.str = SymbolTable_mangleName(scope, dict, originalName);
							}
						}
					}
				}
			}
		}
		break;

		case e_variable:
		case e_argument:
			break;
		}
	}

	// third pass: move nested members to parent scope based on mangled names
	// also moves globals outwards
	for (int i = 0; i < scope->entries->size; i++)
	{
		struct ScopeMember *thisMember = scope->entries->data[i];
		switch (thisMember->type)
		{
		case e_scope:
		case e_function:
			break;

		case e_basicblock:
		{
			if (depth > 0 && scope->parentScope != NULL)
			{
				SymbolTable_moveMemberToParentScope(scope, thisMember, &i);
			}
		}
		break;

		case e_variable:
		case e_argument:
		{
			if (scope->parentScope != NULL)
			{
				thisMember->name = SymbolTable_mangleName(scope, dict, thisMember->name);
				struct VariableEntry *variableToMove = thisMember->entry;
				if (variableToMove->isGlobal || depth > 0)
				{
					SymbolTable_moveMemberToParentScope(scope, thisMember, &i);
				}
			}
		}
		break;

		default:
			break;
		}
	}
}

void SymbolTable_collapseScopes(struct SymbolTable *it, struct Dictionary *dict)
{
	SymbolTable_collapseScopesRec(it->globalScope, parseDict, 0);
}

void SymbolTable_free(struct SymbolTable *it)
{
	Scope_free(it->globalScope);
	free(it);
}

/*
 * Scope functions
 *
 */
struct Scope *Scope_new(struct Scope *parentScope, char *name, struct FunctionEntry *parentFunction)
{
	struct Scope *wip = malloc(sizeof(struct Scope));
	wip->entries = Stack_New();

	// need to set this manually to handle when new functions are declared
	// TODO: supports nested functions? ;)
	wip->parentFunction = parentFunction;
	wip->parentScope = parentScope;
	wip->name = name;
	wip->subScopeCount = 0;
	return wip;
}

void Scope_free(struct Scope *scope)
{
	for (int i = 0; i < scope->entries->size; i++)
	{
		struct ScopeMember *examinedEntry = scope->entries->data[i];
		switch (examinedEntry->type)
		{
		case e_scope:
			Scope_free(examinedEntry->entry);
			break;

		case e_function:
		{
			FunctionEntry_free(examinedEntry->entry);
		}
		break;

		case e_variable:
		case e_argument:
		{
			struct VariableEntry *theVariable = examinedEntry->entry;
			struct Type *variableType = &theVariable->type;
			if (variableType->initializeTo != NULL)
			{
				if (variableType->arraySize > 0)
				{
					for (int i = 0; i < variableType->arraySize; i++)
					{
						free(variableType->initializeArrayTo[i]);
					}
					free(variableType->initializeArrayTo);
				}
				else
				{
					free(variableType->initializeTo);
				}
			}
			free(theVariable);
		}
		break;

		case e_basicblock:
			BasicBlock_free(examinedEntry->entry);
			break;
		}

		free(examinedEntry);
	}
	Stack_Free(scope->entries);
	free(scope);
}

// insert a member with a given name and pointer to entry, along with info about the entry type
void Scope_insert(struct Scope *scope, char *name, void *newEntry, enum ScopeMemberType type)
{
	if (Scope_contains(scope, name))
	{
		ErrorAndExit(ERROR_INTERNAL, "Error defining symbol [%s] - name already exists!\n", name);
	}
	struct ScopeMember *wip = malloc(sizeof(struct ScopeMember));
	wip->name = name;
	wip->entry = newEntry;
	wip->type = type;
	Stack_Push(scope->entries, wip);
}

// create a variable within the given scope
struct VariableEntry *Scope_createVariable(struct Scope *scope,
										   struct AST *name,
										   struct Type *type,
										   char isGlobal,
										   int declaredAt,
										   char isArgument)
{
	struct VariableEntry *newVariable = malloc(sizeof(struct VariableEntry));
	newVariable->type = *type;
	newVariable->stackOffset = 0;
	newVariable->assignedAt = -1;
	newVariable->declaredAt = declaredAt;
	newVariable->isAssigned = 0;
	newVariable->name = name->value;

	newVariable->type.initializeTo = NULL;

	if (isGlobal)
	{
		newVariable->isGlobal = 1;
	}
	else
	{
		newVariable->isGlobal = 0;
	}

	if (Scope_contains(scope, name->value))
	{
		ErrorWithAST(ERROR_CODE, name, "Redifinition of symbol %s!\n", name->value);
	}

	if (isArgument)
	{
		// if we have an argument, obvoiulsy it will be spilled because it comes in on the stack
		newVariable->stackOffset = scope->parentFunction->argStackSize + 8;
		scope->parentFunction->argStackSize += Scope_getSizeOfType(scope, type);
		Scope_insert(scope, name->value, newVariable, e_argument);
	}
	else
	{
		Scope_insert(scope, name->value, newVariable, e_variable);
	}

	return newVariable;
}

// create a new function accessible within the given scope
struct FunctionEntry *Scope_createFunction(struct Scope *parentScope, char *name, struct Type *returnType)
{
	struct FunctionEntry *newFunction = FunctionEntry_new(parentScope, name, returnType);
	Scope_insert(parentScope, name, newFunction, e_function);
	return newFunction;
}

// create and return a child scope of the scope provided as an argument
struct Scope *Scope_createSubScope(struct Scope *parentScope)
{
	if (parentScope->subScopeCount == 0xff)
	{
		ErrorAndExit(ERROR_INTERNAL, "Too many subscopes of scope %s\n", parentScope->name);
	}
	char *helpStr = malloc(2 + strlen(parentScope->name) + 1);
	sprintf(helpStr, "%02x", parentScope->subScopeCount);
	char *newScopeName = Dictionary_LookupOrInsert(parseDict, helpStr);
	free(helpStr);
	parentScope->subScopeCount++;

	struct Scope *newScope = Scope_new(parentScope, newScopeName, parentScope->parentFunction);
	newScope->parentFunction = parentScope->parentFunction;

	Scope_insert(parentScope, newScopeName, newScope, e_scope);
	return newScope;
}

// Scope lookup functions

char Scope_contains(struct Scope *scope, char *name)
{
	for (int i = 0; i < scope->entries->size; i++)
	{
		if (!strcmp(name, ((struct ScopeMember *)scope->entries->data[i])->name))
		{
			return 1;
		}
	}
	return 0;
}

// if a member with the given name exists in this scope or any of its parents, return it
// also looks up entries from deeper scopes, but only as their mangled names specify
struct ScopeMember *Scope_lookup(struct Scope *scope, char *name)
{
	while (scope != NULL)
	{
		for (int i = 0; i < scope->entries->size; i++)
		{
			struct ScopeMember *examinedEntry = scope->entries->data[i];
			if (!strcmp(examinedEntry->name, name))
			{
				return examinedEntry;
			}
		}
		scope = scope->parentScope;
	}
	return NULL;
}

struct VariableEntry *Scope_lookupVarByString(struct Scope *scope, char *name)
{
	struct ScopeMember *lookedUp = Scope_lookup(scope, name);
	if (lookedUp == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Lookup of variable [%s] by string name failed!\n", name);
	}

	switch (lookedUp->type)
	{
	case e_argument:
	case e_variable:
		return lookedUp->entry;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Lookup returned unexpected symbol table entry type when looking up variable [%s]!\n", name);
	}
}

struct VariableEntry *Scope_lookupVar(struct Scope *scope, struct AST *name)
{
	struct ScopeMember *lookedUp = Scope_lookup(scope, name->value);
	if (lookedUp == NULL)
	{
		ErrorWithAST(ERROR_CODE, name, "Use of undeclared variable '%s'\n", name->value);
	}

	switch (lookedUp->type)
	{
	case e_argument:
	case e_variable:
		return lookedUp->entry;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Lookup returned unexpected symbol table entry type when looking up variable [%s]!\n", name->value);
	}
}

struct FunctionEntry *Scope_lookupFun(struct Scope *scope, struct AST *name)
{
	struct ScopeMember *lookedUp = Scope_lookup(scope, name->value);
	if (lookedUp == NULL)
	{
		ErrorWithAST(ERROR_CODE, name, "Use of undeclared function '%s'\n", name->value);
	}
	switch (lookedUp->type)
	{
	case e_function:
		return lookedUp->entry;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Lookup returned unexpected symbol table entry type when looking up function!\n");
	}
}

struct Scope *Scope_lookupSubScope(struct Scope *scope, char *name)
{
	struct ScopeMember *lookedUp = Scope_lookup(scope, name);
	if (lookedUp == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Failure looking up scope with name [%s]\n", name);
	}

	switch (lookedUp->type)
	{
	case e_scope:
		return lookedUp->entry;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Unexpected symbol table entry type found when attempting to look up scope [%s]\n", name);
	}
}

struct Scope *Scope_lookupSubScopeByNumber(struct Scope *scope, unsigned char subScopeNumber)
{
	char subScopeName[3];
	sprintf(subScopeName, "%02x", subScopeNumber);
	struct Scope *lookedUp = Scope_lookupSubScope(scope, subScopeName);
	return lookedUp;
}

int Scope_getSizeOfType(struct Scope *scope, struct Type *t)
{
	int size = 0;

	switch (t->basicType)
	{
	case vt_null:
		ErrorAndExit(ERROR_INTERNAL, "Scope_getSizeOfType called with basic type of vt_null!\n");
		break;

	case vt_uint8:
		size = 1;
		break;

	case vt_uint16:
		size = 2;
		break;

	case vt_uint32:
		size = 4;
		break;

	case vt_class:
		ErrorAndExit(ERROR_INTERNAL, "Scope_getSizeOfType called with basic type of vt_class - not supported yet!\n");
	}

	if (t->arraySize > 0)
	{
		if (t->indirectionLevel > 1)
		{
			size = 4;
		}

		size *= t->arraySize;
	}
	else
	{
		if (t->indirectionLevel > 0)
		{
			size = 4;
		}
	}

	return size;
}

int Scope_getSizeOfDereferencedType(struct Scope *scope, struct Type *t)
{
	struct Type dereferenced = *t;
	dereferenced.indirectionLevel--;
	return Scope_getSizeOfType(scope, &dereferenced);
}

int Scope_getSizeOfVariable(struct Scope *scope, struct VariableEntry *v)
{
	return Scope_getSizeOfType(scope, &v->type);
}

int Scope_getSizeOfArrayElement(struct Scope *scope, struct VariableEntry *v)
{
	if (v->type.indirectionLevel < 1)
	{
		ErrorAndExit(ERROR_INTERNAL, "Non-indirect variable %s passed to Scope_getSizeOfArrayElement!\n", v->name);
	}
	else if (v->type.indirectionLevel == 1)
	{
		struct Type elementType = v->type;
		elementType.indirectionLevel--;
		return Scope_getSizeOfType(scope, &elementType);
	}
	else
	{
		return 4;
	}
}

// allocate and return a string containing the name and pointer level of a type
char *Scope_getNameOfType(struct Scope *scope, struct Type *t)
{
	char *name;
	switch (t->basicType)
	{
	case vt_uint8:
		name = "uint8";
		break;

	case vt_uint16:
		name = "uint16";
		break;

	case vt_uint32:
		name = "uint32";
		break;

	default:
		name = t->classType.name;
		ErrorAndExit(ERROR_INTERNAL, "Unexepcted variable type %s in Scope_getNameOfType!\n", t->classType.name);
	}

	int nameLen = strlen(name);
	int totalLen = nameLen + t->indirectionLevel + 1;
	char *fullName = malloc(totalLen);
	strcpy(fullName, name);
	for (int i = nameLen; i < totalLen; i++)
	{
		fullName[i] = '*';
	}
	fullName[totalLen - 1] = '\0';
	return fullName;
}

void VariableEntry_Print(struct VariableEntry *it, int depth)
{
	for (int j = 0; j < depth; j++)
	{
		printf("\t");
	}
	char *typeName = Type_GetName(&it->type);
	printf("  - %s", typeName);
	free(typeName);
	printf("\n");
}

void Scope_print(struct Scope *it, int depth, char printTAC)
{
	for (int i = 0; i < it->entries->size; i++)
	{
		struct ScopeMember *thisMember = it->entries->data[i];

		if (thisMember->type != e_basicblock || printTAC)
		{
			for (int j = 0; j < depth; j++)
			{
				printf("\t");
			}
		}

		switch (thisMember->type)
		{
		case e_argument:
		{
			struct VariableEntry *theArgument = thisMember->entry;
			printf("> Argument %s:", thisMember->name);
			printf("\n");
			VariableEntry_Print(theArgument, depth);
			for (int j = 0; j < depth; j++)
			{
				printf("\t");
			}
			printf("  - Stack offset: %d\n\n", theArgument->stackOffset);
		}
		break;

		case e_variable:
		{
			struct VariableEntry *theVariable = thisMember->entry;
			printf("> Variable %s:", thisMember->name);
			printf("\n");
			VariableEntry_Print(theVariable, depth);
			printf("\n");
		}
		break;

		case e_function:
		{
			struct FunctionEntry *theFunction = thisMember->entry;
			printf("> Function %s (returns %d) (defined: %d)\n\t%d bytes of arguments on stack\n", thisMember->name, theFunction->returnType.basicType, theFunction->isDefined, theFunction->argStackSize);
			// if (printTAC)
			// {
			// for (struct LinkedListNode *b = theFunction->BasicBlockList->head; b != NULL; b = b->next)
			// {
			// printBasicBlock(b->data, depth + 1);
			// }
			// }
			Scope_print(theFunction->mainScope, depth + 1, printTAC);
		}
		break;

		case e_scope:
		{
			struct Scope *theScope = thisMember->entry;
			printf("> Subscope %s\n", thisMember->name);
			Scope_print(theScope, depth + 1, printTAC);
		}
		break;

		case e_basicblock:
		{
			if (printTAC)
			{
				struct BasicBlock *thisBlock = thisMember->entry;
				printf("> Basic Block %d\n", thisBlock->labelNum);
				printBasicBlock(thisBlock, depth + 1);
			}
		}
		break;
		}
	}
}

void Scope_addBasicBlock(struct Scope *scope, struct BasicBlock *b)
{
	char *blockName = malloc(10);
	sprintf(blockName, "Block%d", b->labelNum);
	Scope_insert(scope, Dictionary_LookupOrInsert(parseDict, blockName), b, e_basicblock);
	free(blockName);

	if (scope->parentFunction != NULL)
	{
		LinkedList_Append(scope->parentFunction->BasicBlockList, b);
	}
}

/*
 * AST walk and symbol table generation functions
 */

// scrape down a chain of nested child star tokens, expecting something at the bottom
int scrapePointers(struct AST *pointerAST, struct AST **resultDestination)
{
	int dereferenceDepth = 0;

	while (pointerAST != NULL && pointerAST->type == t_star)
	{
		dereferenceDepth++;
		pointerAST = pointerAST->sibling;
	}
	*resultDestination = pointerAST;
	return dereferenceDepth;
}
