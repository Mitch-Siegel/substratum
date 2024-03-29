#include "linearizer.h"
#include "linearizer_generic.h"
#include "codegen_generic.h"

#include <ctype.h>

/*
 * These functions walk the AST and convert it to three-address code
 */
struct TempList *temps;
extern struct Dictionary *parseDict;
struct SymbolTable *walkProgram(struct AST *program)
{
	struct SymbolTable *programTable = SymbolTable_new("Program");
	struct BasicBlock *globalBlock = Scope_lookup(programTable->globalScope, "globalblock")->entry;
	struct BasicBlock *asmBlock = BasicBlock_new(1);
	Scope_addBasicBlock(programTable->globalScope, asmBlock);
	temps = TempList_New();

	int globalTACIndex = 0;
	int globalTempNum = 0;

	struct AST *programRunner = program;
	while (programRunner != NULL)
	{
		switch (programRunner->type)
		{
		case t_variable_declaration:
			// walkVariableDeclaration sets isGlobal for us by checking if there is no parent scope
			walkVariableDeclaration(programRunner, globalBlock, programTable->globalScope, &globalTACIndex, &globalTempNum, 0);
			break;

		case t_extern:
		{
			struct VariableEntry *declaredVariable = walkVariableDeclaration(programRunner->child, globalBlock, programTable->globalScope, &globalTACIndex, &globalTempNum, 0);
			declaredVariable->isExtern = 1;
		}
		break;

		case t_class:
		{
			// if (programRunner->child->sibling->type == t_compound_statement)
			// {
			walkClassDeclaration(programRunner, globalBlock, programTable->globalScope);
			break;
			// }
			// else // TODO: disallow bad sibling types?
			// {
			// walkVariableDeclaration(programRunner, globalBlock, programTable->globalScope, &globalTACIndex, &globalTempNum, 0);
			// }
		}
		break;

		case t_assign:
			walkAssignment(programRunner, globalBlock, programTable->globalScope, &globalTACIndex, &globalTempNum);
			break;

		case t_fun:
			walkFunctionDeclaration(programRunner, programTable->globalScope);
			break;

		// ignore asm blocks
		case t_asm:
			walkAsmBlock(programRunner, asmBlock, programTable->globalScope, &globalTACIndex, &globalTempNum);
			break;

		default:
			ErrorAndExit(ERROR_INTERNAL,
						 "Error walking AST - got %s with type %s\n",
						 programRunner->value,
						 getTokenName(programRunner->type));
			break;
		}
		programRunner = programRunner->sibling;
	}

	return programTable;
}

void walkTypeName(struct AST *tree, struct Scope *scope, struct Type *populateTypeTo)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkTypeName: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_type_name)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkTypeName!\n", getTokenName(tree->type));
	}

	memset(populateTypeTo, 0, sizeof(struct Type));

	struct AST *className = NULL;

	switch (tree->child->type)
	{
	case t_any:
		populateTypeTo->basicType = vt_any;
		break;

	case t_u8:
		populateTypeTo->basicType = vt_u8;
		break;

	case t_u16:
		populateTypeTo->basicType = vt_u16;
		break;

	case t_u32:
		populateTypeTo->basicType = vt_u32;
		break;

	case t_u64:
		populateTypeTo->basicType = vt_u64;
		break;

	case t_identifier:
		populateTypeTo->basicType = vt_class;

		className = tree->child;
		if (className->type != t_identifier)
		{
			ErrorWithAST(ERROR_INTERNAL,
						 className,
						 "Malformed AST seen in declaration!\nExpected class name as child of \"class\", saw %s (%s)!",
						 className->value,
						 getTokenName(className->type));
		}
		populateTypeTo->classType.name = className->value;
		break;

	default:
		ErrorWithAST(ERROR_INTERNAL, tree, "Malformed AST seen in declaration!");
	}

	struct AST *declaredArray = NULL;
	populateTypeTo->indirectionLevel = scrapePointers(tree->child, &declaredArray);

	// if declaring something with the 'any' type, make sure it's only as a pointer (as its intended use is to point to unstructured data)
	if (populateTypeTo->basicType == vt_any)
	{
		if (populateTypeTo->indirectionLevel == 0)
		{
			ErrorWithAST(ERROR_CODE, declaredArray, "Use of the type 'any' without indirection is forbidden!\n'any' is meant to represent unstructured data as a pointer type only\n(declare as `any *`, `any **`, etc...)\n");
		}
		else if (populateTypeTo->arraySize > 0)
		{
			ErrorWithAST(ERROR_CODE, declaredArray, "Use of the type 'any' in arrays is forbidden!\n'any' is meant to represent unstructured data as a pointer type only\n(declare as `any *`, `any **`, etc...)\n");
		}
	}

	// don't allow declaration of variables of undeclared class or array of undeclared class (except pointers)
	if ((populateTypeTo->basicType == vt_class) && (populateTypeTo->indirectionLevel == 0))
	{
		// the lookup will bail out if an attempt is made to use an undeclared class
		Scope_lookupClass(scope, className);
	}

	// if we are declaring an array, set the string with the size as the second operand
	if (declaredArray != NULL)
	{
		if (declaredArray->type != t_array_index)
		{
			AST_Print(declaredArray, 0);
			ErrorWithAST(ERROR_INTERNAL, declaredArray, "Unexpected AST at end of pointer declarations!");
		}
		char *arraySizeString = declaredArray->child->value;
		int declaredArraySize = atoi(arraySizeString);

		populateTypeTo->arraySize = declaredArraySize;
	}
	else
	{
		populateTypeTo->arraySize = 0;
	}
}

struct VariableEntry *walkVariableDeclaration(struct AST *tree,
											  struct BasicBlock *block,
											  struct Scope *scope,
											  int *TACIndex,
											  int *tempNum,
											  char isArgument)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkVariableDeclaration: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_variable_declaration)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkVariableDeclaration!\n", getTokenName(tree->type));
	}

	struct Type declaredType;

	/* 'class' trees' children are the class name
	 * other variables' children are the pointer or variable name
	 * so we need to start at tree->child for non-class or tree->child->sibling for classes
	 */

	if (tree->child->type != t_type_name)
	{
		ErrorWithAST(ERROR_INTERNAL, tree->child, "Malformed AST seen in declaration!");
	}

	walkTypeName(tree->child, scope, &declaredType);

	// automatically set as a global if there is no parent scope (declaring at the outermost scope)
	struct VariableEntry *declaredVariable = Scope_createVariable(scope,
																  tree->child->sibling,
																  &declaredType,
																  (scope->parentScope == NULL),
																  *TACIndex,
																  isArgument);

	return declaredVariable;
}

void walkArgumentDeclaration(struct AST *tree,
							 struct BasicBlock *block,
							 int *TACIndex,
							 int *tempNum,
							 struct FunctionEntry *fun)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkArgumentDeclaration: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	struct VariableEntry *declaredArgument = walkVariableDeclaration(tree, block, fun->mainScope, TACIndex, tempNum, 1);

	declaredArgument->assignedAt = 0;
	declaredArgument->isAssigned = 1;

	Stack_Push(fun->arguments, declaredArgument);
}

void walkFunctionDeclaration(struct AST *tree,
							 struct Scope *scope)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkFunctionDeclaration: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_fun)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkFunctionDeclaration!\n", getTokenName(tree->type));
	}

	// skip past the argumnent declarations to the return type declaration
	struct AST *returnTypeTree = tree->child;

	// functions return nothing in the default case
	struct Type returnType;
	memset(&returnType, 0, sizeof(struct Type));

	struct AST *functionNameTree = NULL;

	// if the function returns something, its return type will be the first child of the 'fun' token
	if (returnTypeTree->type == t_type_name)
	{
		walkTypeName(returnTypeTree, scope, &returnType);

		// if we are returning a class, ensure that we're returning some sort of pointer, not a whole object
		if ((returnType.basicType == vt_class) && (returnType.indirectionLevel == 0))
		{
			// use tree->child to get the original returnTypeTree AST as scrapePointers may have modified it
			ErrorWithAST(ERROR_CODE, tree->child, "Return of class object types is not supported!\n");
		}

		functionNameTree = returnTypeTree->sibling;
	}
	else
	{
		// there actually is no return type tree, we just go directly to argument declarations
		functionNameTree = returnTypeTree;
	}

	// child is the lparen, function name is the child of the lparen
	struct ScopeMember *lookedUpFunction = Scope_lookup(scope, functionNameTree->value);
	struct FunctionEntry *parsedFunc = NULL;
	struct FunctionEntry *existingFunc = NULL;

	if (lookedUpFunction != NULL)
	{
		existingFunc = lookedUpFunction->entry;
		parsedFunc = FunctionEntry_new(scope, functionNameTree, &returnType);
	}
	else
	{
		parsedFunc = Scope_createFunction(scope, functionNameTree, &returnType);
		parsedFunc->mainScope->parentScope = scope;
	}

	struct AST *argumentRunner = functionNameTree->sibling;
	int TACIndex = 0;
	int tempNum = 0;
	struct BasicBlock *block = BasicBlock_new(0);
	while ((argumentRunner != NULL) && (argumentRunner->type != t_compound_statement) && (argumentRunner->type != t_asm))
	{
		switch (argumentRunner->type)
		{
		// looking at argument declarations
		case t_variable_declaration:
		{
			walkArgumentDeclaration(argumentRunner, block, &TACIndex, &tempNum, parsedFunc);
		}
		break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Malformed AST within function - expected function name and main scope only!\nMalformed node was of type %s with value [%s]\n", getTokenName(argumentRunner->type), argumentRunner->value);
		}
		argumentRunner = argumentRunner->sibling;
	}

	while (parsedFunc->argStackSize % STACK_ALIGN_BYTES)
	{
		parsedFunc->argStackSize++;
	}

	// if we are parsing a declaration which precedes a definition, there may be an existing declaration (prototype)
	if (existingFunc != NULL)
	{
		// check that if a prototype declaration exists, that our parsed declaration matches it exactly
		int mismatch = 0;

		if ((Type_Compare(&parsedFunc->returnType, &existingFunc->returnType)))
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
					(Type_Compare(&existingArg->type, &parsedArg->type)))
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

			char *existingReturnType = Type_GetName(&existingFunc->returnType);
			printf("\t%s %s(", existingReturnType, existingFunc->name);
			free(existingReturnType);
			for (int i = 0; i < existingFunc->arguments->size; i++)
			{
				struct VariableEntry *existingArg = existingFunc->arguments->data[i];

				char *argType = Type_GetName(&existingArg->type);
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
			char *parsedReturnType = Type_GetName(&parsedFunc->returnType);
			printf("\n\t%s %s(", parsedReturnType, parsedFunc->name);
			free(parsedReturnType);
			for (int i = 0; i < parsedFunc->arguments->size; i++)
			{
				struct VariableEntry *parsedArg = parsedFunc->arguments->data[i];

				char *argType = Type_GetName(&parsedArg->type);
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

			ErrorWithAST(ERROR_CODE, tree, " ");
		}
	}
	// free the basic block we used to walk declarations of arguments
	BasicBlock_free(block);

	struct AST *definition = argumentRunner;
	if (definition != NULL)
	{
		struct FunctionEntry *walkedFunction = NULL;
		if (existingFunc != NULL)
		{
			FunctionEntry_free(parsedFunc);
			existingFunc->isDefined = 1;
			walkFunctionDefinition(definition, existingFunc);
			walkedFunction = existingFunc;
		}
		else
		{
			parsedFunc->isDefined = 1;
			walkFunctionDefinition(definition, parsedFunc);
			walkedFunction = parsedFunc;
		}

		for (struct LinkedListNode *runner = walkedFunction->BasicBlockList->head; runner != NULL; runner = runner->next)
		{
			struct BasicBlock *b = runner->data;
			int prevTacIndex = -1;
			// iterate TAC lines backwards, because the last line with a duplicate number is actually the problem
			// (because we should post-increment the index to number recursive linearzations correctly)
			for (struct LinkedListNode *TACRunner = b->TACList->tail; TACRunner != NULL; TACRunner = TACRunner->prev)
			{
				struct TACLine *t = TACRunner->data;
				if (prevTacIndex != -1)
				{
					if ((t->index + 1) != prevTacIndex)
					{
						printBasicBlock(b, 0);
						char *printedTACLine = sPrintTACLine(t);
						ErrorAndExit(ERROR_INTERNAL, "TAC line allocated at %s:%d doesn't obey ordering - numbering goes from 0x%x to 0x%x:\n\t%s\n",
									 t->allocFile,
									 t->allocLine,
									 t->index,
									 prevTacIndex,
									 printedTACLine);
					}
				}
				prevTacIndex = t->index;
			}
		}
	}
}

