#include "symtab.h"

extern struct Dictionary *parseDict;

char *symbolNames[] = {
	"variable",
	"function"};

struct FunctionEntry *FunctionEntry_new(struct Scope *parentScope, char *name, enum variableTypes returnType, int returnIndirectionLevel)
{
	struct FunctionEntry *newFunction = malloc(sizeof(struct FunctionEntry));
	newFunction->arguments = Stack_New();
	newFunction->argStackSize = 0;
	newFunction->localStackSize = 0;
	newFunction->mainScope = Scope_new(parentScope, name, newFunction);
	newFunction->BasicBlockList = LinkedList_New();
	newFunction->mainScope->parentFunction = newFunction;
	newFunction->returnType = returnType;
	newFunction->returnIndirectionLevel = returnIndirectionLevel;
	newFunction->name = name;
	newFunction->isDefined = 0;
	return newFunction;
}

/*
// create a variable denoted to be an argument within the given function entry
struct VariableEntry *FunctionEntry_createArgument(struct FunctionEntry *func, struct AST *name, enum variableTypes type, int indirectionLevel, int arraySize)
{
	struct VariableEntry *newArgument = malloc(sizeof(struct VariableEntry));
	newArgument->type = type;
	newArgument->indirectionLevel = indirectionLevel;
	newArgument->assignedAt = -1;
	newArgument->declaredAt = -1;
	newArgument->isAssigned = 0;
	newArgument->mustSpill = 0;
	newArgument->isGlobal = 0; // arguments are never globals (pass by value)
	newArgument->name = name->value;

	Stack_Push(func->arguments, newArgument); // keep track of all arguments in order
	Scope_insert(func->mainScope, name->value, newArgument, e_argument);

	int argSize = Scope_getSizeOfVariableByAst(func->mainScope, name);
	newArgument->stackOffset = func->argStackSize + 8;
	func->argStackSize += argSize;

	if (arraySize > 1)
	{
		char *modName = malloc(strlen(name->value) + 5);
		sprintf(modName, "%s.obj", name->value);

		int calculatedStackOffset = 0;
		// since indexing pushes address forward, set the stack offset of the variable to be index 0 (the - size * arraySize) term does this
		calculatedStackOffset = (func->localStackSize * -1) - (argSize * arraySize);
		func->localStackSize += argSize * arraySize;

		struct ObjectEntry *objForArg = Scope_createObject(func->mainScope, Dictionary_LookupOrInsert(parseDict, modName), newArgument, argSize, arraySize, calculatedStackOffset, 0);
		newArgument->localPointerTo = objForArg;
		free(modName);
	}
	else
	{
		newArgument->localPointerTo = NULL;
	}

	return newArgument;
}*/

void FunctionEntry_free(struct FunctionEntry *f)
{
	Stack_Free(f->arguments);
	LinkedList_Free(f->BasicBlockList, NULL);
	Scope_free(f->mainScope);
	free(f);
}

