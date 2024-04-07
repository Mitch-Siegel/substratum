#include "symtab.h"
#include "codegen_generic.h"

extern struct Dictionary *parseDict;

char *symbolNames[] = {
	"variable",
	"function"};

struct FunctionEntry *FunctionEntry_new(struct Scope *parentScope, struct AST *nameTree, struct Type *returnType)
{
	struct FunctionEntry *newFunction = malloc(sizeof(struct FunctionEntry));
	newFunction->arguments = Stack_New();
	newFunction->argStackSize = 0;
	newFunction->mainScope = Scope_new(parentScope, nameTree->value, newFunction);
	newFunction->BasicBlockList = LinkedList_New();
	newFunction->correspondingTree = *nameTree;
	newFunction->mainScope->parentFunction = newFunction;
	newFunction->returnType = *returnType;
	newFunction->name = nameTree->value;
	newFunction->isDefined = 0;
	newFunction->isAsmFun = 0;
	newFunction->callsOtherFunction = 0;
	return newFunction;
}

void FunctionEntry_free(struct FunctionEntry *function)
{
	Stack_Free(function->arguments);
	LinkedList_Free(function->BasicBlockList, NULL);
	Scope_free(function->mainScope);
	free(function);
}

struct SymbolTable *SymbolTable_new(char *name)
{
	struct SymbolTable *wip = malloc(sizeof(struct SymbolTable));
	wip->name = name;
	wip->globalScope = Scope_new(NULL, "Global", NULL);
	struct BasicBlock *globalBlock = BasicBlock_new(0);

	// manually insert a basic block for global code so we can give it the custom name of "globalblock"
	Scope_insert(wip->globalScope, "globalblock", globalBlock, e_basicblock);

	return wip;
}

void SymbolTable_print(struct SymbolTable *table, char printTAC)
{
	printf("~~~~~~~~~~~~~\n");
	printf("Symbol Table For %s:\n\n", table->name);
	Scope_print(table->globalScope, 0, printTAC);
	printf("~~~~~~~~~~~~~\n\n");
}

char *SymbolTable_mangleName(struct Scope *scope, struct Dictionary *dict, char *toMangle)
{
	char *scopeName = scope->name;

	char *mangledName = malloc(strlen(toMangle) + strlen(scopeName) + 2);
	sprintf(mangledName, "%s.%s", scopeName, toMangle);
	char *newName = Dictionary_LookupOrInsert(dict, mangledName);
	free(mangledName);
	return newName;
}

void SymbolTable_moveMemberToParentScope(struct Scope *scope, struct ScopeMember *toMove, size_t *indexWithinCurrentScope)
{
	Scope_insert(scope->parentScope, toMove->name, toMove->entry, toMove->type);
	free(scope->entries->data[*indexWithinCurrentScope]);
	for (size_t entryIndex = *indexWithinCurrentScope; entryIndex < scope->entries->size - 1; entryIndex++)
	{
		scope->entries->data[entryIndex] = scope->entries->data[entryIndex + 1];
	}
	scope->entries->size--;

	// decrement this so that if we are using it as an iterator to entries in the original scope, it is still valid
	(*indexWithinCurrentScope)--;
}