void walkFunctionDefinition(struct AST *tree,
							struct FunctionEntry *fun)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkFunctionDefinition: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if ((tree->type != t_compound_statement) && (tree->type != t_asm))
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkFunctionDefinition!\n", getTokenName(tree->type));
	}

	int TACIndex = 0;
	int tempNum = 0;
	int labelNum = 1;
	struct BasicBlock *block = BasicBlock_new(0);
	Scope_addBasicBlock(fun->mainScope, block);

	if (tree->type == t_compound_statement)
	{
		walkScope(tree, block, fun->mainScope, &TACIndex, &tempNum, &labelNum, -1);
	}
	else
	{
		fun->isAsmFun = 1;
		walkAsmBlock(tree, block, fun->mainScope, &TACIndex, &tempNum);
	}
}

void walkClassDeclaration(struct AST *tree,
						  struct BasicBlock *block,
						  struct Scope *scope)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkClassDeclaration: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_class)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkClassDefinition!\n", getTokenName(tree->type));
	}
	int dummyNum = 0;

	struct ClassEntry *declaredClass = Scope_createClass(scope, tree->child->value);

	struct AST *classBody = tree->child->sibling;

	if (classBody->type != t_class_body)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Malformed AST seen in walkClassDefinition!\n");
	}

	struct AST *classBodyRunner = classBody->child;
	while (classBodyRunner != NULL)
	{
		switch (classBodyRunner->type)
		{
		case t_variable_declaration:
		{
			struct VariableEntry *declaredMember = walkVariableDeclaration(classBodyRunner, block, declaredClass->members, &dummyNum, &dummyNum, 0);
			Class_assignOffsetToMemberVariable(declaredClass, declaredMember);
		}
		break;

		default:
			ErrorWithAST(ERROR_INTERNAL, classBodyRunner, "Wrong AST (%s) seen in body of class definition!\n", getTokenName(classBodyRunner->type));
		}

		classBodyRunner = classBodyRunner->sibling;
	}
}

void walkStatement(struct AST *tree,
				   struct BasicBlock **blockP,
				   struct Scope *scope,
				   int *TACIndex,
				   int *tempNum,
				   int *labelNum,
				   int controlConvergesToLabel)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkStatement: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	switch (tree->type)
	{
	case t_variable_declaration:
		walkVariableDeclaration(tree, *blockP, scope, TACIndex, tempNum, 0);
		break;

	case t_extern:
		ErrorWithAST(ERROR_CODE, tree, "'extern' is only allowed at the global scope.\n");
		break;

	case t_assign:
		walkAssignment(tree, *blockP, scope, TACIndex, tempNum);
		break;

	case t_plus_equals:
	case t_minus_equals:
	case t_times_equals:
	case t_divide_equals:
	case t_modulo_equals:
	case t_bitwise_and_equals:
	case t_bitwise_or_equals:
	case t_bitwise_xor_equals:
	case t_lshift_equals:
	case t_rshift_equals:
		walkArithmeticAssignment(tree, *blockP, scope, TACIndex, tempNum);
		break;

	case t_while:
	{
		struct BasicBlock *afterWhileBlock = BasicBlock_new((*labelNum)++);
		walkWhileLoop(tree, *blockP, scope, TACIndex, tempNum, labelNum, afterWhileBlock->labelNum);
		*blockP = afterWhileBlock;
		Scope_addBasicBlock(scope, afterWhileBlock);
	}
	break;

	case t_if:
	{
		struct BasicBlock *afterIfBlock = BasicBlock_new((*labelNum)++);
		walkIfStatement(tree, *blockP, scope, TACIndex, tempNum, labelNum, afterIfBlock->labelNum);
		*blockP = afterIfBlock;
		Scope_addBasicBlock(scope, afterIfBlock);
	}
	break;

	case t_function_call:
		walkFunctionCall(tree, *blockP, scope, TACIndex, tempNum, NULL);
		break;

	// subscope
	case t_compound_statement:
	{
		// TODO: is there a bug here for simple scopes within code (not attached to if/while/etc... statements? TAC dump for the scopes test seems to indicate so?)
		struct Scope *subScope = Scope_createSubScope(scope);
		struct BasicBlock *afterSubScopeBlock = BasicBlock_new((*labelNum)++);
		walkScope(tree, *blockP, subScope, TACIndex, tempNum, labelNum, afterSubScopeBlock->labelNum);
		*blockP = afterSubScopeBlock;
		Scope_addBasicBlock(scope, afterSubScopeBlock);
	}
	break;

	case t_return:
	{
		struct TACLine *returnLine = newTACLine(*TACIndex, tt_return, tree);
		if (tree->child != NULL)
		{
			walkSubExpression(tree->child, *blockP, scope, TACIndex, tempNum, &returnLine->operands[0]);
		}

		returnLine->index = (*TACIndex)++;

		BasicBlock_append(*blockP, returnLine);
	}
	break;

	case t_asm:
		walkAsmBlock(tree, *blockP, scope, TACIndex, tempNum);
		break;

	default:
		ErrorWithAST(ERROR_INTERNAL, tree, "Unexpected AST type (%s - %s) seen in walkStatement!\n", getTokenName(tree->type), tree->value);
	}
}

void walkScope(struct AST *tree,
			   struct BasicBlock *block,
			   struct Scope *scope,
			   int *TACIndex,
			   int *tempNum,
			   int *labelNum,
			   int controlConvergesToLabel)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkScope: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}
	if (tree->type != t_compound_statement)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkScope!\n", getTokenName(tree->type));
	}

	struct AST *scopeRunner = tree->child;
	while (scopeRunner != NULL)
	{
		walkStatement(scopeRunner, &block, scope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
		scopeRunner = scopeRunner->sibling;
	}

	if (controlConvergesToLabel > 0)
	{
		struct TACLine *controlConvergeJmp = newTACLine((*TACIndex)++, tt_jmp, tree);
		controlConvergeJmp->operands[0].name.val = controlConvergesToLabel;
		BasicBlock_append(block, controlConvergeJmp);
	}
}

struct BasicBlock *walkLogicalOperator(struct AST *tree,
									   struct BasicBlock *block,
									   struct Scope *scope,
									   int *TACIndex,
									   int *tempNum,
									   int *labelNum,
									   int falseJumpLabelNum)
{
	switch (tree->type)
	{
	case t_logical_and:
	{
		// if either condition is false, immediately jump to the false label
		block = walkConditionCheck(tree->child, block, scope, TACIndex, tempNum, labelNum, falseJumpLabelNum);
		block = walkConditionCheck(tree->child->sibling, block, scope, TACIndex, tempNum, labelNum, falseJumpLabelNum);
	}
	break;

	case t_logical_or:
	{
		// this block will only be hit if the first condition comes back false
		struct BasicBlock *checkSecondConditionBlock = BasicBlock_new((*labelNum)++);
		Scope_addBasicBlock(scope, checkSecondConditionBlock);

		// this is the block in which execution will end up if the condition is true
		struct BasicBlock *trueBlock = BasicBlock_new((*labelNum)++);
		Scope_addBasicBlock(scope, trueBlock);
		block = walkConditionCheck(tree->child, block, scope, TACIndex, tempNum, labelNum, checkSecondConditionBlock->labelNum);

		// if we pass the first condition (don't jump to checkSecondConditionBlock), short-circuit directly to the true block
		struct TACLine *firstConditionTrueJump = newTACLine((*TACIndex)++, tt_jmp, tree->child);
		firstConditionTrueJump->operands[0].name.val = trueBlock->labelNum;
		BasicBlock_append(block, firstConditionTrueJump);

		// walk the second condition to checkSecondConditionBlock
		block = walkConditionCheck(tree->child->sibling, checkSecondConditionBlock, scope, TACIndex, tempNum, labelNum, falseJumpLabelNum);

		// jump from whatever block the second condition check ends up in (passing path) to our block
		// this ensures that regardless of which condition is true (first or second) execution always end up in the same block
		struct TACLine *secondConditionTrueJump = newTACLine((*TACIndex)++, tt_jmp, tree->child->sibling);
		secondConditionTrueJump->operands[0].name.val = trueBlock->labelNum;
		BasicBlock_append(block, secondConditionTrueJump);

		block = trueBlock;
	}
	break;

	case t_logical_not:
	{
		// walkConditionCheck already does everything we need it to
		// so just create a block representing the opposite of the condition we are testing
		// then, tell walkConditionCheck to go there if our subcondition is false (!subcondition is true)
		struct BasicBlock *inverseConditionBlock = BasicBlock_new((*labelNum)++);
		Scope_addBasicBlock(scope, inverseConditionBlock);

		block = walkConditionCheck(tree->child, block, scope, TACIndex, tempNum, labelNum, inverseConditionBlock->labelNum);

		// subcondition is true (!subcondition is false), then control flow should end up at the original conditionFalseJump destination
		struct TACLine *conditionFalseJump = newTACLine((*TACIndex)++, tt_jmp, tree->child);
		conditionFalseJump->operands[0].name.val = falseJumpLabelNum;
		BasicBlock_append(block, conditionFalseJump);

		// return the tricky block we created to be jumped to when our subcondition is false, or that the condition we are linearizing at this level is true
		block = inverseConditionBlock;
	}
	break;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Logical operator %s (%s) not supported yet\n",
					 getTokenName(tree->type),
					 tree->value);
	}

	return block;
}

