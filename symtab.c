#include "symtab.h"
#include "codegen_generic.h"
#include "symtab_scope.h"

extern struct Dictionary *parseDict;

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

void VariableEntry_Print(struct VariableEntry *variable, int depth)
{
	char *typeName = Type_GetName(&variable->type);
	printf("%s %s\n", typeName, variable->name);
	free(typeName);
}

void Scope_print(struct Scope *scope, size_t depth, char printTAC)
{
	for (size_t entryIndex = 0; entryIndex < scope->entries->size; entryIndex++)
	{
		struct ScopeMember *thisMember = scope->entries->data[entryIndex];

		if (thisMember->type != e_basicblock || printTAC)
		{
			for (size_t depthPrint = 0; depthPrint < depth; depthPrint++)
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
			for (size_t depthPrint = 0; depthPrint < depth; depthPrint++)
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
