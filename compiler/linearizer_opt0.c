#include "linearizer_opt0.h"

/*
 * These functions walk the AST and convert it to three-address code
 */
struct TempList *temps;
extern struct Dictionary *parseDict;
struct SymbolTable *walkProgram_0(struct AST *program)
{
	struct SymbolTable *programTable = SymbolTable_new("Program");
	struct BasicBlock *globalBlock = BasicBlock_new(1);
	Scope_insert(programTable->globalScope, "GLOBALBLOCK", globalBlock, e_basicblock);
	temps = TempList_New();

	int globalTACIndex = 0;
	int globalTempNum = 0;

	struct AST *programRunner = program;
	while (programRunner != NULL)
	{
		switch (programRunner->type)
		{
		// global variable declarations/definitions are allowed
		// use walkStatement to handle this
		case t_uint8:
		case t_uint16:
		case t_uint32:
			walkVariableDeclaration_0(programRunner, globalBlock, programTable->globalScope, &globalTACIndex, &globalTempNum, 0);
			break;

		case t_single_equals:
			walkAssignment_0(programRunner, globalBlock, programTable->globalScope, &globalTACIndex, &globalTempNum);
			break;

		case t_fun:
			walkFunctionDeclaration_0(programRunner, programTable->globalScope);
			break;

		// ignore asm blocks
		case t_asm:
			break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Error walking AST - got %s with type %d\n", programRunner->value, programRunner->type);
			break;
		}
		programRunner = programRunner->sibling;
	}

	// TempList_Free(temps);
	return programTable;
}