struct BasicBlock *walkConditionCheck(struct AST *tree,
									  struct BasicBlock *block,
									  struct Scope *scope,
									  int *TACIndex,
									  int *tempNum,
									  int *labelNum,
									  int falseJumpLabelNum)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkConditionCheck: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	struct TACLine *condFalseJump = newTACLine(*TACIndex, tt_jmp, tree);
	condFalseJump->operands[0].name.val = falseJumpLabelNum;

	// switch once to decide the jump type
	switch (tree->type)
	{
	case t_equals:
		condFalseJump->operation = tt_bne;
		break;

	case t_not_equals:
		condFalseJump->operation = tt_beq;
		break;

	case t_less_than:
		condFalseJump->operation = tt_bgeu;
		break;

	case t_greater_than:
		condFalseJump->operation = tt_bleu;
		break;

	case t_less_than_equals:
		condFalseJump->operation = tt_bgtu;
		break;

	case t_greater_than_equals:
		condFalseJump->operation = tt_bltu;
		break;

	case t_logical_and:
	case t_logical_or:
	case t_logical_not:
		block = walkLogicalOperator(tree, block, scope, TACIndex, tempNum, labelNum, falseJumpLabelNum);
		break;

	default:
		condFalseJump->operation = tt_bne;
		break;
	}

	// switch a second time to actually walk the condition
	switch (tree->type)
	{
	// arithmetic comparisons
	case t_equals:
	case t_not_equals:
	case t_less_than:
	case t_greater_than:
	case t_less_than_equals:
	case t_greater_than_equals:
		// standard operators (==, !=, <, >, <=, >=)
		{
			switch (tree->child->type)
			{
			case t_logical_and:
			case t_logical_or:
			case t_logical_not:
				ErrorWithAST(ERROR_CODE, tree->child, "Use of comparison operators on results of logical operators is not supported!\n");
				break;

			default:
				walkSubExpression(tree->child, block, scope, TACIndex, tempNum, &condFalseJump->operands[1]);
				break;
			}

			switch (tree->child->sibling->type)
			{
			case t_logical_and:
			case t_logical_or:
			case t_logical_not:
				ErrorWithAST(ERROR_CODE, tree->child->sibling, "Use of comparison operators on results of logical operators is not supported!\n");
				break;

			default:
				walkSubExpression(tree->child->sibling, block, scope, TACIndex, tempNum, &condFalseJump->operands[2]);
				break;
			}
		}
		break;

	case t_logical_and:
	case t_logical_or:
	case t_logical_not:
		free(condFalseJump);
		condFalseJump = NULL;
		break;

	case t_identifier:
	case t_add:
	case t_subtract:
	case t_multiply:
	case t_divide:
	case t_modulo:
	case t_lshift:
	case t_rshift:
	case t_bitwise_and:
	case t_bitwise_or:
	case t_bitwise_not:
	case t_bitwise_xor:
	case t_dereference:
	case t_address_of:
	case t_cast:
	case t_dot:
	case t_arrow:
	case t_function_call:
	{
		condFalseJump->operation = tt_beq;
		walkSubExpression(tree, block, scope, TACIndex, tempNum, &condFalseJump->operands[1]);

		condFalseJump->operands[2].type.basicType = vt_u8;
		condFalseJump->operands[2].permutation = vp_literal;
		condFalseJump->operands[2].name.str = "0";
	}
	break;

	default:
	{
		ErrorAndExit(ERROR_INTERNAL, "Comparison operator %s (%s) not supported yet\n",
					 getTokenName(tree->type),
					 tree->value);
	}
	break;
	}

	if (condFalseJump != NULL)
	{
		condFalseJump->index = (*TACIndex)++;
		BasicBlock_append(block, condFalseJump);
	}
	return block;
}

void walkWhileLoop(struct AST *tree,
				   struct BasicBlock *block,
				   struct Scope *scope,
				   int *TACIndex,
				   int *tempNum,
				   int *labelNum,
				   int controlConvergesToLabel)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkWhileLoop: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}
	if (tree->type != t_while)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkWhileLoop!\n", getTokenName(tree->type));
	}

	struct BasicBlock *beforeWhileBlock = block;

	struct TACLine *enterWhileJump = newTACLine((*TACIndex)++, tt_jmp, tree);
	enterWhileJump->operands[0].name.val = *labelNum;
	BasicBlock_append(beforeWhileBlock, enterWhileJump);

	// create a subscope from which we will work
	struct Scope *whileScope = Scope_createSubScope(scope);
	struct BasicBlock *whileBlock = BasicBlock_new((*labelNum)++);
	Scope_addBasicBlock(whileScope, whileBlock);

	struct TACLine *whileDo = newTACLine((*TACIndex)++, tt_do, tree);
	BasicBlock_append(whileBlock, whileDo);

	whileBlock = walkConditionCheck(tree->child, whileBlock, whileScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);

	int endWhileLabel = (*labelNum)++;

	struct AST *whileBody = tree->child->sibling;
	if (whileBody->type == t_compound_statement)
	{
		walkScope(whileBody, whileBlock, whileScope, TACIndex, tempNum, labelNum, endWhileLabel);
	}
	else
	{
		walkStatement(whileBody, &whileBlock, whileScope, TACIndex, tempNum, labelNum, endWhileLabel);
	}

	struct TACLine *whileLoopJump = newTACLine((*TACIndex)++, tt_jmp, tree);
	whileLoopJump->operands[0].name.val = enterWhileJump->operands[0].name.val;

	block = BasicBlock_new(endWhileLabel);
	Scope_addBasicBlock(scope, block);

	struct TACLine *whileEndDo = newTACLine((*TACIndex)++, tt_enddo, tree);
	BasicBlock_append(block, whileLoopJump);
	BasicBlock_append(block, whileEndDo);
}