void SymbolTable_collapseScopesRec(struct Scope *scope, struct Dictionary *dict, size_t depth)
{
	// first pass: recurse depth-first so everything we do at this call depth will be 100% correct
	for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
	{
		struct ScopeMember *thisMember = scope->entries->data[entryIndex];
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
		case e_class:
		case e_basicblock:
			break;
		}
	}

	// only rename basic block operands if depth > 0
	// we only want to alter variable names for variables whose names we will mangle as a result of a scope collapse
	if (depth > 0)
	{
		// second pass: rename basic block operands relevant to the current scope
		for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
		{
			struct ScopeMember *thisMember = scope->entries->data[entryIndex];
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
						for (size_t operandIndex = 0; operandIndex < 4; operandIndex++)
						{
							// check only TAC operands that both exist and refer to a named variable from the source code (ignore temps etc)
							if ((thisTAC->operands[operandIndex].type.basicType != vt_null) &&
								((thisTAC->operands[operandIndex].permutation == vp_standard) || (thisTAC->operands[operandIndex].permutation == vp_objptr)))
							{
								char *originalName = thisTAC->operands[operandIndex].name.str;

								// bail out early if the variable is not declared within this scope, as we will not need to mangle it
								if (!Scope_contains(scope, originalName))
								{
									continue;
								}

								// if the declaration for the variable is owned by this scope, ensure that we actually get a variable or argument
								struct VariableEntry *variableToMangle = Scope_lookupVarByString(scope, originalName);

								// only mangle things which are not string literals
								if (variableToMangle->isStringLiteral == 0)
								{
									// it should not be possible to see a global as being declared here
									if (variableToMangle->isGlobal)
									{
										ErrorAndExit(ERROR_INTERNAL, "Declaration of variable %s at inner scope %s is marked as a global!\n", variableToMangle->name, scope->name);
									}
									thisTAC->operands[operandIndex].name.str = SymbolTable_mangleName(scope, dict, originalName);
								}
							}
						}
					}
				}
			}
			break;

			case e_variable:
			case e_argument:
			case e_class:
				break;
			}
		}
	}

	// third pass: move nested members to parent scope based on mangled names
	// also moves globals outwards
	for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
	{
		struct ScopeMember *thisMember = scope->entries->data[entryIndex];
		switch (thisMember->type)
		{
		case e_scope:
		case e_function:
			break;

		case e_basicblock:
		{
			if (depth > 0 && scope->parentScope != NULL)
			{
				SymbolTable_moveMemberToParentScope(scope, thisMember, &entryIndex);
			}
		}
		break;

		case e_variable:
		case e_argument:
		{
			if (scope->parentScope != NULL)
			{
				struct VariableEntry *variableToMove = thisMember->entry;
				// we will only ever do anything if we are depth >0 or need to kick a global variable up a scope
				if ((depth > 0) || (variableToMove->isGlobal))
				{
					// mangle all non-global names (want to mangle everything except for string literal names)
					if (!variableToMove->isGlobal)
					{
						thisMember->name = SymbolTable_mangleName(scope, dict, thisMember->name);
					}
					SymbolTable_moveMemberToParentScope(scope, thisMember, &entryIndex);
				}
			}
		}
		break;

		default:
			break;
		}
	}
}

void SymbolTable_collapseScopes(struct SymbolTable *table, struct Dictionary *dict)
{
	SymbolTable_collapseScopesRec(table->globalScope, parseDict, 0);
}

void SymbolTable_free(struct SymbolTable *table)
{
	Scope_free(table->globalScope);
	free(table);
}

/*
 * Scope functions
 *
 */
struct Scope *Scope_new(struct Scope *parentScope, char *name, struct FunctionEntry *parentFunction)
{
	struct Scope *wip = malloc(sizeof(struct Scope));
	wip->entries = Stack_New();

	wip->parentFunction = parentFunction;
	wip->parentScope = parentScope;
	wip->name = name;
	wip->subScopeCount = 0;
	return wip;
}