// int linearizeDeclaration(struct LinearizationMetadata m)
struct VariableEntry *walkVariableDeclaration_0(struct AST *tree,
												struct BasicBlock *block,
												struct Scope *scope,
												int *TACIndex,
												int *tempNum,
												char isArgument)
{
	printf("walkVariableDeclaration: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);

	enum basicTypes declaredBasicType;
	switch (tree->type)
	{
	case t_uint8:
		declaredBasicType = vt_uint8;
		break;

	case t_uint16:
		declaredBasicType = vt_uint16;
		break;

	case t_uint32:
		declaredBasicType = vt_uint32;
		break;

	default:
		ErrorWithAST(ERROR_INTERNAL, tree, "Malformed AST seen in declaration!");
	}

	struct AST *declaredTree = NULL;
	int declaredIndirectionLevel = scrapePointers(tree->child, &declaredTree);

	struct Type declaredType;
	declaredType.basicType = declaredBasicType;
	declaredType.indirectionLevel = declaredIndirectionLevel;

	// if we are declaring an array, set the string with the size as the second operand
	if (declaredTree->type == t_lBracket)
	{
		declaredTree = declaredTree->child;
		char *arraySizeString = declaredTree->sibling->value;
		int declaredArraySize = atoi(arraySizeString);

		declaredType.arraySize = declaredArraySize;
	}
	else
	{
		declaredType.arraySize = 0;
	}
	struct VariableEntry *declaredVariable = Scope_createVariable(scope,
																  declaredTree,
																  &declaredType,
																  (scope->parentScope == NULL),
																  *TACIndex,
																  isArgument);

	return declaredVariable;
}

void walkArgumentDeclaration_0(struct AST *tree,
							   struct BasicBlock *block,
							   int *TACIndex,
							   int *tempNum,
							   struct FunctionEntry *fun)
{
	printf("walkArgumentDeclaration: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	struct VariableEntry *declaredArgument = walkVariableDeclaration_0(tree, block, fun->mainScope, TACIndex, tempNum, 1);

	declaredArgument->assignedAt = 0;
	declaredArgument->isAssigned = 1;

	Stack_Push(fun->arguments, declaredArgument);
}

void walkFunctionDeclaration_0(struct AST *tree,
							   struct Scope *scope)
{
	printf("walkFunctionDeclaration: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	if (tree->type != t_fun)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkFunctionDeclaration!\n", getTokenName(tree->type));
	}

	// skip past the argumnent declarations to the return type declaration
	struct AST *returnTypeRunner = tree->child;
	while (returnTypeRunner->type != t_pointer_op)
	{
		returnTypeRunner = returnTypeRunner->sibling;
	}
	returnTypeRunner = returnTypeRunner->sibling;

	enum basicTypes returnBasicType;
	switch (returnTypeRunner->type)
	{
	case t_void:
		returnBasicType = vt_null;
		break;

	case t_uint8:
		returnBasicType = vt_uint8;
		break;

	case t_uint16:
		returnBasicType = vt_uint16;
		break;

	case t_uint32:
		returnBasicType = vt_uint32;
		break;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Malformed AST as return type for function\n");
	}
	int returnIndirectionLevel = scrapePointers(returnTypeRunner->child, &returnTypeRunner);

	// child is the lparen, function name is the child of the lparen
	struct AST *functionNameTree = tree->child->child;
	struct ScopeMember *lookedUpFunction = Scope_lookup(scope, functionNameTree->value);
	struct FunctionEntry *parsedFunc = NULL;
	struct FunctionEntry *existingFunc = NULL;

	struct Type returnType;
	returnType.basicType = returnBasicType;
	returnType.indirectionLevel = returnIndirectionLevel;
	returnType.arraySize = 0;
	returnType.initializeArrayTo = NULL;

	if (lookedUpFunction != NULL)
	{
		existingFunc = lookedUpFunction->entry;
		parsedFunc = FunctionEntry_new(scope, functionNameTree->value, &returnType);
	}
	else
	{
		parsedFunc = Scope_createFunction(scope, functionNameTree->value, &returnType);
		parsedFunc->mainScope->parentScope = scope;
	}

	struct AST *argumentRunner = tree->child->sibling;
	int TACIndex = 0;
	int tempNum = 0;
	struct BasicBlock *block = BasicBlock_new(0);
	while (argumentRunner->type != t_pointer_op)
	{
		switch (argumentRunner->type)
		{
			// looking at argument declarations
		case t_uint8:
		case t_uint16:
		case t_uint32:
		{
			walkArgumentDeclaration_0(argumentRunner, block, &TACIndex, &tempNum, parsedFunc);
		}
		break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Malformed AST within function - expected function name and main scope only!\nMalformed node was of type %s with value [%s]\n", getTokenName(argumentRunner->type), argumentRunner->value);
		}
		argumentRunner = argumentRunner->sibling;
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

	struct AST *definition = argumentRunner->sibling->sibling;
	if (definition != NULL)
	{
		if (existingFunc != NULL)
		{
			FunctionEntry_free(parsedFunc);
			existingFunc->isDefined = 1;
			walkFunctionDefinition_0(definition, existingFunc);
		}
		else
		{
			parsedFunc->isDefined = 1;
			walkFunctionDefinition_0(definition, parsedFunc);
		}
	}
}

void walkFunctionDefinition_0(struct AST *tree,
							  struct FunctionEntry *fun)
{
	printf("walkFunctionDefinition: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);

	if (tree->type != t_lCurly)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkFunctionDefinition!\n", getTokenName(tree->type));
	}
	int TACIndex = 0;
	int tempNum = 0;
	int labelNum = 1;
	struct BasicBlock *block = BasicBlock_new(0);
	Scope_addBasicBlock(fun->mainScope, block);
	walkScope_0(tree, block, fun->mainScope, &TACIndex, &tempNum, &labelNum, -1);
}

void walkScope_0(struct AST *tree,
				 struct BasicBlock *block,
				 struct Scope *scope,
				 int *TACIndex,
				 int *tempNum,
				 int *labelNum,
				 int controlConvergesToLabel)
{
	printf("walkScope: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);

	if (tree->type != t_lCurly)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkScope!\n", getTokenName(tree->type));
	}

	struct AST *scopeRunner = tree->child;
	while ((scopeRunner != NULL) && (scopeRunner->type != t_rCurly))
	{
		switch (scopeRunner->type)
		{
		case t_uint8:
		case t_uint16:
		case t_uint32:
			walkVariableDeclaration_0(scopeRunner, block, scope, TACIndex, tempNum, 0);
			break;

		case t_single_equals:
			walkAssignment_0(scopeRunner, block, scope, TACIndex, tempNum);
			break;

		case t_plus_equals:
		case t_minus_equals:
			walkArithmeticAssignment_0(scopeRunner, block, scope, TACIndex, tempNum);
			break;

		case t_while:
			// while loop
			{
				struct BasicBlock *afterWhileBlock = BasicBlock_new((*labelNum)++);
				walkWhileLoop_0(scopeRunner, block, scope, TACIndex, tempNum, labelNum, afterWhileBlock->labelNum);
				block = afterWhileBlock;
				Scope_addBasicBlock(scope, afterWhileBlock);
			}
			break;

		case t_if:
			// if statement
			{
				struct BasicBlock *afterIfBlock = BasicBlock_new((*labelNum)++);
				walkIfStatement_0(scopeRunner, block, scope, TACIndex, tempNum, labelNum, afterIfBlock->labelNum);
				block = afterIfBlock;
				Scope_addBasicBlock(scope, afterIfBlock);
			}
			break;

		case t_lParen:
			walkFunctionCall_0(scopeRunner, block, scope, TACIndex, tempNum, NULL);
			break;

		case t_lCurly:
			// subscope
			{
				struct Scope *subScope = Scope_createSubScope(scope);
				struct BasicBlock *afterSubScopeBlock = BasicBlock_new((*labelNum)++);
				walkScope_0(scopeRunner, block, subScope, TACIndex, tempNum, labelNum, afterSubScopeBlock->labelNum);
				block = afterSubScopeBlock;
				Scope_addBasicBlock(scope, afterSubScopeBlock);
			}
			break;

			// case t_fun:
			// walkFunctionDeclaration_0(scopeRunner, programTable->globalScope);
			// break;

		case t_return:
			// return
			{
				struct TACLine *returnLine = newTACLine(*TACIndex, tt_return, scopeRunner);
				if (scopeRunner->child != NULL)
				{
					walkSubExpression_0(scopeRunner->child, block, scope, TACIndex, tempNum, &returnLine->operands[0]);
				}

				returnLine->index = (*TACIndex)++;

				BasicBlock_append(block, returnLine);
			}
			break;

		case t_asm:
			walkAsmBlock_0(scopeRunner, block, scope, TACIndex, tempNum);
			break;

		case t_rCurly:
			break;

		default:
			ErrorWithAST(ERROR_INTERNAL, scopeRunner, "Unexpected AST type (%s - %s) seen in walkScope!\n", getTokenName(scopeRunner->type), scopeRunner->value);
		}
		scopeRunner = scopeRunner->sibling;
	}

	if (controlConvergesToLabel > 0)
	{
		struct TACLine *controlConvergeJmp = newTACLine((*TACIndex)++, tt_jmp, tree);
		controlConvergeJmp->operands[0].name.val = controlConvergesToLabel;
		BasicBlock_append(block, controlConvergeJmp);
	}

	if ((scopeRunner == NULL) || (scopeRunner->type != t_rCurly))
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Expected t_rCurly at end for scope\n");
	}
}

void walkConditionCheck_0(struct AST *tree,
						  struct BasicBlock *block,
						  struct Scope *scope,
						  int *TACIndex,
						  int *tempNum,
						  int falseJumpLabelNum)
{
	printf("walkConditionCheck: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);

	struct TACLine *condFalseJump = newTACLine(*TACIndex, tt_jmp, tree);
	condFalseJump->operands[0].name.val = falseJumpLabelNum;

	// switch once to decide the jump type
	switch (tree->type)
	{
	case t_equals:
		condFalseJump->operation = tt_jne;
		break;

	case t_nEquals:
		condFalseJump->operation = tt_je;
		break;

	case t_lThan:
		condFalseJump->operation = tt_jge;
		break;

	case t_gThan:
		condFalseJump->operation = tt_jle;
		break;

	case t_lThanE:
		condFalseJump->operation = tt_jg;
		break;

	case t_gThanE:
		condFalseJump->operation = tt_jl;
		break;

	case t_not:
		condFalseJump->operation = tt_jnz;
		break;

	default:
		condFalseJump->operation = tt_jz;
	}

	struct TACLine *compareOperation = newTACLine(*TACIndex, tt_cmp, tree);

	// switch a second time to actually walk the condition
	switch (tree->type)
	{
	case t_equals:
	case t_nEquals:
	case t_lThan:
	case t_gThan:
	case t_lThanE:
	case t_gThanE:
		// standard operators (==, !=, <, >, <=, >=)
		{
			walkSubExpression_0(tree->child, block, scope, TACIndex, tempNum, &compareOperation->operands[1]);
			walkSubExpression_0(tree->child->sibling, block, scope, TACIndex, tempNum, &compareOperation->operands[2]);
		}
		break;

	case t_not:
		// NOT any condition (!)
		{
			walkSubExpression_0(tree->child, block, scope, TACIndex, tempNum, &compareOperation->operands[1]);
			compareOperation->operands[2].name.val = 0;
			compareOperation->operands[2].permutation = vp_literal;
			TACOperand_SetBasicType(&compareOperation->operands[2], vt_uint32, 0);
		}
		break;

	default:
		// any other sort of condition - just some expression
		{
			walkSubExpression_0(tree, block, scope, TACIndex, tempNum, &compareOperation->operands[1]);
			compareOperation->operands[2].name.val = 0;
			compareOperation->operands[2].permutation = vp_literal;
			TACOperand_SetBasicType(&compareOperation->operands[2], vt_uint32, 0);
			condFalseJump->operation = tt_jz;
		}
		break;
	}
	compareOperation->index = (*TACIndex)++;
	BasicBlock_append(block, compareOperation);

	condFalseJump->index = (*TACIndex)++;
	BasicBlock_append(block, condFalseJump);
}

void walkWhileLoop_0(struct AST *tree,
					 struct BasicBlock *block,
					 struct Scope *scope,
					 int *TACIndex,
					 int *tempNum,
					 int *labelNum,
					 int controlConvergesToLabel)
{
	printf("walkWhileLoop: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);

	if (tree->type != t_while)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkWhileLoop!\n", getTokenName(tree->type));
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

	walkConditionCheck_0(tree->child, whileBlock, whileScope, TACIndex, tempNum, controlConvergesToLabel);

	int endWhileLabel = (*labelNum)++;

	walkScope_0(tree->child->sibling, whileBlock, whileScope, TACIndex, tempNum, labelNum, endWhileLabel);

	struct TACLine *whileLoopJump = newTACLine((*TACIndex)++, tt_jmp, tree);
	whileLoopJump->operands[0].name.val = whileBlock->labelNum;

	block = BasicBlock_new(endWhileLabel);
	Scope_addBasicBlock(scope, block);

	struct TACLine *whileEndDo = newTACLine((*TACIndex)++, tt_enddo, tree);
	BasicBlock_append(block, whileLoopJump);
	BasicBlock_append(block, whileEndDo);
}

void walkIfStatement_0(struct AST *tree,
					   struct BasicBlock *block,
					   struct Scope *scope,
					   int *TACIndex,
					   int *tempNum,
					   int *labelNum,
					   int controlConvergesToLabel)
{
	if (tree->type != t_if)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkIfStatement!\n", getTokenName(tree->type));
	}

	// if we have an else block
	if (tree->child->sibling->sibling != NULL)
	{
		int elseLabel = (*labelNum)++;
		walkConditionCheck_0(tree->child, block, scope, TACIndex, tempNum, elseLabel);

		struct Scope *ifScope = Scope_createSubScope(scope);
		struct BasicBlock *ifBlock = BasicBlock_new((*labelNum)++);
		Scope_addBasicBlock(scope, ifBlock);

		struct TACLine *enterIfJump = newTACLine((*TACIndex)++, tt_jmp, tree);
		enterIfJump->operands[0].name.val = ifBlock->labelNum;
		BasicBlock_append(block, enterIfJump);

		walkScope_0(tree->child->sibling, ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);

		struct Scope *elseScope = Scope_createSubScope(scope);
		struct BasicBlock *elseBlock = BasicBlock_new(elseLabel);
		Scope_addBasicBlock(scope, elseBlock);
		walkScope_0(tree->child->sibling->sibling, elseBlock, elseScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
	}
	// no else block
	else
	{
		walkConditionCheck_0(tree->child, block, scope, TACIndex, tempNum, controlConvergesToLabel);

		struct Scope *ifScope = Scope_createSubScope(scope);
		struct BasicBlock *ifBlock = BasicBlock_new((*labelNum)++);
		Scope_addBasicBlock(scope, ifBlock);

		struct TACLine *enterIfJump = newTACLine((*TACIndex)++, tt_jmp, tree);
		enterIfJump->operands[0].name.val = ifBlock->labelNum;
		BasicBlock_append(block, enterIfJump);

		walkScope_0(tree->child->sibling, ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
	}
}

void walkAssignment_0(struct AST *tree,
					  struct BasicBlock *block,
					  struct Scope *scope,
					  int *TACIndex,
					  int *tempNum)
{
	if (tree->type != t_single_equals)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkAssignment!\n", getTokenName(tree->type));
	}

	struct AST *lhs = tree->child;
	struct AST *rhs = tree->child->sibling;

	// don't increment the index until after we deal with nested expressions
	struct TACLine *assignment = newTACLine((*TACIndex), tt_assign, tree);

	struct TACOperand assignedValue;
	memset(&assignedValue, 0, sizeof(struct TACOperand));
	// walk the RHS of the assignment as a subexpression and save the operand for later
	walkSubExpression_0(rhs, block, scope, TACIndex, tempNum, &assignedValue);

	struct VariableEntry *assignedVariable = NULL;
	switch (lhs->type)
	{
	case t_uint8:
	case t_uint16:
	case t_uint32:
		assignedVariable = walkVariableDeclaration_0(lhs, block, scope, TACIndex, tempNum, 0);
		populateTACOperandFromVariable(&assignment->operands[0], assignedVariable);
		// copyTACOperandDecayArrays(&assignment->operands[1], &assignedValue);
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
		// copyTACOperandDecayArrays(&assignment->operands[1], &assignedValue);
		assignment->operands[1] = assignedValue;
		break;

	// TODO: generate optimized addressing modes for arithmetic
	case t_star:
	{
		struct AST *writtenPointer = lhs->child;
		switch (writtenPointer->type)
		{
		case t_plus:
		case t_minus:
			walkPointerArithmetic_0(writtenPointer, block, scope, TACIndex, tempNum, &assignment->operands[0]);
			break;

		default:
			walkSubExpression_0(writtenPointer, block, scope, TACIndex, tempNum, &assignment->operands[0]);
			break;
		}
		assignment->operation = tt_memw_1;
		assignment->operands[1] = assignedValue;
	}
	break;

	case t_lBracket:
	{
		struct AST *arrayName = lhs->child;
		struct AST *arrayIndex = lhs->child->sibling;
		struct VariableEntry *arrayVariable = Scope_lookupVar(scope, arrayName);

		if ((arrayVariable->type.indirectionLevel < 1) &&
			(arrayVariable->type.arraySize == 0))
		{
			ErrorWithAST(ERROR_CODE, arrayName, "Use of non-pointer variable %s as array!\n", arrayName->value);
		}

		assignment->operation = tt_memw_3;
		populateTACOperandFromVariable(&assignment->operands[0], arrayVariable);

		assignment->operands[2].permutation = vp_literal;
		assignment->operands[2].type.indirectionLevel = 0;
		assignment->operands[2].type.basicType = vt_uint8;
		assignment->operands[2].name.val = alignSize(Scope_getSizeOfArrayElement(scope, arrayVariable));

		walkSubExpression_0(arrayIndex, block, scope, TACIndex, tempNum, &assignment->operands[1]);

		assignment->operands[3] = assignedValue;
	}
	break;

	default:
		ErrorWithAST(ERROR_INTERNAL, lhs, "Unexpected AST (%s) seen in walkAssignment!\n", lhs->value);
		break;
	}

	assignment->index = (*TACIndex)++;
	BasicBlock_append(block, assignment);
}

void walkArithmeticAssignment_0(struct AST *tree,
								struct BasicBlock *block,
								struct Scope *scope,
								int *TACIndex,
								int *tempNum)
{
	struct AST fakeArith = *tree;
	switch (tree->type)
	{
	case t_plus_equals:
		fakeArith.type = t_plus;
		fakeArith.value = "+";
		break;

	case t_minus_equals:
		fakeArith.type = t_minus;
		fakeArith.value = "-";
		break;

	default:
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkAssignment!\n", getTokenName(tree->type));
	}

	// our fake arithmetic ast will have the child of the arithmetic assignment operator
	// this effectively duplicates the LHS of the assignment to the first operand of the arithmetic operator
	struct AST *lhs = tree->child;
	fakeArith.child = lhs;

	struct AST fakelhs = *lhs;
	fakelhs.sibling = &fakeArith;

	struct AST fakeAssignment = *tree;
	fakeAssignment.value = "=";
	fakeAssignment.type = t_single_equals;

	fakeAssignment.child = &fakelhs;

	AST_Print(&fakeAssignment, 0);
	walkAssignment_0(&fakeAssignment, block, scope, TACIndex, tempNum);
}

void walkSubExpression_0(struct AST *tree,
						 struct BasicBlock *block,
						 struct Scope *scope,
						 int *TACIndex,
						 int *tempNum,
						 struct TACOperand *destinationOperand)
{
	switch (tree->type)
	{
	case t_identifier:
		// variable read
		{
			struct VariableEntry *readVariable = Scope_lookupVar(scope, tree);
			populateTACOperandFromVariable(destinationOperand, readVariable);
		}
		break;

	case t_constant:
		// constant
		destinationOperand->name.str = tree->value;
		destinationOperand->type.basicType = selectVariableTypeForLiteral(tree->value);
		destinationOperand->permutation = vp_literal;
		break;

	case t_char_literal:
		// char literal
		{
			char literalAsNumber[8];
			sprintf(literalAsNumber, "%d", tree->value[0]);
			destinationOperand->name.str = Dictionary_LookupOrInsert(parseDict, literalAsNumber);
			destinationOperand->type.basicType = vt_uint8;
			destinationOperand->permutation = vp_literal;
		}
		break;

	case t_string_literal:
		// string literal
		walkStringLiteral_0(tree, block, scope, destinationOperand);
		break;

	case t_lParen:
		// function call
		walkFunctionCall_0(tree, block, scope, TACIndex, tempNum, destinationOperand);
		break;

	case t_plus:
	case t_minus:
	case t_lThan:
	// case t_bin_lThanE:
	case t_gThan:
		// case t_bin_gThanE:
		// case t_bin_equals:
		// case t_bin_notEquals:
		{
			struct TACOperand *expressionResult = walkExpression_0(tree, block, scope, TACIndex, tempNum);
			*destinationOperand = *expressionResult;
		}
		break;

	case t_lBracket:
		// array reference
		{
			struct TACOperand *arrayRefResult = walkArrayRef_0(tree, block, scope, TACIndex, tempNum);
			*destinationOperand = *arrayRefResult;
		}
		break;

	case t_star:
		// '*' as dereference operator
		{
			struct TACOperand *dereferenceResult = walkDereference_0(tree, block, scope, TACIndex, tempNum);
			*destinationOperand = *dereferenceResult;
		}
		break;

	case t_reference:
		// '&' as reference (address-of) operator
		{
			struct TACOperand *addrOfResult = walkAddrOf_0(tree, block, scope, TACIndex, tempNum);
			*destinationOperand = *addrOfResult;
		}
		break;

	default:
		ErrorWithAST(ERROR_INTERNAL, tree, "Incorrect AST type (%s) seen while linearizing subexpression!\n", getTokenName(tree->type));
		break;
	}
}

void walkFunctionCall_0(struct AST *tree,
						struct BasicBlock *block,
						struct Scope *scope,
						int *TACIndex,
						int *tempNum,
						struct TACOperand *destinationOperand)
{
	if (tree->type != t_lParen)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkFunctionCall!\n", getTokenName(tree->type));
	}

	struct FunctionEntry *calledFunction = Scope_lookupFun(scope, tree->child);

	if (destinationOperand != NULL && (calledFunction->returnType.basicType == vt_null))
	{
		ErrorWithAST(ERROR_CODE, tree, "Attempt to use return value of function %s (returning void)\n", calledFunction->name);
	}

	struct Stack *argumentTrees = Stack_New();
	struct AST *argumentRunner = tree->child->sibling;
	while (argumentRunner != NULL && argumentRunner->type != t_rParen)
	{
		Stack_Push(argumentTrees, argumentRunner);
		argumentRunner = argumentRunner->sibling;
	}

	if ((argumentRunner == NULL) || (argumentRunner->type != t_rParen))
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Expected t_rParen at end of arguments for function call %s\n", tree->child->value);
	}

	int argIndex = 0;
	while (argumentTrees->size > 0)
	{
		struct AST *pushedArgument = Stack_Pop(argumentTrees);
		struct TACLine *push = newTACLine(*TACIndex, tt_push, pushedArgument);
		walkSubExpression_0(pushedArgument, block, scope, TACIndex, tempNum, &push->operands[0]);

		struct VariableEntry *expectedArgument = calledFunction->arguments->data[argIndex];

		if (Type_CompareAllowImplicitWidening(TAC_GetTypeOfOperand(push, 0), &expectedArgument->type))
		{
			ErrorWithAST(ERROR_CODE, pushedArgument, "Error in argument %s passed to function %s!\n\tExpected %s, got %s\n",
						 expectedArgument->name,
						 calledFunction->name,
						 Type_GetName(&expectedArgument->type),
						 Type_GetName(TAC_GetTypeOfOperand(push, 0)));
		}

		// allow us to automatically widen
		if (Scope_getSizeOfType(scope, TAC_GetTypeOfOperand(push, 0)) <= Scope_getSizeOfType(scope, &expectedArgument->type))
		{
			// char *gottenName = Type_GetName(TAC_GetTypeOfOperand(push, 0));
			// char *expectedName = Type_GetName(&expectedArgument->type);

			// TODO: emit warning here
			push->operands[0].castAsType = expectedArgument->type;
		}
		else
		{
			char *convertFromType = Type_GetName(&push->operands[0].type);
			char *convertToType = Type_GetName(&expectedArgument->type);
			ErrorWithAST(ERROR_CODE, pushedArgument, "Potential narrowing conversion passed to argument %s of function %s\n\tConversion from %s to %s\n",
						 expectedArgument->name,
						 calledFunction->name,
						 convertFromType,
						 convertToType);
		}

		push->index = (*TACIndex)++;
		BasicBlock_append(block, push);
		argIndex++;
	}
	Stack_Free(argumentTrees);

	if (argIndex != calledFunction->arguments->size)
	{
		ErrorWithAST(ERROR_CODE, tree, "Error in call to function %s - expected %d arguments, saw %d!\n", calledFunction->name, calledFunction->arguments->size, argIndex);
	}

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

struct TACOperand *walkExpression_0(struct AST *tree,
									struct BasicBlock *block,
									struct Scope *scope,
									int *TACIndex,
									int *tempNum)
{
	// generically set to tt_add, we will actually set the operation within switch cases
	struct TACLine *expression = newTACLine(*TACIndex, tt_subtract, tree);

	expression->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
	expression->operands[0].permutation = vp_temp;

	switch (tree->type)
	{
	case t_plus:
		expression->reorderable = 1;
		expression->operation = tt_add;
		// fall through, having set to plus and reorderable
	case t_minus:
		// basic arithmetic
		{
			walkSubExpression_0(tree->child, block, scope, TACIndex, tempNum, &expression->operands[1]);

			if (TAC_GetTypeOfOperand(expression, 1)->indirectionLevel > 0)
			{
				struct TACLine *scaleMultiply = setUpScaleMultiplication(tree, scope, TACIndex, tempNum, TAC_GetTypeOfOperand(expression, 1));
				walkSubExpression_0(tree->child->sibling, block, scope, TACIndex, tempNum, &scaleMultiply->operands[1]);

				scaleMultiply->operands[0].type = scaleMultiply->operands[1].type;
				copyTACOperandDecayArrays(&expression->operands[2], &scaleMultiply->operands[0]);

				scaleMultiply->index = (*TACIndex)++;
				BasicBlock_append(block, scaleMultiply);
			}
			else
			{
				walkSubExpression_0(tree->child->sibling, block, scope, TACIndex, tempNum, &expression->operands[2]);
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
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkExpression!\n", getTokenName(tree->type));
	}

	expression->index = (*TACIndex)++;
	BasicBlock_append(block, expression);

	return &expression->operands[0];
}

struct TACOperand *walkArrayRef_0(struct AST *tree,
								  struct BasicBlock *block,
								  struct Scope *scope,
								  int *TACIndex,
								  int *tempNum)
{
	if (tree->type != t_lBracket)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkArrayRef!\n", getTokenName(tree->type));
	}

	struct AST *arrayBase = tree->child;
	struct AST *arrayIndex = tree->child->sibling;
	if (arrayBase->type != t_identifier)
	{
		ErrorWithAST(ERROR_INTERNAL, arrayBase, "Invalid AST type (%s) as child of arrayref\n", getTokenName(arrayBase->type));
	}

	struct TACLine *arrayRefTAC = newTACLine((*TACIndex), tt_memr_3, tree);

	struct VariableEntry *arrayVariable = Scope_lookupVar(scope, arrayBase);
	populateTACOperandFromVariable(&arrayRefTAC->operands[1], arrayVariable);

	copyTACOperandDecayArrays(&arrayRefTAC->operands[0], &arrayRefTAC->operands[1]);
	arrayRefTAC->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
	arrayRefTAC->operands[0].permutation = vp_temp;
	arrayRefTAC->operands[0].type.indirectionLevel--;

	if (arrayIndex->type == t_constant)
	{
		arrayRefTAC->operation = tt_memr_2;

		int indexSize = atoi(arrayIndex->value);
		indexSize *= 1 << alignSize(Scope_getSizeOfArrayElement(scope, arrayVariable));

		arrayRefTAC->operands[2].name.val = indexSize;
		arrayRefTAC->operands[2].permutation = vp_literal;
		arrayRefTAC->operands[2].type.basicType = selectVariableTypeForNumber(arrayRefTAC->operands[2].name.val);
	}
	// otherwise, the index is either a variable or subexpression
	else
	{
		// set the scale for the array access
		arrayRefTAC->operands[3].name.val = alignSize(Scope_getSizeOfArrayElement(scope, arrayVariable));
		arrayRefTAC->operands[3].permutation = vp_literal;
		arrayRefTAC->operands[3].type.basicType = selectVariableTypeForNumber(arrayRefTAC->operands[3].name.val);

		walkSubExpression_0(arrayIndex, block, scope, TACIndex, tempNum, &arrayRefTAC->operands[2]);
	}

	arrayRefTAC->index = (*TACIndex)++;
	BasicBlock_append(block, arrayRefTAC);
	return &arrayRefTAC->operands[0];
}

struct TACOperand *walkDereference_0(struct AST *tree,
									 struct BasicBlock *block,
									 struct Scope *scope,
									 int *TACIndex,
									 int *tempNum)
{
	if (tree->type != t_star)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkDereference!\n", getTokenName(tree->type));
	}

	struct TACLine *dereference = newTACLine(*tempNum, tt_dereference, tree);

	switch (tree->child->type)
	{
	case t_plus:
	case t_minus:
	{
		walkPointerArithmetic_0(tree->child, block, scope, TACIndex, tempNum, &dereference->operands[1]);
	}
	break;

	default:
		walkSubExpression_0(tree->child, block, scope, TACIndex, tempNum, &dereference->operands[1]);
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

struct TACOperand *walkAddrOf_0(struct AST *tree,
								struct BasicBlock *block,
								struct Scope *scope,
								int *TACIndex,
								int *tempNum)
{
	if (tree->type != t_reference)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkDereference!\n", getTokenName(tree->type));
	}

	struct TACLine *addrOfLine = newTACLine(*TACIndex, tt_addrof, tree);
	addrOfLine->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
	addrOfLine->operands[0].permutation = vp_temp;

	switch (tree->child->type)
	{
	case t_identifier:
		// look up the variable entry and ensure that we will spill it to the stack since we take its address
		{
			struct VariableEntry *addrTakenOf = Scope_lookupVar(scope, tree->child);
			if(addrTakenOf->type.arraySize > 0)
			{
				ErrorWithAST(ERROR_CODE, tree->child, "Can't take address of local array %s!\n", addrTakenOf->name);
			}
			addrTakenOf->mustSpill = 1;
			walkSubExpression_0(tree->child, block, scope, TACIndex, tempNum, &addrOfLine->operands[1]);
			// copyTACOperandDecayArrays(&addrOfLine->operands[1], &addrOfLine->operands[1]);
		}
		break;

	case t_lBracket:
	{
		struct AST *arrayBase = tree->child->child;
		struct AST *arrayIndex = tree->child->child->sibling;
		if (arrayBase->type != t_identifier)
		{
			ErrorWithAST(ERROR_INTERNAL, arrayBase, "Invalid AST type (%s) as child of arrayref\n", getTokenName(arrayBase->type));
		}

		struct VariableEntry *arrayVariable = Scope_lookupVar(scope, arrayBase);
		populateTACOperandFromVariable(&addrOfLine->operands[1], arrayVariable);

		if (arrayIndex->type == t_constant)
		{
			addrOfLine->operation = tt_lea_2;

			int indexSize = atoi(arrayIndex->value);
			indexSize *= 1 << alignSize(Scope_getSizeOfArrayElement(scope, arrayVariable));

			addrOfLine->operands[2].name.val = indexSize;
			addrOfLine->operands[2].permutation = vp_literal;
			addrOfLine->operands[2].type.basicType = selectVariableTypeForNumber(addrOfLine->operands[2].name.val);

		}
		// otherwise, the index is either a variable or subexpression
		else
		{
			addrOfLine->operation = tt_lea_3;

			// set the scale for the array access
			addrOfLine->operands[3].name.val = alignSize(Scope_getSizeOfArrayElement(scope, arrayVariable));
			addrOfLine->operands[3].permutation = vp_literal;
			addrOfLine->operands[3].type.basicType = selectVariableTypeForNumber(addrOfLine->operands[3].name.val);

			walkSubExpression_0(arrayIndex, block, scope, TACIndex, tempNum, &addrOfLine->operands[2]);
		}
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

void walkPointerArithmetic_0(struct AST *tree,
							 struct BasicBlock *block,
							 struct Scope *scope,
							 int *TACIndex,
							 int *tempNum,
							 struct TACOperand *destinationOperand)
{
	if ((tree->type != t_plus) && (tree->type != t_minus))
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkPointerArithmetic!\n", getTokenName(tree->type));
	}

	struct AST *pointerArithLHS = tree->child;
	struct AST *pointerArithRHS = tree->child->sibling;

	struct TACLine *pointerArithmetic = newTACLine(*TACIndex, tt_add, tree->child);
	if (tree->type == t_minus)
	{
		pointerArithmetic->operation = tt_subtract;
	}

	walkSubExpression_0(pointerArithLHS, block, scope, TACIndex, tempNum, &pointerArithmetic->operands[1]);
	copyTACOperandDecayArrays(&pointerArithmetic->operands[0], &pointerArithmetic->operands[1]);
	pointerArithmetic->operands[0].name.str = TempList_Get(temps, (*tempNum)++);

	// pointerArithmetic->operands[0].type = pointerArithmetic->operands[1].type;
	// pointerArithmetic->operands[0].type.arraySize = 0;
	pointerArithmetic->operands[0].permutation = vp_temp;

	struct TACLine *scaleMultiplication = setUpScaleMultiplication(pointerArithRHS,
																   scope,
																   TACIndex,
																   tempNum,
																   TAC_GetTypeOfOperand(pointerArithmetic, 1));

	walkSubExpression_0(pointerArithRHS, block, scope, TACIndex, tempNum, &scaleMultiplication->operands[1]);

	copyTACOperandTypeDecayArrays(&scaleMultiplication->operands[0], &scaleMultiplication->operands[1]);

	copyTACOperandDecayArrays(&pointerArithmetic->operands[2], &scaleMultiplication->operands[0]);

	scaleMultiplication->index = (*TACIndex)++;
	BasicBlock_append(block, scaleMultiplication);
	pointerArithmetic->index = (*TACIndex)++;
	BasicBlock_append(block, pointerArithmetic);

	*destinationOperand = pointerArithmetic->operands[0];
}

void walkAsmBlock_0(struct AST *tree,
					struct BasicBlock *block,
					struct Scope *scope,
					int *TACIndex,
					int *tempNum)
{
	if (tree->type != t_asm)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkAsmBlock!\n", getTokenName(tree->type));
	}

	struct AST *asmRunner = tree->child;
	while (asmRunner != NULL)
	{
		struct TACLine *asmLine = newTACLine((*TACIndex)++, tt_asm, asmRunner);
		asmLine->operands[0].name.str = asmRunner->value;

		BasicBlock_append(block, asmLine);

		asmRunner = asmRunner->sibling;
	}
}

void walkStringLiteral_0(struct AST *tree,
						 struct BasicBlock *block,
						 struct Scope *scope,
						 struct TACOperand *destinationOperand)
{
	if (tree->type != t_string_literal)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkStringLiteral!\n", getTokenName(tree->type));
	}

	// Scope_createStringLiteral will modify the value of our AST node
	// it inserts underscores in place of spaces and other modifications to turn the literal into a name that the symtab can use
	// but first, it copies the string exactly as-is so it knows what the string object should be initialized to
	char *stringName = tree->value;
	char *stringValue = strdup(stringName);
	int stringSize = strlen(stringName) + 1;

	for (int i = 0; i < stringSize - 1; i++)
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
		stringType.basicType = vt_uint8;
		stringType.arraySize = stringSize;
		stringType.indirectionLevel = 1;

		stringLiteralEntry = Scope_createVariable(scope, &fakeStringTree, &stringType, 1, 0, 0);

		struct Type *realStringType = &stringLiteralEntry->type;
		realStringType->initializeArrayTo = malloc(stringSize * sizeof(char *));
		for (int i = 0; i < stringSize; i++)
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
	destinationOperand->type.arraySize = 0;
}