void walkIfStatement(struct AST *tree,
					 struct BasicBlock *block,
					 struct Scope *scope,
					 int *TACIndex,
					 int *tempNum,
					 int *labelNum,
					 int controlConvergesToLabel)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkIfStatement: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_if)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkIfStatement!\n", getTokenName(tree->type));
	}

	// if we have an else block
	if (tree->child->sibling->sibling != NULL)
	{
		int elseLabel = (*labelNum)++;
		block = walkConditionCheck(tree->child, block, scope, TACIndex, tempNum, labelNum, elseLabel);

		struct Scope *ifScope = Scope_createSubScope(scope);
		struct BasicBlock *ifBlock = BasicBlock_new((*labelNum)++);
		Scope_addBasicBlock(ifScope, ifBlock);

		struct TACLine *enterIfJump = newTACLine((*TACIndex)++, tt_jmp, tree);
		enterIfJump->operands[0].name.val = ifBlock->labelNum;
		BasicBlock_append(block, enterIfJump);

		struct AST *ifBody = tree->child->sibling;
		if (ifBody->type == t_compound_statement)
		{
			walkScope(ifBody, ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
		}
		else
		{
			walkStatement(ifBody, &ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
		}

		struct Scope *elseScope = Scope_createSubScope(scope);
		struct BasicBlock *elseBlock = BasicBlock_new(elseLabel);
		Scope_addBasicBlock(elseScope, elseBlock);

		struct AST *elseBody = tree->child->sibling->sibling;
		if (elseBody->type == t_compound_statement)
		{
			walkScope(elseBody, elseBlock, elseScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
		}
		else
		{
			walkStatement(elseBody, &elseBlock, elseScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
		}
	}
	// no else block
	else
	{
		block = walkConditionCheck(tree->child, block, scope, TACIndex, tempNum, labelNum, controlConvergesToLabel);

		struct Scope *ifScope = Scope_createSubScope(scope);
		struct BasicBlock *ifBlock = BasicBlock_new((*labelNum)++);
		Scope_addBasicBlock(ifScope, ifBlock);

		struct TACLine *enterIfJump = newTACLine((*TACIndex)++, tt_jmp, tree);
		enterIfJump->operands[0].name.val = ifBlock->labelNum;
		BasicBlock_append(block, enterIfJump);

		struct AST *ifBody = tree->child->sibling;
		if (ifBody->type == t_compound_statement)
		{
			walkScope(ifBody, ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
		}
		else
		{
			walkStatement(ifBody, &ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
		}
	}
}

void walkDotOperatorAssignment(struct AST *tree,
							   struct BasicBlock *block,
							   struct Scope *scope,
							   int *TACIndex,
							   int *tempNum,
							   struct TACLine *wipAssignment,
							   struct TACOperand *assignedValue)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkDotOperatorAssignment: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_dot)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkDotOperatorAssignment!\n", getTokenName(tree->type));
	}

	struct AST *class = tree->child;
	// the RHS is what member we are accessing
	struct AST *member = tree->child->sibling;

	if (member->type != t_identifier)
	{
		ErrorWithAST(ERROR_CODE, member, "Expected identifier on RHS of dot operator, got %s (%s) instead!\n", tree->value, getTokenName(tree->type));
	}

	wipAssignment->operation = tt_store_off;
	switch (class->type)
	{

	case t_dereference:
		ErrorWithAST(ERROR_CODE, class, "Use of the dot operator assignment on dereferenced values is not supported\nAssign using object->member instead of (*object).member\n");

	case t_array_index:
	{
		// let walkArrayRef do the heavy lifting for us
		struct TACLine *arrayRefToDot = walkArrayRef(class, block, scope, TACIndex, tempNum);

		// before we convert our array ref to an LEA to get the address of the class we're dotting, check to make sure everything is good
		struct Type nonDecayedType = *TAC_GetTypeOfOperand(arrayRefToDot, 1);
		nonDecayedType.arraySize = 0;
		checkAccessedClassForDot(tree, scope, &nonDecayedType);

		// now that we know we are dotting something valid, we will just use the array reference as an address calculation for the base of whatever we're dotting
		convertArrayRefLoadToLea(arrayRefToDot);

		// copy the TAC operand containing the address on which we will dot
		copyTACOperandDecayArrays(&wipAssignment->operands[0], &arrayRefToDot->operands[0]);
	}
	break;

	case t_identifier:
	{
		// construct a TAC line responsible for figuring out the address of what we're dotting since it's not a pointer already
		struct TACLine *getAddressForDot = newTACLine(*TACIndex, tt_addrof, tree);
		getAddressForDot->operands[0].permutation = vp_temp;
		getAddressForDot->operands[0].name.str = TempList_Get(temps, (*tempNum)++);

		// walk the LHS of the dot operator using walkSubExpression
		walkSubExpression(class, block, scope, TACIndex, tempNum, &getAddressForDot->operands[1]);

		// look up the identifier by name, make sure it's not a pointer (ensure a dot operator is valid on it)
		checkAccessedClassForDot(class, scope, &getAddressForDot->operands[1].type);

		// copy the operand from [1] to [0] for the implicit address-of, incrementing the indirection level
		copyTACOperandTypeDecayArrays(&getAddressForDot->operands[0], &getAddressForDot->operands[1]);
		TAC_GetTypeOfOperand(getAddressForDot, 0)->indirectionLevel++;

		// assign TAC index and append the address-of before the actual assignment
		getAddressForDot->index = (*TACIndex)++;
		BasicBlock_append(block, getAddressForDot);

		// copy the TAC operands for the direct part of the assignment
		copyTACOperandDecayArrays(&wipAssignment->operands[0], &getAddressForDot->operands[0]);
	}
	break;

	case t_arrow:
	case t_dot:
	{
		struct TACLine *memberAccess = walkMemberAccess(class, block, scope, TACIndex, tempNum, &wipAssignment->operands[0], 0);
		struct Type *readType = TAC_GetTypeOfOperand(memberAccess, 0);
		checkAccessedClassForDot(class, scope, readType);

		// if our arrow or dot operator results in getting a full class instead of a pointer
		if ((readType->basicType == vt_class) &&
			((readType->indirectionLevel == 0) &&
			 (readType->arraySize == 0)))
		{
			// retroatcively convert the read to an LEA so we have the address we're about to write to
			memberAccess->operation = tt_lea_off;
			TAC_GetTypeOfOperand(memberAccess, 0)->indirectionLevel++;
			TAC_GetTypeOfOperand(memberAccess, 1)->indirectionLevel++;
			TAC_GetTypeOfOperand(wipAssignment, 0)->indirectionLevel++;
		}
	}
	break;

	default:
		ErrorAndExit(ERROR_CODE, "Unecpected token %s (%s) seen on LHS of dot operator which itself is LHS of assignment!\n\tExpected identifier, dot operator, or arrow operator only!\n", class->value, getTokenName(class->type));
	}

	// check to see that what we expect to treat as our class pointer is actually a class
	struct ClassEntry *writtenClass = Scope_lookupClassByType(scope, TAC_GetTypeOfOperand(wipAssignment, 0));

	struct ClassMemberOffset *accessedMember = Class_lookupMemberVariable(writtenClass, member);

	wipAssignment->operands[1].type.basicType = vt_u32;
	wipAssignment->operands[1].permutation = vp_literal;
	wipAssignment->operands[1].name.val = accessedMember->offset;

	wipAssignment->operands[2] = *assignedValue;

	// cast the class pointer to the type we are actually reading out of the class
	wipAssignment->operands[0].castAsType = accessedMember->variable->type;
}

void walkArrowOperatorAssignment(struct AST *tree,
								 struct BasicBlock *block,
								 struct Scope *scope,
								 int *TACIndex,
								 int *tempNum,
								 struct TACLine *wipAssignment,
								 struct TACOperand *assignedValue)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkArrowOperatorAssignment: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_arrow)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkArrowOperatorAssignment!\n", getTokenName(tree->type));
	}

	struct AST *class = tree->child;
	// the RHS is what member we are accessing
	struct AST *member = tree->child->sibling;

	if (member->type != t_identifier)
	{
		ErrorAndExit(ERROR_CODE, "Expected identifier on RHS of dot operator, got %s (%s) instead!\n", tree->value, getTokenName(tree->type));
	}

	wipAssignment->operation = tt_store_off;
	switch (class->type)
	{
	case t_array_index:
	{
		// let walkArrayRef do the heavy lifting for us
		struct TACLine *arrayRefToArrow = walkArrayRef(class, block, scope, TACIndex, tempNum);

		// before we convert our array ref to an LEA to get the address of the class we're dotting, check to make sure everything is good
		struct Type nonDecayedType = *TAC_GetTypeOfOperand(arrayRefToArrow, 1);
		nonDecayedType.arraySize = 0;
		checkAccessedClassForArrow(tree, scope, &nonDecayedType);

		// now that we know we are dotting something valid, we will just use the array reference as an address calculation for the base of whatever we're dotting
		convertArrayRefLoadToLea(arrayRefToArrow);

		// copy the TAC operand containing the address on which we will dot
		copyTACOperandDecayArrays(&wipAssignment->operands[0], &arrayRefToArrow->operands[0]);
	}
	break;

	case t_identifier:
	{
		walkSubExpression(class, block, scope, TACIndex, tempNum, &wipAssignment->operands[0]);
		struct VariableEntry *classVariable = Scope_lookupVar(scope, class);

		checkAccessedClassForArrow(class, scope, &classVariable->type);
	}
	break;

	// nothing special to do for these, just treat as subexpression and check the ultimate type of the subexpression to ensure we can actually arrow it
	case t_dereference:
	case t_function_call:
	{
		walkSubExpression(class, block, scope, TACIndex, tempNum, &wipAssignment->operands[0]);

		checkAccessedClassForArrow(class, scope, TAC_GetTypeOfOperand(wipAssignment, 0));
	}
	break;

	case t_arrow:
	case t_dot:
	{
		struct TACLine *memberAccess = walkMemberAccess(class, block, scope, TACIndex, tempNum, &wipAssignment->operands[0], 0);
		struct Type *readType = TAC_GetTypeOfOperand(memberAccess, 0);

		if ((readType->indirectionLevel != 1))
		{
			char *typeName = Type_GetName(readType);
			ErrorWithAST(ERROR_CODE, class, "Can't use dot operator on non-indirect type %s\n", typeName);
		}

		checkAccessedClassForArrow(class, scope, readType);

		// if our arrow or dot operator results in getting a full class instead of a pointer
		if ((readType->basicType == vt_class) &&
			((readType->indirectionLevel == 0) &&
			 (readType->arraySize == 0)))
		{
			// retroatcively convert the read to an LEA so we have the address we're about to write to
			memberAccess->operation = tt_lea_off;
			TAC_GetTypeOfOperand(memberAccess, 0)->indirectionLevel++;
			TAC_GetTypeOfOperand(memberAccess, 1)->indirectionLevel++;
			TAC_GetTypeOfOperand(wipAssignment, 0)->indirectionLevel++;
		}
	}
	break;

	default:
		ErrorAndExit(ERROR_CODE, "Unecpected token %s (%s) seen on LHS of dot operator which itself is LHS of assignment!\n\tExpected identifier, dot operator, or arrow operator only!\n", class->value, getTokenName(class->type));
	}

	// check to see that what we expect to treat as our class pointer is actually a class
	struct ClassEntry *writtenClass = Scope_lookupClassByType(scope, TAC_GetTypeOfOperand(wipAssignment, 0));

	struct ClassMemberOffset *accessedMember = Class_lookupMemberVariable(writtenClass, member);

	wipAssignment->operands[1].type.basicType = vt_u32;
	wipAssignment->operands[1].permutation = vp_literal;
	wipAssignment->operands[1].name.val = accessedMember->offset;

	wipAssignment->operands[2] = *assignedValue;

	// cast the class pointer to the type we are actually reading out of the class
	wipAssignment->operands[0].castAsType = accessedMember->variable->type;
}

void walkAssignment(struct AST *tree,
					struct BasicBlock *block,
					struct Scope *scope,
					int *TACIndex,
					int *tempNum)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkAssignment: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_assign)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkAssignment!\n", getTokenName(tree->type));
	}

	struct AST *lhs = tree->child;
	struct AST *rhs = tree->child->sibling;

	// don't increment the index until after we deal with nested expressions
	struct TACLine *assignment = newTACLine((*TACIndex), tt_assign, tree);

	struct TACOperand assignedValue;
	memset(&assignedValue, 0, sizeof(struct TACOperand));

	// walk the RHS of the assignment as a subexpression and save the operand for later
	walkSubExpression(rhs, block, scope, TACIndex, tempNum, &assignedValue);

	struct VariableEntry *assignedVariable = NULL;
	switch (lhs->type)
	{
	case t_variable_declaration:
		assignedVariable = walkVariableDeclaration(lhs, block, scope, TACIndex, tempNum, 0);
		populateTACOperandFromVariable(&assignment->operands[0], assignedVariable);
		assignment->operands[1] = assignedValue;

		if (assignedVariable->type.arraySize > 0)
		{
			char *arrayName = Type_GetName(&assignedVariable->type);
			ErrorWithAST(ERROR_CODE, tree, "Assignment to local array variable %s with type %s is not allowed!\n", assignedVariable->name, arrayName);
		}
		break;

	case t_identifier:
		assignedVariable = Scope_lookupVar(scope, lhs);
		populateTACOperandFromVariable(&assignment->operands[0], assignedVariable);
		assignment->operands[1] = assignedValue;
		break;

	// TODO: generate optimized addressing modes for arithmetic
	case t_dereference:
	{
		struct AST *writtenPointer = lhs->child;
		switch (writtenPointer->type)
		{
		case t_add:
		case t_subtract:
			walkPointerArithmetic(writtenPointer, block, scope, TACIndex, tempNum, &assignment->operands[0]);
			break;

		default:
			walkSubExpression(writtenPointer, block, scope, TACIndex, tempNum, &assignment->operands[0]);
			break;
		}
		assignment->operation = tt_store;
		assignment->operands[1] = assignedValue;
	}
	break;

	case t_array_index:
	{
		struct AST *arrayBase = lhs->child;
		struct AST *arrayIndex = lhs->child->sibling;
		struct Type *arrayType = NULL;

		assignment->operation = tt_store_arr;

		// if our array is simply an identifier, do a standard lookup to find it
		if (arrayBase->type == t_identifier)
		{
			struct VariableEntry *arrayVariable = Scope_lookupVar(scope, arrayBase);
			arrayType = &arrayVariable->type;
			if ((arrayType->indirectionLevel < 1) &&
				(arrayType->arraySize == 0))
			{
				ErrorWithAST(ERROR_CODE, arrayBase, "Use of non-pointer variable %s as array!\n", arrayBase->value);
			}
			populateTACOperandFromVariable(&assignment->operands[0], arrayVariable);
		}
		// otherwise, our array base comes from some sort of subexpression
		else
		{
			walkSubExpression(arrayBase, block, scope, TACIndex, tempNum, &assignment->operands[0]);
			arrayType = TAC_GetTypeOfOperand(assignment, 0);

			if ((arrayType->indirectionLevel < 1) &&
				(arrayType->arraySize == 0))
			{
				ErrorWithAST(ERROR_CODE, arrayBase, "Use of non-pointer expression as array!\n");
			}
		}

		assignment->operands[2].permutation = vp_literal;
		assignment->operands[2].type.indirectionLevel = 0;
		assignment->operands[2].type.basicType = vt_u8;
		struct Type decayedType;
		copyTypeDecayArrays(&decayedType, arrayType);
		assignment->operands[2].name.val = alignSize(Scope_getSizeOfDereferencedType(scope, &decayedType));

		walkSubExpression(arrayIndex, block, scope, TACIndex, tempNum, &assignment->operands[1]);

		assignment->operands[3] = assignedValue;
	}
	break;

	case t_dot:
		walkDotOperatorAssignment(lhs, block, scope, TACIndex, tempNum, assignment, &assignedValue);
		break;

	case t_arrow:
		walkArrowOperatorAssignment(lhs, block, scope, TACIndex, tempNum, assignment, &assignedValue);
		break;

	default:
		ErrorWithAST(ERROR_INTERNAL, lhs, "Unexpected AST (%s) seen in walkAssignment!\n", lhs->value);
		break;
	}

	if (assignment != NULL)
	{
		assignment->index = (*TACIndex)++;
		BasicBlock_append(block, assignment);
	}
}