void Scope_free(struct Scope *scope)
{
	for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
	{
		struct ScopeMember *examinedEntry = scope->entries->data[entryIndex];
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
					for (size_t arrayInitializeIndex = 0; arrayInitializeIndex < variableType->arraySize; arrayInitializeIndex++)
					{
						free(variableType->initializeArrayTo[arrayInitializeIndex]);
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

		case e_class:
		{
			struct ClassEntry *theClass = examinedEntry->entry;
			Scope_free(theClass->members);

			while (theClass->memberLocations->size > 0)
			{
				free(Stack_Pop(theClass->memberLocations));
			}

			Stack_Free(theClass->memberLocations);
			free(theClass);
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
										   u8 isGlobal,
										   size_t declaredAt,
										   u8 isArgument)
{
	struct VariableEntry *newVariable = malloc(sizeof(struct VariableEntry));
	newVariable->type = *type;
	newVariable->stackOffset = 0;
	newVariable->assignedAt = -1;
	newVariable->mustSpill = 0;
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

	// don't take these as arguments as they will only ever be set for specific declarations
	newVariable->isExtern = 0;
	newVariable->isStringLiteral = 0;

	if (Scope_contains(scope, name->value))
	{
		ErrorWithAST(ERROR_CODE, name, "Redifinition of symbol %s!\n", name->value);
	}

	// if we have an argument, it will be trivially spilled because it is passed in on the stack
	if (isArgument)
	{
		// compute the padding necessary for alignment of this argument
		scope->parentFunction->argStackSize += Scope_ComputePaddingForAlignment(scope, type, scope->parentFunction->argStackSize);

		// put our argument's offset at the newly-aligned stack size, then add the size of the argument to the argument stack size
		newVariable->stackOffset = scope->parentFunction->argStackSize;
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
struct FunctionEntry *Scope_createFunction(struct Scope *parentScope, struct AST *nameTree, struct Type *returnType)
{
	struct FunctionEntry *newFunction = FunctionEntry_new(parentScope, nameTree, returnType);
	Scope_insert(parentScope, nameTree->value, newFunction, e_function);
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

struct ClassEntry *Scope_createClass(struct Scope *scope,
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

int Scope_ComputePaddingForAlignment(struct Scope *scope, struct Type *alignedType, int currentOffset)
{
	// calculate the number of bytes to which this member needs to be aligned
	int alignBytesForType = unalignSize(Scope_getAlignmentOfType(scope, alignedType));

	// compute how many bytes of padding we will need before this member to align it correctly
	int paddingRequired = 0;
	int bytesAfterAlignBoundary = currentOffset % alignBytesForType;
	if (bytesAfterAlignBoundary)
	{
		paddingRequired = alignBytesForType - bytesAfterAlignBoundary;
	}

	// add the padding to the total size of the class
	return paddingRequired;
}

void Class_assignOffsetToMemberVariable(struct ClassEntry *class,
										struct VariableEntry *variable)
{

	struct ClassMemberOffset *newMemberLocation = malloc(sizeof(struct ClassMemberOffset));

	// add the padding to the total size of the class
	class->totalSize += Scope_ComputePaddingForAlignment(class->members, &variable->type, class->totalSize);

	// place the new member at the (now aligned) current max size of the class
	newMemberLocation->offset = class->totalSize;
	newMemberLocation->variable = variable;

	// add the size of the member we just added to the total size of the class
	class->totalSize += Scope_getSizeOfType(class->members, &variable->type);

	Stack_Push(class->memberLocations, newMemberLocation);
}

struct ClassMemberOffset *Class_lookupMemberVariable(struct ClassEntry *class,
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

	for (int i = 0; i < class->memberLocations->size; i++)
	{
		struct ClassMemberOffset *member = class->memberLocations->data[i];
		if (!strcmp(member->variable->name, name->value))
		{
			return member;
		}
	}

	ErrorWithAST(ERROR_CODE, name, "Use of nonexistent member variable %s in class %s\n", name->value, class->name);
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

struct FunctionEntry *Scope_lookupFunByString(struct Scope *scope, char *name)
{
	struct ScopeMember *lookedUp = Scope_lookup(scope, name);
	if (lookedUp == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Lookup of undeclared function '%s'\n", name);
	}

	switch (lookedUp->type)
	{
	case e_function:
		return lookedUp->entry;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Lookup returned unexpected symbol table entry type when looking up function!\n");
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

struct ClassEntry *Scope_lookupClass(struct Scope *scope,
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

struct ClassEntry *Scope_lookupClassByType(struct Scope *scope,
										   struct Type *type)
{
	if (type->classType.name == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Type with null classType name passed to Scope_lookupClassByType!\n");
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
		ErrorAndExit(ERROR_INTERNAL, "Scope_lookupClassByType for %s lookup got a non-class ScopeMember!\n", type->classType.name);
	}
}

size_t Scope_getSizeOfType(struct Scope *scope, struct Type *type)
{
	size_t size = 0;

	if (type->indirectionLevel > 0)
	{
		size = MACHINE_REGISTER_SIZE_BYTES;
		if (type->arraySize == 0)
		{
			return size;
		}
	}

	switch (type->basicType)
	{
	case vt_null:
		ErrorAndExit(ERROR_INTERNAL, "Scope_getSizeOfType called with basic type of vt_null!\n");
		break;

	case vt_any:
		// triple check that `any` is only ever used as a pointer type a la c's void *
		if ((type->indirectionLevel == 0) || (type->arraySize > 0))
		{
			char *illegalAnyTypeName = Type_GetName(type);
			ErrorAndExit(ERROR_INTERNAL, "Illegal `any` type detected - %s\nSomething slipped through earlier sanity checks on use of `any` as `any *` or some other pointer type\n", illegalAnyTypeName);
		}
		size = sizeof(u8);
		break;

	case vt_u8:
		size = sizeof(u8);
		break;

	case vt_u16:
		size = sizeof(u16);
		break;

	case vt_u32:
		size = sizeof(u32);
		break;

	case vt_u64:
		size = sizeof(u64);
		break;

	case vt_class:
	{
		struct ClassEntry *class = Scope_lookupClassByType(scope, type);
		size = class->totalSize;
	}
	break;
	}

	if (type->arraySize > 0)
	{
		if (type->indirectionLevel > 1)
		{
			size = MACHINE_REGISTER_SIZE_BYTES;
		}

		size *= type->arraySize;
	}

	return size;
}

size_t Scope_getSizeOfDereferencedType(struct Scope *scope, struct Type *type)
{
	struct Type dereferenced = *type;
	dereferenced.indirectionLevel--;

	if (dereferenced.arraySize > 0)
	{
		dereferenced.arraySize = 0;
		dereferenced.indirectionLevel++;
	}
	return Scope_getSizeOfType(scope, &dereferenced);
}

size_t Scope_getSizeOfArrayElement(struct Scope *scope, struct VariableEntry *variable)
{
	size_t size = 0;
	if (variable->type.arraySize < 1)
	{
		if (variable->type.indirectionLevel)
		{
			char *nonArrayPointerTypeName = Type_GetName(&variable->type);
			printf("Warning - variable %s with type %s used in array access!\n", variable->name, nonArrayPointerTypeName);
			free(nonArrayPointerTypeName);
			struct Type elementType = variable->type;
			elementType.indirectionLevel--;
			elementType.arraySize = 0;
			size = Scope_getSizeOfType(scope, &elementType);
		}
		else
		{
			ErrorAndExit(ERROR_INTERNAL, "Non-array variable %s passed to Scope_getSizeOfArrayElement!\n", variable->name);
		}
	}
	else
	{
		if (variable->type.indirectionLevel == 0)
		{
			struct Type elementType = variable->type;
			elementType.indirectionLevel--;
			elementType.arraySize = 0;
			size = Scope_getSizeOfType(scope, &elementType);
		}
		else
		{
			size = MACHINE_REGISTER_SIZE_BYTES;
		}
	}

	return size;
}

// Return the number of bits required to align a given type
u8 Scope_getAlignmentOfType(struct Scope *scope, struct Type *type)
{
	u8 alignBits = 0;

	// TODO: handle arrays of pointers
	if (type->indirectionLevel > 0)
	{
		alignBits = alignSize(sizeof(size_t));
		if (type->arraySize == 0)
		{
			return alignBits;
		}
	}

	switch (type->basicType)
	{
	case vt_null:
		ErrorAndExit(ERROR_INTERNAL, "Scope_getAlignmentOfType called with basic type of vt_null!\n");
		break;

	case vt_any:
		// triple check that `any` is only ever used as a pointer type a la c's void *
		if ((type->indirectionLevel == 0) || (type->arraySize > 0))
		{
			char *illegalAnyTypeName = Type_GetName(type);
			ErrorAndExit(ERROR_INTERNAL, "Illegal `any` type detected - %s\nSomething slipped through earlier sanity checks on use of `any` as `any *` or some other pointer type\n", illegalAnyTypeName);
		}
		// TODO: unreachable? indirectionlevels > 0 should always be caught above.
		alignBits = alignSize(sizeof(size_t));
		break;

	// the compiler is becoming the compilee
	case vt_u8:
		alignBits = alignSize(sizeof(u8));
		break;

	case vt_u16:
		alignBits = alignSize(sizeof(u16));
		break;

	case vt_u32:
		alignBits = alignSize(sizeof(u32));
		break;

	case vt_u64:
		alignBits = alignSize(sizeof(u64));
		break;

	case vt_class:
	{
		struct ClassEntry *class = Scope_lookupClassByType(scope, type);

		for (size_t i = 0; i < class->memberLocations->size; i++)
		{
			struct ClassMemberOffset *examinedMember = (struct ClassMemberOffset *)class->memberLocations->data[i];

			u8 examinedMemberAlignment = Scope_getAlignmentOfType(scope, &examinedMember->variable->type);
			if (examinedMemberAlignment > alignBits)
			{
				alignBits = examinedMemberAlignment;
			}
		}
	}
	break;
	}

	// TODO: see above todo about handling arrays of pointers
	if (type->arraySize > 0)
	{
		if (type->indirectionLevel > 1)
		{
			alignBits = alignSize(sizeof(size_t));
		}
	}

	return alignBits;
}

void VariableEntry_Print(struct VariableEntry *variable, int depth)
{
	char *typeName = Type_GetName(&variable->type);
	printf("%s %s\n", typeName, variable->name);
	free(typeName);
}

void Scope_print(struct Scope *scope, size_t depth, char printTAC)
{
	for (size_t i = 0; i < scope->entries->size; i++)
	{
		struct ScopeMember *thisMember = scope->entries->data[i];

		if (thisMember->type != e_basicblock || printTAC)
		{
			for (size_t j = 0; j < depth; j++)
			{
				printf("\t");
			}
		}

		switch (thisMember->type)
		{
		case e_argument:
		{
			struct VariableEntry *theArgument = thisMember->entry;
			printf("> Argument: ");
			VariableEntry_Print(theArgument, depth);
			for (size_t j = 0; j < depth; j++)
			{
				printf("\t");
			}
			printf("  - Stack offset: %d\n", theArgument->stackOffset);
		}
		break;

		case e_variable:
		{
			struct VariableEntry *theVariable = thisMember->entry;
			printf("> ");
			VariableEntry_Print(theVariable, depth);
		}
		break;

		case e_class:
		{
			struct ClassEntry *theClass = thisMember->entry;
			printf("> Class %s:\n", thisMember->name);
			for (size_t j = 0; j < depth; j++)
			{
				printf("\t");
			}
			printf("  - Size: %d bytes\n", theClass->totalSize);
			Scope_print(theClass->members, depth + 1, 0);
		}
		break;

		case e_function:
		{
			struct FunctionEntry *theFunction = thisMember->entry;
			char *returnTypeName = Type_GetName(&theFunction->returnType);
			printf("> Function %s (returns %s) (defined: %d)\n\t%ld bytes of arguments on stack\n", thisMember->name, returnTypeName, theFunction->isDefined, theFunction->argStackSize);
			free(returnTypeName);
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

void Scope_addBasicBlock(struct Scope *scope, struct BasicBlock *block)
{
	const u8 basicBlockNameStrSize = 10; // TODO: manage this better
	char *blockName = malloc(basicBlockNameStrSize);
	sprintf(blockName, "Block%d", block->labelNum);
	Scope_insert(scope, Dictionary_LookupOrInsert(parseDict, blockName), block, e_basicblock);
	free(blockName);

	if (scope->parentFunction != NULL)
	{
		LinkedList_Append(scope->parentFunction->BasicBlockList, block);
	}
}

/*
 * AST walk and symbol table generation functions
 */

// scrape down a chain of adjacent sibling star tokens, expecting something at the bottom
size_t scrapePointers(struct AST *pointerAST, struct AST **resultDestination)
{
	size_t dereferenceDepth = 0;
	pointerAST = pointerAST->sibling;

	while ((pointerAST != NULL) && (pointerAST->type == t_dereference))
	{
		dereferenceDepth++;
		pointerAST = pointerAST->sibling;
	}

	*resultDestination = pointerAST;
	return dereferenceDepth;
}