void ObjectEntry_free(struct ObjectEntry *o)
{
	if (o->initialized)
	{
		free(o->initializeTo);
	}
	free(o);
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
		case e_object:
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
						if (thisTAC->operands[j].type != vt_null && (thisTAC->operands[j].permutation == vp_standard || thisTAC->operands[j].permutation == vp_objptr))
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
		case e_object:
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

		case e_object:
		{
			if (scope->parentScope != NULL)
			{
				thisMember->name = SymbolTable_mangleName(scope, dict, thisMember->name);
				struct ObjectEntry *objectToMove = thisMember->entry;
				if (objectToMove->isGlobal || depth > 0)
				{
					SymbolTable_moveMemberToParentScope(scope, thisMember, &i);
				}
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
			free(theVariable);
		}
		break;

		case e_basicblock:
			BasicBlock_free(examinedEntry->entry);
			break;

		case e_object:
			ObjectEntry_free(examinedEntry->entry);
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
										   enum variableTypes type,
										   int indirectionLevel,
										   int arraySize,
										   char isGlobal,
										   int declaredAt,
										   char isArgument)
{
	struct VariableEntry *newVariable = malloc(sizeof(struct VariableEntry));
	newVariable->type = type;
	newVariable->indirectionLevel = indirectionLevel;
	newVariable->arraySize = arraySize;
	newVariable->stackOffset = 0;
	newVariable->assignedAt = -1;
	newVariable->declaredAt = declaredAt;
	newVariable->isAssigned = 0;
	newVariable->name = name->value;

	if (isGlobal)
	{
		newVariable->isGlobal = 1;
		newVariable->mustSpill = 1;
	}
	else
	{
		newVariable->isGlobal = 0;
		if (arraySize > 1)
		{
			newVariable->mustSpill = 1;
		}
		else
		{
			newVariable->mustSpill = 0;
		}
	}

	if (Scope_contains(scope, name->value))
	{
		ErrorWithAST(ERROR_CODE, name, "Redifinition of symbol %s!\n", name->value);
	}

	if (arraySize > 1)
	{
		newVariable->indirectionLevel++;
	}

	int totalVariableFootprint = GetSizeOfPrimitive(newVariable->type) * arraySize;

	if (isArgument)
	{
		// if we have an argument, obvoiulsy it will be spilled because it comes in on the stack
		newVariable->mustSpill = 1;
		newVariable->stackOffset = scope->parentFunction->argStackSize + 8;
		scope->parentFunction->argStackSize += totalVariableFootprint;
		Scope_insert(scope, name->value, newVariable, e_argument);
	}
	else
	{
		if ((scope->parentFunction != NULL) && newVariable->mustSpill)
		{

			scope->parentFunction->localStackSize -= totalVariableFootprint;
			newVariable->stackOffset = scope->parentFunction->localStackSize;
		}
		Scope_insert(scope, name->value, newVariable, e_variable);
	}

	/*
	int varSize = Scope_getSizeOfVariableByAst(scope, name);

	if (arraySize > 1)
	{
		char *modName = malloc(strlen(name->value) + 5);
		sprintf(modName, "%s.obj", name->value);

		int calculatedStackOffset;
		if (!isGlobal)
		{
			// since indexing pushes address forward, set the stack offset of the variable to be index 0 (the - size * arraySize) term does this
			calculatedStackOffset = (scope->parentFunction->localStackSize * -1) - (varSize * arraySize);
			scope->parentFunction->localStackSize += varSize * arraySize;
		}
		else
		{
			calculatedStackOffset = 0;
		}

		struct ObjectEntry *objForVar = Scope_createObject(scope, Dictionary_LookupOrInsert(parseDict, modName), newVariable, varSize, arraySize, calculatedStackOffset, isGlobal);
		newVariable->localPointerTo = objForVar;
		newVariable->indirectionLevel++;
		free(modName);
	}
	else
	{
		newVariable->localPointerTo = NULL;
	}*/

	return newVariable;
}

// create a new function accessible within the given scope
struct FunctionEntry *Scope_createFunction(struct Scope *parentScope, char *name, enum variableTypes returnType, int returnIndirectionLevel)
{
	struct FunctionEntry *newFunction = FunctionEntry_new(parentScope, name, returnType, returnIndirectionLevel);
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

/*
 * this function may modify the `char *name` it is passed if:
 *	- the name contains spaces (is a string literal)
 *		- this has ramifications on the initialization of the object (value in the char * needs to be copied before calling this!)
 */
struct ObjectEntry *Scope_createObject(struct Scope *scope, char *name, struct VariableEntry *myLocalPointer, int size, int arraySize, int stackOffset, char isGlobal)
{
	struct ObjectEntry *newObject = malloc(sizeof(struct ObjectEntry));

	newObject->myLocalPointer = myLocalPointer;
	newObject->size = size;
	newObject->arraySize = arraySize;
	newObject->stackOffset = stackOffset;
	newObject->isGlobal = isGlobal;
	newObject->initialized = 0;

	Scope_insert(scope, name, newObject, e_object);
	return newObject;
}

struct ObjectEntry *Scope_createStringLiteral(struct Scope *scope, char *name, struct VariableEntry *myLocalPointer)
{
	int nameLen = strlen(name);
	for (int i = 0; i < nameLen; i++)
	{
		if ((!isalnum(name[i])) && (name[i] != '_'))
		{
			if (isspace(name[i]))
			{
				name[i] = '_';
			}
			else
			{
				// for any non-whitespace character, map it to lower/uppercase alphabetic characters
				// this should avoid collisions with renamed strings to the point that it isn't a problem
				char altVal = name[i] % 52;
				if (altVal > 25)
				{
					name[i] = altVal + 'A';
				}
				else
				{
					name[i] = altVal + 'a';
				}
			}
		}
	}

	struct ScopeMember *existingStringLiteral = Scope_lookup(scope, name);
	if (existingStringLiteral != NULL)
	{
		return existingStringLiteral->entry;
	}
	else
	{
		return Scope_createObject(scope, name, myLocalPointer, nameLen + 1, 1, 0, 1);
	}
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

struct ObjectEntry *Scope_lookupObject(struct Scope *scope, char *name)
{
	struct ScopeMember *lookedUp = Scope_lookup(scope, name);
	if (lookedUp == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Failure looking up scope with name [%s]\n", name);
	}

	switch (lookedUp->type)
	{
	case e_object:
		return lookedUp->entry;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Unexpected symbol table entry type found when attempting to look up object [%s]\n", name);
	}
}

// return the actual size of a primitive in bytes
int GetSizeOfPrimitive(enum variableTypes type)
{
	switch (type)
	{
	case vt_uint8:
		return 1;

	case vt_uint16:
		return 2;

	case vt_uint32:
		return 4;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Unexepcted primitive type (enum variableTypes %d)!\n", type);
	}
}

// // return the actual size of a variable in bytes (lookup by its name only)
// int Scope_getSizeOfVariableByString(struct Scope *scope, char *name, char beingDereferenced)
// {
// 	struct VariableEntry *theVariable = Scope_lookupVarByString(scope, name);
// 	int realIndirectionLevel = theVariable->indirectionLevel - (beingDereferenced > 0);
// 	if (realIndirectionLevel > 0)
// 	{
// 		return 4;
// 	}
// 	else
// 	{
// 		return GetSizeOfPrimitive(theVariable->type);
// 	}
// }

// // return the actual size of a variable in bytes (lookup by its name using the ast for line/col error messages)
int Scope_getSizeOfVariableByAst(struct Scope *scope, struct AST *name)
{
	struct VariableEntry *theVariable = Scope_lookupVar(scope, name);
	if (theVariable->indirectionLevel > 0)
	{
		return 4;
	}

	switch (theVariable->type)
	{
	case vt_uint8:
	case vt_uint16:
	case vt_uint32:
		return GetSizeOfPrimitive(theVariable->type);
		break;
	default:
		ErrorWithAST(ERROR_INTERNAL, name, "Variable '%s' has unexpected type %d!\n", name->value, theVariable->type);
	}
}

int Scope_getSizeOfVariable(struct Scope *scope, struct VariableEntry *v)
{
	if (v->indirectionLevel > 0)
	{
		return 4;
	}

	switch (v->type)
	{
	case vt_uint8:
		return 1;

	case vt_uint16:
		return 2;

	case vt_uint32:
		return 4;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Variable '%s' has unexpected type %d!\n", v->name, v->type);
	}
}

int Scope_getSizeOfArrayElement(struct Scope *scope, struct VariableEntry *v)
{
	if (v->indirectionLevel < 1)
	{
		ErrorAndExit(ERROR_INTERNAL, "Non-indirect variable %s passed to Scope_getSizeOfArrayElement!\n", v->name);
	}
	else if (v->indirectionLevel == 1)
	{
		return GetSizeOfPrimitive(v->type);
	}
	else
	{
		return 4;
	}
}

// allocate and return a string containing the name and pointer level of a type
char *Scope_getNameOfType(struct Scope *scope, enum variableTypes t, int indirectionLevel)
{
	char *name;
	switch (t)
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
		ErrorAndExit(ERROR_INTERNAL, "Unexepcted variable type %d in Scope_getNaeOfType!\n", t);
	}

	int nameLen = strlen(name);
	int totalLen = nameLen + indirectionLevel + 1;
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
	printf("  - Type: %d", it->type);
	for (int i = 0; i < it->indirectionLevel; i++)
	{
		printf("*");
	}
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
			printf("> Function %s (returns %d) (defined: %d)\n\t%d bytes of arguments on stack\n", thisMember->name, theFunction->returnType, theFunction->isDefined, theFunction->argStackSize);
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

		case e_object:
		{
			struct ObjectEntry *thisObj = thisMember->entry;
			printf("> Object %s (%d bytes * %d)\n", thisMember->name, thisObj->size, thisObj->arraySize);
			for (int j = 0; j < depth; j++)
			{
				printf("\t");
			}
			if (thisObj->isGlobal)
			{
				printf("  - Global\n");
			}
			else
			{
				printf("  - Stack offset: %d\n", thisObj->stackOffset);
			}

			for (int j = 0; j < depth; j++)
			{
				printf("\t");
			}
			printf("  - Initialized: %d", thisObj->initialized);
			if (thisObj->initialized)
			{
				printf(" (hex: ");
				for (int i = 0; i < thisObj->size; i++)
				{
					printf("%02x ", thisObj->initializeTo[i]);
				}
				printf(")");
			}
			printf("\n\n");
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

	if(scope->parentFunction != NULL)
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

/*
struct VariableEntry *walkDeclaration(struct AST *declaration, struct Scope *wipScope, char isArgument)
{
	enum variableTypes theType;

	switch (declaration->type)
	{
	case t_uint8:
		theType = vt_uint8;
		break;

	case t_uint16:
		theType = vt_uint16;
		break;

	case t_uint32:
		theType = vt_uint32;
		break;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Unexpected type while walking declaration!\n");
		break;
	}

	struct AST *declared = NULL;
	int indirectionLevel = scrapePointers(declaration->child, &declared);

	int arraySize;

	if (declared->type == t_lBracket)
	{
		declared = declared->child;
		struct AST *declaredSize = declared->sibling;
		if (declaredSize->type != t_constant)
		{
			ErrorWithAST(ERROR_CODE, declaredSize, "Locally scoped arrays must have a constant size!\n");
		}
		arraySize = atoi(declaredSize->value);
	}
	else
	{
		arraySize = 1;
	}

	struct VariableEntry *created = NULL;

	// lookup the variable being assigned, only insert if unique
	// also covers modification of argument values
	if (!Scope_contains(wipScope, declared->value))
	{
		if (isArgument)
		{
			created = FunctionEntry_createArgument(wipScope->parentFunction, declared, theType, indirectionLevel, arraySize);
		}
		else
		{
			created = Scope_createVariable(wipScope, declared, theType, indirectionLevel, arraySize, wipScope->parentScope == NULL, current);
		}
	}
	else
	{
		ErrorWithAST(ERROR_CODE, declared, "Redeclaration of identifier '%s'\n", declared->value);
	}

	return created;
}

void walkStringLiteral(struct AST *stringLiteral, struct VariableEntry *myLocalPointer, struct Scope *wipScope)
{
	if (stringLiteral->type != t_string_literal)
	{
		ErrorAndExit(ERROR_INTERNAL, "walkStringLiteral called with AST type %s!\n", getTokenName(stringLiteral->type));
	}

	int stringSize = strlen(stringLiteral->value) + 1;
	char *stringValue = malloc(stringSize);
	strcpy(stringValue, stringLiteral->value);
	struct ObjectEntry *stringObject = Scope_createStringLiteral(wipScope, stringLiteral->value, myLocalPointer);
	stringObject->initialized = 1;
	stringObject->initializeTo = stringValue;

	if (myLocalPointer != NULL)
	{
		myLocalPointer->localPointerTo = stringObject;
	}
}

void walkStatement(struct AST *it, struct Scope *wipScope)
{
	switch (it->type)
	{
	case t_lCurly:
		walkScope(it, Scope_createSubScope(wipScope), 0);
		break;

	case t_star:
	{
	}

	case t_uint8:
	case t_uint16:
	case t_uint32:
	{
		walkDeclaration(it, wipScope, 0);
	}
	break;

	// check the LHS of the assignment to check if it is a declare-and-assign
	case t_single_equals:
	{
		struct VariableEntry *declaredVariable = NULL;
		switch (it->child->type)
		{
		case t_uint8:
		case t_uint16:
		case t_uint32:
		{
			declaredVariable = walkDeclaration(it->child, wipScope, 0);
		}
		break;

		default:
			break;
		}

		switch (it->child->sibling->type)
		{
		case t_string_literal:
		{
			// if this string literal corresponds to the declaration of a variable
			if (declaredVariable != NULL)
			{
				walkStringLiteral(it->child->sibling, declaredVariable, wipScope);
			}
			// alternatively, we could be setting a variable which has already been declared
			else
			{
				struct VariableEntry *literalAssigneedTo = Scope_lookupVar(wipScope, it->child);

				walkStringLiteral(it->child->sibling, literalAssigneedTo, wipScope);
			}
		}
		break;

		default:
			break;
		}
	}
	break;

	case t_if:
	{
		// walk the if true block (can skip the condition check because it can never declare anything)
		struct AST *ifTrue = it->child->sibling;
		if (ifTrue->type == t_lCurly)
		{
			struct Scope *ifTrueScope = Scope_createSubScope(wipScope);
			walkScope(ifTrue, ifTrueScope, 0);
		}
		else
		{
			walkStatement(ifTrue, wipScope);
		}

		if (ifTrue->sibling != NULL)
		{
			struct AST *ifFalse = ifTrue->sibling;
			if (ifFalse->type == t_lCurly)
			{
				struct Scope *ifFalseScope = Scope_createSubScope(wipScope);
				walkScope(ifFalse, ifFalseScope, 0);
			}
			else
			{
				walkStatement(ifFalse, wipScope);
			}
		}
	}
	break;

	case t_while:
	{
		struct AST *whileBody = it->child->sibling;
		if (whileBody->type == t_lCurly)
		{
			struct Scope *whileBodyScope = Scope_createSubScope(wipScope);
			walkScope(whileBody, whileBodyScope, 0);
		}
		else
		{
			walkStatement(whileBody, wipScope);
		}
	}
	break;

	case t_lParen:
	{
		struct AST *functionArgument = it->child->sibling;
		while (functionArgument != NULL)
		{
			if (functionArgument->type == t_string_literal)
			{
				walkStringLiteral(functionArgument, NULL, wipScope);
			}
			functionArgument = functionArgument->sibling;
		}
	}
	break;

	// asm blocks can't create new symbols so ignore
	case t_asm:
		break;

	default:
		// TODO: should this really be an internal error?
		ErrorAndExit(ERROR_INTERNAL, "Error walking AST for function %s - expected 'var', name, or function call, saw %s with value of [%s]\n", wipScope->parentFunction->name, getTokenName(it->type), it->value);
	}
}

void walkScope(struct AST *it, struct Scope *wipScope, char isMainScope)
{
	struct AST *scopeRunner = it->child;
	while (scopeRunner != NULL && scopeRunner->type != t_rCurly)
	{
		switch (scopeRunner->type)
		{
		// any statement starting with an identifier (eg 'a = b + 1', 'foo()', etc...) can't declare things so ignore
		case t_identifier:
		// return can't create new symbols so ignore
		case t_return:
			break;

		// otherwise we are looking at some arbitrary statement
		default:
			walkStatement(scopeRunner, wipScope);
			break;
		}
		scopeRunner = scopeRunner->sibling;
	}

	// sanity check to make sure we didn't run off the end and actually got the rCurly we expected
	if (scopeRunner == NULL || scopeRunner->type != t_rCurly)
	{
		ErrorAndExit(ERROR_INTERNAL, "Malformed AST at end of scope!\n");
	}
}

void walkFunction(struct AST *it, struct Scope *parentScope)
{
	struct AST *functionRunner = it->child;

	// skip past the argumnent declarations to the return type declaration
	struct AST *returnTypeRunner = functionRunner;
	while (returnTypeRunner->type != t_pointer_op)
	{
		returnTypeRunner = returnTypeRunner->sibling;
	}
	returnTypeRunner = returnTypeRunner->sibling;

	enum variableTypes returnType;
	switch (returnTypeRunner->type)
	{
	case t_void:
		returnType = vt_null;
		break;

	case t_uint8:
		returnType = vt_uint8;
		break;

	case t_uint16:
		returnType = vt_uint16;
		break;

	case t_uint32:
		returnType = vt_uint32;
		break;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Malformed AST as return type for function\n");
	}
	int returnIndirectionLevel = scrapePointers(returnTypeRunner->child, &returnTypeRunner);

	// child is the lparen, function name is the child of the lparen
	struct ScopeMember *lookedUpFunction = Scope_lookup(parentScope, functionRunner->child->value);
	struct FunctionEntry *parsedFunc = NULL;
	struct FunctionEntry *existingFunc = NULL;

	if (lookedUpFunction != NULL)
	{
		existingFunc = lookedUpFunction->entry;
		parsedFunc = FunctionEntry_new(parentScope, functionRunner->child->value, returnType, returnIndirectionLevel);
	}
	else
	{
		parsedFunc = Scope_createFunction(parentScope, functionRunner->child->value, returnType, returnIndirectionLevel);
		parsedFunc->mainScope->parentScope = parentScope;
		// record return type
		parsedFunc->returnType = returnType;
		parsedFunc->returnIndirectionLevel = returnIndirectionLevel;
	}

	functionRunner = functionRunner->sibling; // start at argument definitions

	// record argument definitions
	while (functionRunner->type != t_pointer_op)
	{
		switch (functionRunner->type)
		{
			// looking at argument declarations
		case t_uint8:
		case t_uint16:
		case t_uint32:
		{
			walkDeclaration(functionRunner, parsedFunc->mainScope, 1);
		}
		break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Malformed AST within function - expected function name and main scope only!\nMalformed node was of type %s with value [%s]\n", getTokenName(functionRunner->type), functionRunner->value);
		}
		functionRunner = functionRunner->sibling;
	}
	// skip past return type
	functionRunner = functionRunner->sibling;
	functionRunner = functionRunner->sibling;

	// if we are defining an existing declaration, make sure they match
	if (existingFunc)
	{
		int mismatch = 0;

		if (returnType != existingFunc->returnType || returnIndirectionLevel != existingFunc->returnIndirectionLevel)
		{
			mismatch = 1;
		}

		// ensure we have both the same number of bytes of arguments and same number of arguments
		if (!mismatch &&
			(existingFunc->argStackSize == parsedFunc->argStackSize) &&
			(existingFunc->arguments->size == parsedFunc->arguments->size))
		{
			// if we have same number of bytes and same number, ensure everything is exactly the same
			for (int i = 0; i < existingFunc->arguments->size; i++)
			{
				struct VariableEntry *existingArg = existingFunc->arguments->data[i];
				struct VariableEntry *parsedArg = parsedFunc->arguments->data[i];
				// ensure all arguments in order have same name, type, indirection level
				if (strcmp(existingArg->name, parsedArg->name) ||
					(existingArg->type != parsedArg->type) ||
					(existingArg->indirectionLevel != parsedArg->indirectionLevel))
				{
					mismatch = 1;
					break;
				}
			}
		}
		else
		{
			mismatch = 1;
		}

		if (mismatch)
		{
			printf("\nConflicting declarations of function:\n");

			char *existingReturnType = Scope_getNameOfType(parentScope, existingFunc->returnType, existingFunc->returnIndirectionLevel);
			printf("\t%s %s(", existingReturnType, existingFunc->name);
			free(existingReturnType);
			for (int i = 0; i < existingFunc->arguments->size; i++)
			{
				struct VariableEntry *existingArg = existingFunc->arguments->data[i];

				char *argType = Scope_getNameOfType(parentScope, existingArg->type, existingArg->indirectionLevel);
				printf("%s %s", argType, existingArg->name);
				free(argType);

				if (i < existingFunc->arguments->size - 1)
				{
					printf(", ");
				}
				else
				{
					printf(")");
				}
			}
			char *parsedReturnType = Scope_getNameOfType(parentScope, returnType, returnIndirectionLevel);
			printf("\n\t%s %s(", parsedReturnType, parsedFunc->name);
			free(parsedReturnType);
			for (int i = 0; i < parsedFunc->arguments->size; i++)
			{
				struct VariableEntry *parsedArg = parsedFunc->arguments->data[i];

				char *argType = Scope_getNameOfType(parentScope, parsedArg->type, parsedArg->indirectionLevel);
				printf("%s %s", argType, parsedArg->name);
				free(argType);

				if (i < parsedFunc->arguments->size - 1)
				{
					printf(", ");
				}
				else
				{
					printf(")");
				}
			}
			printf("\n");

			ErrorWithAST(ERROR_CODE, it, " ");
		}
	}

	if (functionRunner != NULL)
	{
		if (existingFunc && existingFunc->isDefined)
		{
			ErrorWithAST(ERROR_CODE, it, "Redefinition of function %s->%s\n",
						 existingFunc->name,
						 Scope_getNameOfType(parentScope, existingFunc->returnType, existingFunc->returnIndirectionLevel));
		}

		if (existingFunc != NULL)
		{
			existingFunc->isDefined = 1;
			walkScope(functionRunner, existingFunc->mainScope, 1);
		}
		else
		{
			parsedFunc->isDefined = 1;
			walkScope(functionRunner, parsedFunc->mainScope, 1);
		}
	}

	if (lookedUpFunction != NULL)
		FunctionEntry_free(parsedFunc);
}
*/