void walkArithmeticAssignment(struct AST *tree,
							  struct BasicBlock *block,
							  struct Scope *scope,
							  int *TACIndex,
							  int *tempNum)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkArithmeticAssignment: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	struct AST fakeArith = *tree;
	switch (tree->type)
	{
	case t_plus_equals:
		fakeArith.type = t_add;
		fakeArith.value = "+";
		break;

	case t_minus_equals:
		fakeArith.type = t_subtract;
		fakeArith.value = "-";
		break;

	case t_times_equals:
		fakeArith.type = t_multiply;
		fakeArith.value = "*";
		break;

	case t_divide_equals:
		fakeArith.type = t_divide;
		fakeArith.value = "/";
		break;

	case t_modulo_equals:
		fakeArith.type = t_modulo;
		fakeArith.value = "%";
		break;

	case t_bitwise_and_equals:
		fakeArith.type = t_bitwise_and;
		fakeArith.value = "&";
		break;

	case t_bitwise_or_equals:
		fakeArith.type = t_bitwise_or;
		fakeArith.value = "|";
		break;

	case t_bitwise_xor_equals:
		fakeArith.type = t_bitwise_xor;
		fakeArith.value = "^";
		break;

	case t_lshift_equals:
		fakeArith.type = t_lshift;
		fakeArith.value = "<<";
		break;

	case t_rshift_equals:
		fakeArith.type = t_rshift;
		fakeArith.value = ">>";
		break;

	default:
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkArithmeticAssignment!\n", getTokenName(tree->type));
	}

	// our fake arithmetic ast will have the child of the arithmetic assignment operator
	// this effectively duplicates the LHS of the assignment to the first operand of the arithmetic operator
	struct AST *lhs = tree->child;
	fakeArith.child = lhs;

	struct AST fakelhs = *lhs;
	fakelhs.sibling = &fakeArith;

	struct AST fakeAssignment = *tree;
	fakeAssignment.value = "=";
	fakeAssignment.type = t_assign;

	fakeAssignment.child = &fakelhs;

	walkAssignment(&fakeAssignment, block, scope, TACIndex, tempNum);
}

struct TACOperand *walkBitwiseNot(struct AST *tree,
								  struct BasicBlock *block,
								  struct Scope *scope,
								  int *TACIndex,
								  int *tempNum)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkBitwiseNot: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_bitwise_not)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkBitwiseNot!\n", getTokenName(tree->type));
	}

	// generically set to tt_add, we will actually set the operation within switch cases
	struct TACLine *bitwiseNotLine = newTACLine(*TACIndex, tt_bitwise_not, tree);

	bitwiseNotLine->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
	bitwiseNotLine->operands[0].permutation = vp_temp;

	walkSubExpression(tree->child, block, scope, TACIndex, tempNum, &bitwiseNotLine->operands[1]);
	copyTACOperandTypeDecayArrays(&bitwiseNotLine->operands[0], &bitwiseNotLine->operands[1]);

	struct TACOperand *operandA = &bitwiseNotLine->operands[1];
	if ((operandA->type.indirectionLevel > 0))
	{
		ErrorWithAST(ERROR_CODE, tree, "Bitwise arithmetic on pointers is not allowed!\n");
	}

	bitwiseNotLine->index = (*TACIndex)++;
	BasicBlock_append(block, bitwiseNotLine);

	return &bitwiseNotLine->operands[0];
}

void walkSubExpression(struct AST *tree,
					   struct BasicBlock *block,
					   struct Scope *scope,
					   int *TACIndex,
					   int *tempNum,
					   struct TACOperand *destinationOperand)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkSubExpression: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	switch (tree->type)
	{
		// variable read
	case t_identifier:
	{
		struct VariableEntry *readVariable = Scope_lookupVar(scope, tree);
		populateTACOperandFromVariable(destinationOperand, readVariable);
	}
	break;

	case t_constant:
		destinationOperand->name.str = tree->value;
		destinationOperand->type.basicType = selectVariableTypeForLiteral(tree->value);
		destinationOperand->permutation = vp_literal;
		break;

	case t_char_literal:
	{
		int literalLen = strlen(tree->value);
		char literalAsNumber[8];
		if (literalLen == 1)
		{
			sprintf(literalAsNumber, "%d", tree->value[0]);
		}
		else if (literalLen == 2)
		{
			if (tree->value[0] != '\\')
			{
				ErrorWithAST(ERROR_INTERNAL, tree, "Saw t_char_literal with escape character value of %s - expected first char to be \\!\n", tree->value);
			}

			char escapeCharValue = 0;

			switch (tree->value[1])
			{
			case 'a':
				escapeCharValue = '\a';
				break;

			case 'b':
				escapeCharValue = '\b';
				break;

			case 'n':
				escapeCharValue = '\n';
				break;

			case 'r':
				escapeCharValue = '\r';
				break;

			case 't':
				escapeCharValue = '\t';
				break;

			case '\\':
				escapeCharValue = '\\';
				break;

			case '\'':
				escapeCharValue = '\'';
				break;

			case '\"':
				escapeCharValue = '\"';
				break;
			}

			sprintf(literalAsNumber, "%d", escapeCharValue);
		}
		else
		{
			ErrorWithAST(ERROR_INTERNAL, tree, "Saw t_char_literal with string length of %d (value '%s')!\n", literalLen, tree->value);
		}

		destinationOperand->name.str = Dictionary_LookupOrInsert(parseDict, literalAsNumber);
		destinationOperand->type.basicType = vt_u8;
		destinationOperand->permutation = vp_literal;
	}
	break;

	case t_string_literal:
		walkStringLiteral(tree, block, scope, destinationOperand);
		break;

	case t_function_call:
		walkFunctionCall(tree, block, scope, TACIndex, tempNum, destinationOperand);
		break;

	case t_dot:
	case t_arrow:
	{
		walkMemberAccess(tree, block, scope, TACIndex, tempNum, destinationOperand, 0);
	}
	break;

	case t_add:
	case t_subtract:
	case t_multiply:
	case t_divide:
	case t_modulo:
	case t_lshift:
	case t_rshift:
	case t_less_than:
	case t_greater_than:
	case t_less_than_equals:
	case t_greater_than_equals:
	case t_bitwise_or:
	case t_bitwise_xor:
	{
		struct TACOperand *expressionResult = walkExpression(tree, block, scope, TACIndex, tempNum);
		*destinationOperand = *expressionResult;
	}
	break;

	case t_bitwise_not:
	{
		struct TACOperand *bitwiseNotResult = walkBitwiseNot(tree, block, scope, TACIndex, tempNum);
		*destinationOperand = *bitwiseNotResult;
	}
	break;

	// array reference
	case t_array_index:
	{
		struct TACLine *arrayRefLine = walkArrayRef(tree, block, scope, TACIndex, tempNum);
		*destinationOperand = arrayRefLine->operands[0];
	}
	break;

	case t_dereference:
	{
		struct TACOperand *dereferenceResult = walkDereference(tree, block, scope, TACIndex, tempNum);
		*destinationOperand = *dereferenceResult;
	}
	break;

	case t_address_of:
	{
		struct TACOperand *addrOfResult = walkAddrOf(tree, block, scope, TACIndex, tempNum);
		*destinationOperand = *addrOfResult;
	}
	break;

	case t_bitwise_and:
	{
		struct TACOperand *expressionResult = walkExpression(tree, block, scope, TACIndex, tempNum);
		*destinationOperand = *expressionResult;
	}
	break;

	case t_cast:
	{
		struct TACOperand expressionResult;

		// walk the right child of the cast, the subexpression we are casting
		walkSubExpression(tree->child->sibling, block, scope, TACIndex, tempNum, &expressionResult);
		// set the result's cast as type based on the child of the cast, the type we are casting to
		walkTypeName(tree->child, scope, &expressionResult.castAsType);

		if ((expressionResult.castAsType.basicType == vt_class) &&
			(expressionResult.castAsType.indirectionLevel == 0))
		{
			char *castToType = Type_GetName(&expressionResult.castAsType);
			ErrorWithAST(ERROR_CODE, tree->child, "Casting to a class (%s) is not allowed!", castToType);
		}

		// If necessary, lop bits off the big end of the value with an explicit bitwise and operation, storing to an intermediate temp
		if (Type_CompareAllowImplicitWidening(&expressionResult.castAsType, &destinationOperand->type) && (expressionResult.castAsType.indirectionLevel == 0))
		{
			struct TACLine *castBitManipulation = newTACLine((*TACIndex)++, tt_bitwise_and, tree);

			// RHS of the assignment is whatever we are storing, what is being cast
			castBitManipulation->operands[1] = expressionResult;

			// construct the bit pattern we will use in order to properly mask off the extra bits (TODO: will not hold for unsigned types)
			castBitManipulation->operands[2].permutation = vp_literal;
			castBitManipulation->operands[2].type.basicType = vt_u32;
			char literalAndValue[32];

			// manually generate a string with an 'F' hex digit for each 4 bits in the mask
			sprintf(literalAndValue, "0x");
			int maskBitWidth = (8 * Scope_getSizeOfType(scope, TAC_GetTypeOfOperand(castBitManipulation, 1)));
			int maskBit = 0;
			for (maskBit = 0; maskBit < maskBitWidth; maskBit += 4)
			{
				literalAndValue[2 + (maskBit / 4)] = 'F';
				literalAndValue[3 + (maskBit / 4)] = '\0';
			}

			castBitManipulation->operands[2].name.str = Dictionary_LookupOrInsert(parseDict, literalAndValue);

			// destination of our bit manipulation is a temporary variable with the type to which we are casting
			castBitManipulation->operands[0].permutation = vp_temp;
			castBitManipulation->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
			castBitManipulation->operands[0].type = *TAC_GetTypeOfOperand(castBitManipulation, 1);

			// attach our bit manipulation operation to the end of the basic block
			BasicBlock_append(block, castBitManipulation);
			// set the destination operation of this subexpression to read the manipulated value we just wrote
			*destinationOperand = castBitManipulation->operands[0];
		}
		else
		{
			// no bit manipulation required, simply set the destination operand to the result of the casted subexpression (with cast as type set by us)
			*destinationOperand = expressionResult;
		}
	}
	break;

	case t_sizeof:
		walkSizeof(tree, block, scope, destinationOperand);
		break;

	default:
		ErrorWithAST(ERROR_INTERNAL, tree, "Incorrect AST type (%s) seen while linearizing subexpression!\n", getTokenName(tree->type));
		break;
	}
}

void walkFunctionCall(struct AST *tree,
					  struct BasicBlock *block,
					  struct Scope *scope,
					  int *TACIndex,
					  int *tempNum,
					  struct TACOperand *destinationOperand)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkFunctionCall: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_function_call)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkFunctionCall!\n", getTokenName(tree->type));
	}

	scope->parentFunction->callsOtherFunction = 1;

	struct FunctionEntry *calledFunction = Scope_lookupFun(scope, tree->child);

	if ((destinationOperand != NULL) &&
		((calledFunction->returnType.basicType == vt_null) &&
		 (calledFunction->returnType.indirectionLevel == 0)))
	{
		char *typeName = Type_GetName(&calledFunction->returnType);
		ErrorWithAST(ERROR_CODE, tree, "Attempt to use return value of function %s (returning %s)\n", calledFunction->name, typeName);
	}

	struct Stack *argumentTrees = Stack_New();
	struct AST *argumentRunner = tree->child->sibling;
	while (argumentRunner != NULL)
	{
		Stack_Push(argumentTrees, argumentRunner);
		argumentRunner = argumentRunner->sibling;
	}

	struct Stack *argumentPushes = Stack_New();
	if (argumentTrees->size != calledFunction->arguments->size)
	{
		ErrorWithAST(ERROR_CODE, tree,
					 "Error in call to function %s - expected %d arguments, saw %d!\n",
					 calledFunction->name,
					 calledFunction->arguments->size,
					 argumentTrees->size);
	}

	int argIndex = calledFunction->arguments->size - 1;
	while (argumentTrees->size > 0)
	{
		struct AST *pushedArgument = Stack_Pop(argumentTrees);
		struct TACLine *push = newTACLine(*TACIndex, tt_stack_store, pushedArgument);
		Stack_Push(argumentPushes, push);
		walkSubExpression(pushedArgument, block, scope, TACIndex, tempNum, &push->operands[0]);


		struct VariableEntry *expectedArgument = calledFunction->arguments->data[argIndex];

		if (Type_CompareAllowImplicitWidening(TAC_GetTypeOfOperand(push, 0), &expectedArgument->type))
		{
			ErrorWithAST(ERROR_CODE, pushedArgument,
						 "Error in argument %s passed to function %s!\n\tExpected %s, got %s\n",
						 expectedArgument->name,
						 calledFunction->name,
						 Type_GetName(&expectedArgument->type),
						 Type_GetName(TAC_GetTypeOfOperand(push, 0)));
		}

		struct TACOperand decayed;
		copyTACOperandDecayArrays(&decayed, &push->operands[0]);

		// allow us to automatically widen
		if (Scope_getSizeOfType(scope, TACOperand_GetType(&decayed)) <= Scope_getSizeOfType(scope, &expectedArgument->type))
		{
			push->operands[0].castAsType = expectedArgument->type;
		}
		else
		{
			char *convertFromType = Type_GetName(&push->operands[0].type);
			char *convertToType = Type_GetName(&expectedArgument->type);
			ErrorWithAST(ERROR_CODE, pushedArgument,
						 "Potential narrowing conversion passed to argument %s of function %s\n\tConversion from %s to %s\n",
						 expectedArgument->name,
						 calledFunction->name,
						 convertFromType,
						 convertToType);
		}

		push->operands[1].name.val = expectedArgument->stackOffset;
		push->operands[1].type.basicType = vt_u64;
		push->operands[1].permutation = vp_literal;
		
		argIndex--;
	}
	Stack_Free(argumentTrees);

	if (calledFunction->arguments->size > 0)
	{
		struct TACLine *reserveStackSpaceForCall = newTACLine((*TACIndex)++, tt_stack_reserve, tree->child);
		reserveStackSpaceForCall->operands[0].name.val = calledFunction->argStackSize;
		BasicBlock_append(block, reserveStackSpaceForCall);
	}

	while (argumentPushes->size > 0)
	{
		struct TACLine *push = Stack_Pop(argumentPushes);
		push->index = (*TACIndex)++;
		BasicBlock_append(block, push);
	}
	Stack_Free(argumentPushes);

	struct TACLine *call = newTACLine((*TACIndex)++, tt_call, tree);
	call->operands[1].name.str = calledFunction->name;
	BasicBlock_append(block, call);

	if (destinationOperand != NULL)
	{
		call->operands[0].type = calledFunction->returnType;
		call->operands[0].type.indirectionLevel = calledFunction->returnType.indirectionLevel;
		call->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
		call->operands[0].permutation = vp_temp;

		*destinationOperand = call->operands[0];
	}
}

struct TACLine *walkMemberAccess(struct AST *tree,
								 struct BasicBlock *block,
								 struct Scope *scope,
								 int *TACIndex,
								 int *tempNum,
								 struct TACOperand *srcDestOperand,
								 int depth)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkMemberAccess: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if ((tree->type != t_dot) && (tree->type != t_arrow))
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkDotOperator!\n", getTokenName(tree->type));
	}

	struct AST *lhs = tree->child;
	struct AST *rhs = lhs->sibling;

	if (rhs->type != t_identifier)
	{
		ErrorWithAST(ERROR_CODE, rhs,
					 "Expected identifier on RHS of %s operator, got %s (%s) instead!\n",
					 getTokenName(tree->type),
					 rhs->value,
					 getTokenName(rhs->type));
	}

	struct TACLine *accessLine = NULL;

	switch (lhs->type)
	{
	// in the case that we are dot/arrow-ing a LHS which is a dot or arrow, walkMemberAccess will generate the TAC line for the *read* which gets us the address to dot/arrow on
	case t_dot:
	case t_arrow:
		accessLine = walkMemberAccess(lhs, block, scope, TACIndex, tempNum, srcDestOperand, depth + 1);
		break;

	// for all other cases, we can  populate the LHS using walkSubExpression as it is just a more basic read
	default:
	{
		// the LHS of the dot/arrow is the class instance being accessed
		struct AST *class = tree->child;
		// the RHS is what member we are accessing
		struct AST *member = tree->child->sibling;

		// TODO: check more deeply what's being arrow/dotted? Shortlist: dereference, array index, identifier, maybe some pointer arithmetic?
		//		 	- things like (myObjectPointer & 0xFFFFFFF0)->member are obviously wrong, so probably should disallow
		// prevent silly things like (&a)->b
		if ((class->type == t_address_of) && (tree->type == t_arrow))
		{
			ErrorWithAST(ERROR_CODE, class, "Use of arrow operator after address-of operator `(&a)->b` is not supported.\nUse `a.b` instead\n");
		}

		if (member->type != t_identifier)
		{
			ErrorWithAST(ERROR_CODE, member,
						 "Expected identifier on RHS of %s operator, got %s (%s) instead!\n",
						 (tree->type == t_dot ? "dot" : "arrow"),
						 member->value,
						 getTokenName(member->type));
		}

		// our access line is a completely new TAC line, which is a load operation with an offset, storing the load result to a temp
		accessLine = newTACLine(*TACIndex, tt_load_off, tree);
		accessLine->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
		accessLine->operands[0].permutation = vp_temp;

		// if we are at the bottom of potentially-nested dot/arrow operators,
		// we need the base address of the object we're accessing the member from
		if (tree->type == t_dot)
		{
			// we may need to do some manipulation of the subexpression depending on what exactly we're dotting
			switch (class->type)
			{
			case t_dereference:
			{
				struct TACOperand dummyOperand;
				walkSubExpression(class, block, scope, TACIndex, tempNum, &dummyOperand);

				char *indirectTypeName = Type_GetName(TACOperand_GetType(&dummyOperand));
				ErrorWithAST(ERROR_CODE, class, "Use of dot operator on indirect type %s not supported - use arrow operator instead!\n", indirectTypeName);
			}
			break;

			case t_array_index:
			{
				// let walkArrayRef do the heavy lifting for us
				struct TACLine *arrayRefToDot = walkArrayRef(class, block, scope, TACIndex, tempNum);

				// before we convert our array ref to an LEA to get the address of the class we're dotting, check to make sure everything is good
				struct Type nonDecayedType = *TAC_GetTypeOfOperand(arrayRefToDot, 1);
				nonDecayedType.arraySize = 0;
				checkAccessedClassForDot(tree, scope, &nonDecayedType);

				// now that we know we are dotting something valid, we will just use the array reference as an address calculation for the base of whatever we're dotting
				convertArrayRefLoadToLea(arrayRefToDot);

				// copy the TAC operand containing the address on which we will dot
				copyTACOperandDecayArrays(&accessLine->operands[1], &arrayRefToDot->operands[0]);
			};
			break;

			case t_identifier:
			{
				struct TACLine *getAddressForDot = newTACLine(*TACIndex, tt_addrof, tree);
				getAddressForDot->operands[0].permutation = vp_temp;
				getAddressForDot->operands[0].name.str = TempList_Get(temps, (*tempNum)++);

				walkSubExpression(class, block, scope, TACIndex, tempNum, &getAddressForDot->operands[1]);

				if (getAddressForDot->operands[1].permutation != vp_temp)
				{
					// while this check is duplicated in the checks immediately following the switch,
					// we may be able to print more verbose error info if we are directly member-accessing an identifier, so do it here.
					checkAccessedClassForDot(class, scope, &getAddressForDot->operands[1].type);
				}

				copyTACOperandTypeDecayArrays(&getAddressForDot->operands[0], &getAddressForDot->operands[1]);
				TAC_GetTypeOfOperand(getAddressForDot, 0)->indirectionLevel++;

				getAddressForDot->index = (*TACIndex)++;
				BasicBlock_append(block, getAddressForDot);
				copyTACOperandDecayArrays(&accessLine->operands[1], &getAddressForDot->operands[0]);
			}
			break;

			default:
				ErrorWithAST(ERROR_CODE, class, "Dot operator member access on disallowed tree type %s", getTokenName(class->type));
				break;
			}
		}
		else
		{
			walkSubExpression(class, block, scope, TACIndex, tempNum, &accessLine->operands[1]);

			// while this check is duplicated in the checks immediately following the switch,
			// we may be able to print more verbose error info if we are directly member-accessing an identifier, so do it here.
			checkAccessedClassForArrow(class, scope, &accessLine->operands[1].type);
			copyTACOperandTypeDecayArrays(&accessLine->operands[0], &accessLine->operands[1]);
		}

		accessLine->operands[2].type.basicType = vt_u32;
		accessLine->operands[2].permutation = vp_literal;
	}
	break;
	}

	// if we have an arrow, the first thing we need to do is "jump through" the indirection
	// this if statement basically "finalizes" the load before the arrow operator,
	// then sets up a new access solely for the arrow
	if (tree->type == t_arrow)
	{
		// if the existing load operation actually applies an offset, it is necessary to actually "jump through" the indirection by dereferencing at that offset
		// to do this, we cement the existing access for good and then create a new load TAC instruction which will apply only the offset from this arrow operator
		if (accessLine->operands[2].name.val > 0)
		{
			// index and add the existing offset
			accessLine->index = (*TACIndex)++;
			BasicBlock_append(block, accessLine);

			// understand some info about what we're actually reading at this offset, keep track of the old access
			struct Type *existingReadType = TAC_GetTypeOfOperand(accessLine, 0);
			struct TACLine *oldAccessLine = accessLine;

			// the LHS of our arrow must be some sort of class pointer, otherwise we shouldn't be able to use the arrow operator on it!
			if ((existingReadType->basicType == vt_class) && (existingReadType->indirectionLevel == 0))
			{
				// convert the old access to a lea - we don't want to dereference the class, we just need a pointer to it
				oldAccessLine->operation = tt_lea_off;
			}
			else if (existingReadType->basicType != vt_class) // we are currently walking a member access using the arrow operator on something that's not a class pointer
			{
				char *existingReadTypeName = Type_GetName(existingReadType);
				ErrorWithAST(ERROR_CODE, tree, "Use of arrow operator '->' on non-class-pointer type %s!\n", existingReadTypeName);
			}

			accessLine = newTACLine(*TACIndex, tt_load_off, tree);

			copyTACOperandDecayArrays(&accessLine->operands[1], &oldAccessLine->operands[0]);

			accessLine->operands[0].permutation = vp_temp;
			accessLine->operands[0].name.str = TempList_Get(temps, (*tempNum)++);

			accessLine->operands[2].type.basicType = vt_u32;
			accessLine->operands[2].permutation = vp_literal;
			// BasicBlock_append(block, accessLine);
		}
		else
		{
			accessLine->operation = tt_load_off;
		}
	}

	// get the ClassEntry and ClassMemberOffset of what we're accessing within and the member we access
	struct ClassEntry *accessedClass = Scope_lookupClassByType(scope, TAC_GetTypeOfOperand(accessLine, 1));
	struct ClassMemberOffset *accessedMember = Class_lookupMemberVariable(accessedClass, rhs);

	// populate type information (use cast for the first operand as we are treating a class as a pointer to something else with a given offset)
	accessLine->operands[1].castAsType = accessedMember->variable->type;
	accessLine->operands[0].type = *TAC_GetTypeOfOperand(accessLine, 1);			   // copy type info to the temp we're reading to
	copyTACOperandTypeDecayArrays(&accessLine->operands[0], &accessLine->operands[0]); // decay arrays in-place so we only have pointers instead of arrays

	accessLine->operands[2].name.val += accessedMember->offset;

	if (depth == 0)
	{
		accessLine->index = (*TACIndex)++;
		BasicBlock_append(block, accessLine);
		*srcDestOperand = accessLine->operands[0];
	}

	return accessLine;
}

struct TACOperand *walkExpression(struct AST *tree,
								  struct BasicBlock *block,
								  struct Scope *scope,
								  int *TACIndex,
								  int *tempNum)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkExpression: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	// generically set to tt_add, we will actually set the operation within switch cases
	struct TACLine *expression = newTACLine(*TACIndex, tt_subtract, tree);

	expression->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
	expression->operands[0].permutation = vp_temp;

	char fallingThrough = 0;

	switch (tree->type)
	{
	// basic arithmetic
	case t_add:
		expression->reorderable = 1;
		expression->operation = tt_add;
		fallingThrough = 1;
		// fall through, having set to plus and reorderable
	case t_multiply:
		if (!fallingThrough)
		{
			expression->reorderable = 1;
			expression->operation = tt_mul;
			fallingThrough = 1;
		}
		// fall through
	case t_bitwise_and:
		if (!fallingThrough)
		{
			expression->reorderable = 1;
			expression->operation = tt_bitwise_and;
			fallingThrough = 1;
		}
		// fall through
	case t_bitwise_or:
		if (!fallingThrough)
		{
			expression->reorderable = 1;
			expression->operation = tt_bitwise_or;
			fallingThrough = 1;
		}
		// fall through
	case t_bitwise_xor:
		if (!fallingThrough)
		{
			expression->reorderable = 1;
			expression->operation = tt_bitwise_xor;
			fallingThrough = 1;
		}
		// fall through
	case t_bitwise_not:
		if (!fallingThrough)
		{
			expression->reorderable = 1;
			expression->operation = tt_bitwise_not;
			fallingThrough = 1;
		}
		// fall through
	case t_divide:
		if (!fallingThrough)
		{
			expression->operation = tt_div;
			fallingThrough = 1;
		}
		// fall through

	case t_modulo:
		if (!fallingThrough)
		{
			expression->operation = tt_modulo;
			fallingThrough = 1;
		}
		// fall through

	case t_lshift:
		if (!fallingThrough)
		{
			expression->operation = tt_lshift;
			fallingThrough = 1;
		}
		// fall through

	case t_rshift:
		if (!fallingThrough)
		{
			expression->operation = tt_rshift;
			fallingThrough = 1;
		}
		// fall through

	case t_subtract:
	{
		walkSubExpression(tree->child, block, scope, TACIndex, tempNum, &expression->operands[1]);

		if (TAC_GetTypeOfOperand(expression, 1)->indirectionLevel > 0)
		{
			struct TACLine *scaleMultiply = setUpScaleMultiplication(tree, scope, TACIndex, tempNum, TAC_GetTypeOfOperand(expression, 1));
			walkSubExpression(tree->child->sibling, block, scope, TACIndex, tempNum, &scaleMultiply->operands[1]);

			scaleMultiply->operands[0].type = scaleMultiply->operands[1].type;
			copyTACOperandDecayArrays(&expression->operands[2], &scaleMultiply->operands[0]);

			scaleMultiply->index = (*TACIndex)++;
			BasicBlock_append(block, scaleMultiply);
		}
		else
		{
			walkSubExpression(tree->child->sibling, block, scope, TACIndex, tempNum, &expression->operands[2]);
		}

		struct TACOperand *operandA = &expression->operands[1];
		struct TACOperand *operandB = &expression->operands[2];
		if ((operandA->type.indirectionLevel > 0) && (operandB->type.indirectionLevel > 0))
		{
			ErrorWithAST(ERROR_CODE, tree, "Arithmetic between 2 pointers is not allowed!\n");
		}

		// TODO generate errors for bad pointer arithmetic here
		if (Scope_getSizeOfType(scope, &operandA->type) > Scope_getSizeOfType(scope, &operandB->type))
		{
			copyTACOperandTypeDecayArrays(&expression->operands[0], operandA);
		}
		else
		{
			copyTACOperandTypeDecayArrays(&expression->operands[0], operandB);
		}
	}
	break;

	default:
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkExpression!\n", getTokenName(tree->type));
	}

	expression->index = (*TACIndex)++;
	BasicBlock_append(block, expression);

	return &expression->operands[0];
}

struct TACLine *walkArrayRef(struct AST *tree,
							 struct BasicBlock *block,
							 struct Scope *scope,
							 int *TACIndex,
							 int *tempNum)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkArrayRef: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_array_index)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkArrayRef!\n", getTokenName(tree->type));
	}

	struct AST *arrayBase = tree->child;
	struct AST *arrayIndex = tree->child->sibling;
	if (arrayBase->type != t_identifier)
	{
		ErrorWithAST(ERROR_INTERNAL, arrayBase, "Wrong AST (%s) as child of arrayref\n", getTokenName(arrayBase->type));
	}

	struct TACLine *arrayRefTAC = newTACLine((*TACIndex), tt_load_arr, tree);

	struct VariableEntry *arrayVariable = Scope_lookupVar(scope, arrayBase);
	populateTACOperandFromVariable(&arrayRefTAC->operands[1], arrayVariable);

	copyTACOperandDecayArrays(&arrayRefTAC->operands[0], &arrayRefTAC->operands[1]);
	arrayRefTAC->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
	arrayRefTAC->operands[0].permutation = vp_temp;
	arrayRefTAC->operands[0].type.indirectionLevel--;

	if (arrayIndex->type == t_constant)
	{
		// if referencing an array of classes, implicitly convert to an LEA to avoid copying the entire class to a temp
		if ((arrayVariable->type.basicType == vt_class) && (arrayVariable->type.indirectionLevel == 0))
		{
			arrayRefTAC->operation = tt_lea_off;
			arrayRefTAC->operands[0].type.indirectionLevel++;
		}
		else
		{
			arrayRefTAC->operation = tt_load_off;
		}

		int indexSize = atoi(arrayIndex->value);
		indexSize *= 1 << alignSize(Scope_getSizeOfArrayElement(scope, arrayVariable));

		arrayRefTAC->operands[2].name.val = indexSize;
		arrayRefTAC->operands[2].permutation = vp_literal;
		arrayRefTAC->operands[2].type.basicType = selectVariableTypeForNumber(arrayRefTAC->operands[2].name.val);
	}
	// otherwise, the index is either a variable or subexpression
	else
	{
		// if referencing an array of classes, implicitly convert to an LEA to avoid copying the entire class to a temp
		if ((arrayVariable->type.basicType == vt_class) && (arrayVariable->type.indirectionLevel == 0))
		{
			arrayRefTAC->operation = tt_lea_arr;
			arrayRefTAC->operands[0].type.indirectionLevel++;
		}
		// set the scale for the array access
		arrayRefTAC->operands[3].name.val = alignSize(Scope_getSizeOfArrayElement(scope, arrayVariable));
		arrayRefTAC->operands[3].permutation = vp_literal;
		arrayRefTAC->operands[3].type.basicType = selectVariableTypeForNumber(arrayRefTAC->operands[3].name.val);

		walkSubExpression(arrayIndex, block, scope, TACIndex, tempNum, &arrayRefTAC->operands[2]);
	}

	arrayRefTAC->index = (*TACIndex)++;
	BasicBlock_append(block, arrayRefTAC);
	return arrayRefTAC;
}

struct TACOperand *walkDereference(struct AST *tree,
								   struct BasicBlock *block,
								   struct Scope *scope,
								   int *TACIndex,
								   int *tempNum)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkDereference: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_dereference)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkDereference!\n", getTokenName(tree->type));
	}

	struct TACLine *dereference = newTACLine(*tempNum, tt_load, tree);

	switch (tree->child->type)
	{
	case t_add:
	case t_subtract:
	{
		walkPointerArithmetic(tree->child, block, scope, TACIndex, tempNum, &dereference->operands[1]);
	}
	break;

	default:
		walkSubExpression(tree->child, block, scope, TACIndex, tempNum, &dereference->operands[1]);
		break;
	}

	copyTACOperandDecayArrays(&dereference->operands[0], &dereference->operands[1]);
	dereference->operands[0].type.indirectionLevel--;
	dereference->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
	dereference->operands[0].permutation = vp_temp;

	dereference->index = (*TACIndex)++;
	BasicBlock_append(block, dereference);

	return &dereference->operands[0];
}

struct TACOperand *walkAddrOf(struct AST *tree,
							  struct BasicBlock *block,
							  struct Scope *scope,
							  int *TACIndex,
							  int *tempNum)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkAddrOf: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_address_of)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkAddressOf!\n", getTokenName(tree->type));
	}

	struct TACLine *addrOfLine = newTACLine(*TACIndex, tt_addrof, tree);
	addrOfLine->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
	addrOfLine->operands[0].permutation = vp_temp;

	switch (tree->child->type)
	{
	// look up the variable entry and ensure that we will spill it to the stack since we take its address
	case t_identifier:
	{
		struct VariableEntry *addrTakenOf = Scope_lookupVar(scope, tree->child);
		if (addrTakenOf->type.arraySize > 0)
		{
			ErrorWithAST(ERROR_CODE, tree->child, "Can't take address of local array %s!\n", addrTakenOf->name);
		}
		addrTakenOf->mustSpill = 1;
		walkSubExpression(tree->child, block, scope, TACIndex, tempNum, &addrOfLine->operands[1]);
	}
	break;

	case t_array_index:
	{
		// use walkArrayRef to generate the access we need, just the direct accessing load to an lea to calculate the address we would have loaded from
		struct TACLine *arrayRefLine = walkArrayRef(tree->child, block, scope, TACIndex, tempNum);
		convertArrayRefLoadToLea(arrayRefLine);

		// early return, no need for explicit address-of TAC
		freeTAC(addrOfLine);
		addrOfLine = NULL;

		return &arrayRefLine->operands[0];
	}
	break;

	case t_dot:
	case t_arrow:
	{
		// walkMemberAccess can do everything we need
		// the only thing we have to do is ensure we have an LEA at the end instead of a direct read in the case of the dot operator
		struct TACLine *memberAccessLine = walkMemberAccess(tree->child, block, scope, TACIndex, tempNum, &addrOfLine->operands[1], 0);

		memberAccessLine->operation = tt_lea_off;
		memberAccessLine->operands[0].type.indirectionLevel++;
		memberAccessLine->operands[1].castAsType.indirectionLevel++;
		addrOfLine->operands[0].type.indirectionLevel++;

		// free the line created at the top of this function and return early
		freeTAC(addrOfLine);
		return &memberAccessLine->operands[0];
	}
	break;

	default:
		ErrorWithAST(ERROR_CODE, tree, "Address of operator is not supported for non-identifiers! Saw %s\n", getTokenName(tree->child->type));
	}

	addrOfLine->operands[0].type = *TAC_GetTypeOfOperand(addrOfLine, 1);
	addrOfLine->operands[0].type.indirectionLevel++;
	addrOfLine->operands[0].type.arraySize = 0;

	addrOfLine->index = (*TACIndex)++;
	BasicBlock_append(block, addrOfLine);

	return &addrOfLine->operands[0];
}

void walkPointerArithmetic(struct AST *tree,
						   struct BasicBlock *block,
						   struct Scope *scope,
						   int *TACIndex,
						   int *tempNum,
						   struct TACOperand *destinationOperand)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkPointerArithmetic: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if ((tree->type != t_add) && (tree->type != t_subtract))
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkPointerArithmetic!\n", getTokenName(tree->type));
	}

	struct AST *pointerArithLHS = tree->child;
	struct AST *pointerArithRHS = tree->child->sibling;

	struct TACLine *pointerArithmetic = newTACLine(*TACIndex, tt_add, tree->child);
	if (tree->type == t_subtract)
	{
		pointerArithmetic->operation = tt_subtract;
	}

	walkSubExpression(pointerArithLHS, block, scope, TACIndex, tempNum, &pointerArithmetic->operands[1]);
	copyTACOperandDecayArrays(&pointerArithmetic->operands[0], &pointerArithmetic->operands[1]);
	pointerArithmetic->operands[0].name.str = TempList_Get(temps, (*tempNum)++);

	pointerArithmetic->operands[0].permutation = vp_temp;

	struct TACLine *scaleMultiplication = setUpScaleMultiplication(pointerArithRHS,
																   scope,
																   TACIndex,
																   tempNum,
																   TAC_GetTypeOfOperand(pointerArithmetic, 1));

	walkSubExpression(pointerArithRHS, block, scope, TACIndex, tempNum, &scaleMultiplication->operands[1]);

	copyTACOperandTypeDecayArrays(&scaleMultiplication->operands[0], &scaleMultiplication->operands[1]);

	copyTACOperandDecayArrays(&pointerArithmetic->operands[2], &scaleMultiplication->operands[0]);

	scaleMultiplication->index = (*TACIndex)++;
	BasicBlock_append(block, scaleMultiplication);
	pointerArithmetic->index = (*TACIndex)++;
	BasicBlock_append(block, pointerArithmetic);

	*destinationOperand = pointerArithmetic->operands[0];
}

void walkAsmBlock(struct AST *tree,
				  struct BasicBlock *block,
				  struct Scope *scope,
				  int *TACIndex,
				  int *tempNum)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkAsmBlock: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_asm)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkAsmBlock!\n", getTokenName(tree->type));
	}

	struct AST *asmRunner = tree->child;
	while (asmRunner != NULL)
	{
		if (asmRunner->type != t_asm)
		{
			ErrorWithAST(ERROR_INTERNAL, tree, "Non-asm seen as contents of ASM block!\n");
		}

		struct TACLine *asmLine = newTACLine((*TACIndex)++, tt_asm, asmRunner);
		asmLine->operands[0].name.str = asmRunner->value;

		BasicBlock_append(block, asmLine);

		asmRunner = asmRunner->sibling;
	}
}

void walkStringLiteral(struct AST *tree,
					   struct BasicBlock *block,
					   struct Scope *scope,
					   struct TACOperand *destinationOperand)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkStringLiteral: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_string_literal)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkStringLiteral!\n", getTokenName(tree->type));
	}

	// Scope_createStringLiteral will modify the value of our AST node
	// it inserts underscores in place of spaces and other modifications to turn the literal into a name that the symtab can use
	// but first, it copies the string exactly as-is so it knows what the string object should be initialized to
	char *stringName = tree->value;
	char *stringValue = strdup(stringName);
	int stringLength = strlen(stringName);

	for (int i = 0; i < stringLength; i++)
	{
		if ((!isalnum(stringName[i])) && (stringName[i] != '_'))
		{
			if (isspace(stringName[i]))
			{
				stringName[i] = '_';
			}
			else
			{
				// for any non-whitespace character, map it to lower/uppercase alphabetic characters
				// this should avoid collisions with renamed strings to the point that it isn't a problem
				char altVal = stringName[i] % 52;
				if (altVal > 25)
				{
					stringName[i] = altVal + 'A';
				}
				else
				{
					stringName[i] = altVal + 'a';
				}
			}
		}
	}

	struct VariableEntry *stringLiteralEntry = NULL;
	struct ScopeMember *existingMember = NULL;

	// if we already have a string literal for this thing, nothing else to do
	if ((existingMember = Scope_lookup(scope, stringName)) == NULL)
	{
		struct AST fakeStringTree;
		fakeStringTree.value = stringName;
		fakeStringTree.sourceFile = tree->sourceFile;
		fakeStringTree.sourceLine = tree->sourceLine;
		fakeStringTree.sourceCol = tree->sourceCol;

		struct Type stringType;
		stringType.basicType = vt_u8;
		stringType.arraySize = stringLength;
		stringType.indirectionLevel = 0;

		stringLiteralEntry = Scope_createVariable(scope, &fakeStringTree, &stringType, 1, 0, 0);
		stringLiteralEntry->isStringLiteral = 1;

		struct Type *realStringType = &stringLiteralEntry->type;
		realStringType->initializeArrayTo = malloc(stringLength * sizeof(char *));
		for (int i = 0; i < stringLength; i++)
		{
			realStringType->initializeArrayTo[i] = malloc(1);
			*realStringType->initializeArrayTo[i] = stringValue[i];
		}
	}
	else
	{
		stringLiteralEntry = existingMember->entry;
	}

	free(stringValue);
	populateTACOperandFromVariable(destinationOperand, stringLiteralEntry);
	destinationOperand->name.str = stringName;
	destinationOperand->type.arraySize = stringLength;
}

void walkSizeof(struct AST *tree,
				struct BasicBlock *block,
				struct Scope *scope,
				struct TACOperand *destinationOperand)
{
	if (currentVerbosity == VERBOSITY_MAX)
	{
		printf("walkSizeof: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	}

	if (tree->type != t_sizeof)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Wrong AST (%s) passed to walkSizeof!\n", getTokenName(tree->type));
	}

	int sizeInBytes = -1;

	switch (tree->child->type)
	{
	// if we see an identifier, it may be an identifier or a class name
	case t_identifier:
	{
		// do a generic scope lookup on the identifier
		struct ScopeMember *lookedUpIdentifier = Scope_lookup(scope, tree->child->value);
		
		// if it looks up nothing, or it's a variable
		if ((lookedUpIdentifier == NULL) || (lookedUpIdentifier->type == e_variable))
		{
			// Scope_lookupVar is not redundant as it will give us a 'use of undeclared' error in the case where we looked up nothing
			struct VariableEntry *getSizeof = Scope_lookupVar(scope, tree->child);

			sizeInBytes = Scope_getSizeOfType(scope, &getSizeof->type);
			break;
		}
		// we looked something up but it's not a variable 
		else
		{
			struct ClassEntry *getSizeof = Scope_lookupClass(scope, tree->child);

			sizeInBytes = getSizeof->totalSize;
		}
	}
	break;

	case t_type_name:
	{
		struct Type getSizeof;
		walkTypeName(tree->child, scope, &getSizeof);

		sizeInBytes = Scope_getSizeOfType(scope, &getSizeof);
	}
	break;
	default:
		ErrorWithAST(ERROR_CODE, tree, "sizeof is only supported on type names and identifiers!\n");
	}

	char sizeString[16];
	snprintf(sizeString, 15, "%d", sizeInBytes);
	destinationOperand->type.basicType = vt_u8;
	destinationOperand->permutation = vp_literal;
	destinationOperand->name.str = Dictionary_LookupOrInsert(parseDict, sizeString);
}