#include "linearizer.h"

// #define LITERAL_VARIABLE_TYPE vt_uint32

// given a raw size of an object, find the nearest power-of-two aligned size
int alignSize(int nBytes)
{
	int i = 0;
	while ((nBytes > (0b1 << i)) > 0)
	{
		i++;
	}
	return i;
}

enum variableTypes selectVariableTypeForNumber(int num)
{
	if (num < 256)
	{
		return vt_uint8;
	}
	else if (num < 65536)
	{
		return vt_uint16;
	}
	else
	{
		return vt_uint32;
	}
}

enum variableTypes selectVariableTypeForLiteral(char *literal)
{
	int literalAsNumber = atoi(literal);
	return selectVariableTypeForNumber(literalAsNumber);
}

void populateTACOperandFromVariable(struct TACOperand *o, struct VariableEntry *e)
{
	o->indirectionLevel = e->indirectionLevel;
	o->name.str = e->name;
	o->permutation = vp_standard;
	o->type = e->type;
}

char checkTACOperandFromVariable(struct TACOperand *o, struct VariableEntry *e)
{
	if (o->type != e->type)
	{
		return 1;
	}

	if (o->indirectionLevel != e->indirectionLevel)
	{
		return 1;
	}

	return 0;
}

/*
 * These functions walk the AST and convert it to three-address code
 */
struct TempList *temps;
extern struct Dictionary *parseDict;
struct SymbolTable *linearizeProgram(struct AST *program)
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
			walkVariableDeclaration(programRunner, globalBlock, programTable->globalScope, &globalTACIndex, &globalTempNum, 0);
			break;

		case t_single_equals:
			walkAssignment(programRunner, globalBlock, programTable->globalScope, &globalTACIndex, &globalTempNum);
			break;

		case t_fun:
			walkFunctionDeclaration(programRunner, programTable->globalScope);
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
struct VariableEntry *walkVariableDeclaration(struct AST *tree,
											  struct BasicBlock *block,
											  struct Scope *scope,
											  int *TACIndex,
											  int *tempNum,
											  char isArgument)
{
	printf("walkVariableDeclaration: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);

	struct TACLine *declarationLine = newTACLine((*TACIndex)++, tt_declare, tree);
	enum variableTypes declaredType;
	switch (tree->type)
	{
	case t_uint8:
		declaredType = vt_uint8;
		break;

	case t_uint16:
		declaredType = vt_uint16;
		break;

	case t_uint32:
		declaredType = vt_uint32;
		break;

	default:
		ErrorWithAST(ERROR_INTERNAL, tree, "Malformed AST seen in declaration!");
	}

	struct AST *declaredTree = NULL;
	int declaredIndirectionLevel = scrapePointers(tree->child, &declaredTree);
	int declaredArraySize = 1;

	// if we are declaring an array, set the string with the size as the second operand
	if (declaredTree->type == t_lBracket)
	{
		declaredTree = declaredTree->child;
		char *arraySizeString = declaredTree->sibling->value;
		declarationLine->operands[1].name.str = arraySizeString;
		declaredArraySize = atoi(arraySizeString);
		declarationLine->operands[1].permutation = vp_literal;
		declarationLine->operands[1].type = selectVariableTypeForLiteral(arraySizeString);
	}

	struct VariableEntry *declaredVariable = Scope_createVariable(scope,
																  declaredTree,
																  declaredType,
																  declaredIndirectionLevel,
																  declaredArraySize,
																  (scope->parentScope == NULL),
																  declarationLine->index,
																  isArgument);
	declarationLine->operands[0].name.str = declaredTree->value;
	declarationLine->operands[0].type = declaredType;
	declarationLine->operands[0].indirectionLevel = declaredIndirectionLevel;

	BasicBlock_append(block, declarationLine);
	return declaredVariable;
}

void walkArgumentDeclaration(struct AST *tree,
							 struct BasicBlock *block,
							 int *TACIndex,
							 int *tempNum,
							 struct FunctionEntry *fun)
{
	printf("walkArgumentDeclaration: %s:%d:%d\n", tree->sourceFile, tree->sourceLine, tree->sourceCol);
	struct VariableEntry *delcaredArgument = walkVariableDeclaration(tree, block, fun->mainScope, TACIndex, tempNum, 1);

	delcaredArgument->assignedAt = 0;
	delcaredArgument->isAssigned = 1;
	Stack_Push(fun->arguments, delcaredArgument);
}

void walkFunctionDeclaration(struct AST *tree,
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
	struct AST *functionNameTree = tree->child->child;
	struct ScopeMember *lookedUpFunction = Scope_lookup(scope, functionNameTree->value);
	struct FunctionEntry *parsedFunc = NULL;
	struct FunctionEntry *existingFunc = NULL;

	if (lookedUpFunction != NULL)
	{
		existingFunc = lookedUpFunction->entry;
		parsedFunc = FunctionEntry_new(scope, functionNameTree->value, returnType, returnIndirectionLevel);
	}
	else
	{
		parsedFunc = Scope_createFunction(scope, functionNameTree->value, returnType, returnIndirectionLevel);
		parsedFunc->mainScope->parentScope = scope;
		// record return type
		parsedFunc->returnType = returnType;
		parsedFunc->returnIndirectionLevel = returnIndirectionLevel;
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
			walkArgumentDeclaration(argumentRunner, block, &TACIndex, &tempNum, parsedFunc);
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

			char *existingReturnType = Scope_getNameOfType(scope, existingFunc->returnType, existingFunc->returnIndirectionLevel);
			printf("\t%s %s(", existingReturnType, existingFunc->name);
			free(existingReturnType);
			for (int i = 0; i < existingFunc->arguments->size; i++)
			{
				struct VariableEntry *existingArg = existingFunc->arguments->data[i];

				char *argType = Scope_getNameOfType(scope, existingArg->type, existingArg->indirectionLevel);
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
			char *parsedReturnType = Scope_getNameOfType(scope, returnType, returnIndirectionLevel);
			printf("\n\t%s %s(", parsedReturnType, parsedFunc->name);
			free(parsedReturnType);
			for (int i = 0; i < parsedFunc->arguments->size; i++)
			{
				struct VariableEntry *parsedArg = parsedFunc->arguments->data[i];

				char *argType = Scope_getNameOfType(scope, parsedArg->type, parsedArg->indirectionLevel);
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
			walkFunctionDefinition(definition, existingFunc);
		}
		else
		{
			parsedFunc->isDefined = 1;
			walkFunctionDefinition(definition, parsedFunc);
		}
	}
}

void walkFunctionDefinition(struct AST *tree,
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
	walkScope(tree, block, fun->mainScope, &TACIndex, &tempNum, &labelNum, -1);
}

void walkScope(struct AST *tree,
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
			walkVariableDeclaration(scopeRunner, block, scope, TACIndex, tempNum, 0);
			break;

		case t_single_equals:
			walkAssignment(scopeRunner, block, scope, TACIndex, tempNum);
			break;

		case t_while:
			// while loop
			{
				struct BasicBlock *afterWhileBlock = BasicBlock_new((*labelNum)++);
				walkWhileLoop(scopeRunner, block, scope, TACIndex, tempNum, labelNum, afterWhileBlock->labelNum);
				block = afterWhileBlock;
				Scope_addBasicBlock(scope, afterWhileBlock);
			}
			break;

		case t_if:
			// if statement
			{
				struct BasicBlock *afterIfBlock = BasicBlock_new((*labelNum)++);
				walkIfStatement(scopeRunner, block, scope, TACIndex, tempNum, labelNum, afterIfBlock->labelNum);
				block = afterIfBlock;
				Scope_addBasicBlock(scope, afterIfBlock);
			}
			break;

		case t_lParen:
			walkFunctionCall(scopeRunner, block, scope, TACIndex, tempNum, NULL);
			break;

		case t_lCurly:
			// subscope
			{
				struct Scope *subScope = Scope_createSubScope(scope);
				struct BasicBlock *afterSubScopeBlock = BasicBlock_new((*labelNum)++);
				walkScope(scopeRunner, block, subScope, TACIndex, tempNum, labelNum, afterSubScopeBlock->labelNum);
				block = afterSubScopeBlock;
				Scope_addBasicBlock(scope, afterSubScopeBlock);
			}
			break;

			// case t_fun:
			// walkFunctionDeclaration(scopeRunner, programTable->globalScope);
			// break;

		case t_return:
			// return
			{
				struct TACLine *returnLine = newTACLine(*TACIndex, tt_return, scopeRunner);
				if (scopeRunner->child != NULL)
				{
					walkSubExpression(scopeRunner->child, block, scope, TACIndex, tempNum, &returnLine->operands[0]);
				}

				returnLine->index = (*TACIndex)++;

				BasicBlock_append(block, returnLine);
			}
			break;

		case t_asm:
			walkAsmBlock(scopeRunner, block, scope, TACIndex, tempNum);
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

void walkConditionCheck(struct AST *tree,
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
			walkSubExpression(tree->child, block, scope, TACIndex, tempNum, &compareOperation->operands[1]);
			walkSubExpression(tree->child->sibling, block, scope, TACIndex, tempNum, &compareOperation->operands[2]);
		}
		break;

	case t_not:
		// NOT any condition (!)
		{
			walkSubExpression(tree->child, block, scope, TACIndex, tempNum, &compareOperation->operands[1]);
			compareOperation->operands[2].name.val = 0;
			compareOperation->operands[2].permutation = vp_literal;
			compareOperation->operands[2].type = vt_uint32;
			compareOperation->operands[2].indirectionLevel = 0;
		}
		break;

	default:
		// any other sort of condition - just some expression
		{
			walkSubExpression(tree, block, scope, TACIndex, tempNum, &compareOperation->operands[1]);
			compareOperation->operands[2].name.val = 0;
			compareOperation->operands[2].permutation = vp_literal;
			compareOperation->operands[2].type = vt_uint32;
			compareOperation->operands[2].indirectionLevel = 0;
			condFalseJump->operation = tt_jz;
		}
		break;
	}
	compareOperation->index = (*TACIndex)++;
	BasicBlock_append(block, compareOperation);

	condFalseJump->index = (*TACIndex)++;
	BasicBlock_append(block, condFalseJump);
}

void walkWhileLoop(struct AST *tree,
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

	walkConditionCheck(tree->child, whileBlock, whileScope, TACIndex, tempNum, controlConvergesToLabel);

	int endWhileLabel = (*labelNum)++;

	walkScope(tree->child->sibling, whileBlock, whileScope, TACIndex, tempNum, labelNum, endWhileLabel);

	struct TACLine *whileLoopJump = newTACLine((*TACIndex)++, tt_jmp, tree);
	whileLoopJump->operands[0].name.val = whileBlock->labelNum;

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
	if (tree->type != t_if)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkIfStatement!\n", getTokenName(tree->type));
	}

	// if we have an else block
	if (tree->child->sibling->sibling != NULL)
	{
		int elseLabel = (*labelNum)++;
		walkConditionCheck(tree->child, block, scope, TACIndex, tempNum, elseLabel);

		struct Scope *ifScope = Scope_createSubScope(scope);
		struct BasicBlock *ifBlock = BasicBlock_new((*labelNum)++);
		Scope_addBasicBlock(scope, ifBlock);

		struct TACLine *enterIfJump = newTACLine((*TACIndex)++, tt_jmp, tree);
		enterIfJump->operands[0].name.val = ifBlock->labelNum;
		BasicBlock_append(block, enterIfJump);

		walkScope(tree->child->sibling, ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);

		struct Scope *elseScope = Scope_createSubScope(scope);
		struct BasicBlock *elseBlock = BasicBlock_new(elseLabel);
		Scope_addBasicBlock(scope, elseBlock);
		walkScope(tree->child->sibling->sibling, elseBlock, elseScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
	}
	// no else block
	else
	{
		walkConditionCheck(tree->child, block, scope, TACIndex, tempNum, controlConvergesToLabel);

		struct Scope *ifScope = Scope_createSubScope(scope);
		struct BasicBlock *ifBlock = BasicBlock_new((*labelNum)++);
		Scope_addBasicBlock(scope, ifBlock);

		struct TACLine *enterIfJump = newTACLine((*TACIndex)++, tt_jmp, tree);
		enterIfJump->operands[0].name.val = ifBlock->labelNum;
		BasicBlock_append(block, enterIfJump);

		walkScope(tree->child->sibling, ifBlock, ifScope, TACIndex, tempNum, labelNum, controlConvergesToLabel);
	}
}

void walkAssignment(struct AST *tree,
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
	// walk the RHS of the assignment as a subexpression and save the operand for later
	walkSubExpression(rhs, block, scope, TACIndex, tempNum, &assignedValue);

	struct VariableEntry *assignedVariable = NULL;
	switch (lhs->type)
	{
	case t_uint8:
	case t_uint16:
	case t_uint32:
		assignedVariable = walkVariableDeclaration(lhs, block, scope, TACIndex, tempNum, 0);
		populateTACOperandFromVariable(&assignment->operands[0], assignedVariable);
		assignment->operands[1] = assignedValue;
		break;

	case t_identifier:
		assignedVariable = Scope_lookupVar(scope, lhs);
		populateTACOperandFromVariable(&assignment->operands[0], assignedVariable);
		assignment->operands[1] = assignedValue;
		break;

	// TODO: generate optimized addressing modes for arithmetic
	case t_star:
	{
		struct AST *writtenPointer = lhs->child;
		struct TACOperand writtenAddress;
		walkSubExpression(writtenPointer, block, scope, TACIndex, tempNum, &writtenAddress);
		assignment->operation = tt_memw_1;
		assignment->operands[0] = writtenAddress;
		assignment->operands[1] = assignedValue;
	}
	break;

	case t_lBracket:
	{
		struct AST *arrayName = lhs->child;
		struct AST *arrayIndex = lhs->child->sibling;
		struct VariableEntry *arrayVariable = Scope_lookupVar(scope, arrayName);

		if (arrayVariable->indirectionLevel < 1)
		{
			ErrorWithAST(ERROR_CODE, arrayName, "Use of non-pointer variable %s as array!\n", arrayName->value);
		}

		assignment->operation = tt_memw_3;
		populateTACOperandFromVariable(&assignment->operands[0], arrayVariable);

		assignment->operands[2].permutation = vp_literal;
		assignment->operands[2].indirectionLevel = 0;
		assignment->operands[2].name.val = alignSize(Scope_getSizeOfArrayElement(scope, arrayVariable));

		walkSubExpression(arrayIndex, block, scope, TACIndex, tempNum, &assignment->operands[1]);

		assignment->operands[3] = assignedValue;
	}
	break;

	default:
		ErrorWithAST(ERROR_INTERNAL, lhs, "Unexpected AST (%s) seen in walkAssignment!\n", lhs->value);
		break;
	}

	assignment->index = (*TACIndex)++;
	BasicBlock_append(block, assignment);

	// grab the TAC we just generated for the RHS of the expression
	// struct TACLine *RHSTac = currentBlock->TACList->tail->data;

	// switch (lhs->type)

	// // array element reference
	// case t_lBracket:
	// {
	// 	struct TACLine *finalAssignment = newTACLine(m.currentTACIndex, tt_memw_3, LHS);

	// 	struct VariableEntry *assignedArray = Scope_lookupVar(m.scope, LHS->child);

	// 	finalAssignment->operands[0].name.str = assignedArray->name;
	// 	finalAssignment->operands[0].type = assignedArray->type;
	// 	finalAssignment->operands[0].indirectionLevel = assignedArray->indirectionLevel;
	// 	finalAssignment->operands[0].permutation = vp_standard;

	// 	RHSTac->operands[0].name.str = TempList_Get(temps, *m.tempNum);
	// 	RHSTac->operands[0].permutation = vp_temp;
	// 	(*m.tempNum)++;
	// 	RHSTac->operands[0].type = RHSTac->operands[1].type;
	// 	RHSTac->operands[0].indirectionLevel = RHSTac->operands[1].indirectionLevel;

	// 	// copy the temp from the RHS to the source of the memory write operation
	// 	finalAssignment->operands[3] = RHSTac->operands[0];

	// 	// handle the scaling value (scale by size of array element)
	// 	finalAssignment->operands[2].indirectionLevel = 0;
	// 	finalAssignment->operands[2].permutation = vp_literal;
	// 	finalAssignment->operands[2].name.val = alignSize(Scope_getSizeOfVariable(m.scope, LHS->child));
	// 	finalAssignment->operands[2].type = selectVariableTypeForNumber(finalAssignment->operands[2].name.val);

	// 	// handle the base (array start)

	// 	struct AST *arrayIndex = LHS->child->sibling;
	// 	switch (arrayIndex->type)
	// 	{
	// 	case t_identifier:
	// 	{
	// 		struct VariableEntry *indexVariable = Scope_lookupVar(m.scope, arrayIndex);

	// 		finalAssignment->operands[1].indirectionLevel = 0;
	// 		finalAssignment->operands[1].name.str = arrayIndex->value;
	// 		finalAssignment->operands[1].permutation = vp_standard;
	// 		finalAssignment->operands[1].type = indexVariable->type;
	// 	}
	// 	break;

	// 	// switch to addressing mode 2 if we have a constant
	// 	case t_constant:
	// 	{
	// 		finalAssignment->operation = tt_memw_2;
	// 		// finalAssignment->operands[0].indirectionLevel--;
	// 		finalAssignment->operands[1].indirectionLevel = 0;

	// 		int indexSize = atoi(arrayIndex->value);
	// 		// indexSize *= Scope_getSizeOfVariable(m.scope, LHS->child);

	// 		if (assignedArray->indirectionLevel == 1)
	// 		{
	// 			// TODO: this will cause problems with complex types
	// 			indexSize *= GetSizeOfPrimitive(assignedArray->type);
	// 		}
	// 		else
	// 		{
	// 			indexSize *= Scope_getSizeOfVariable(m.scope, LHS->child);
	// 		}

	// 		finalAssignment->operands[1].name.val = indexSize;
	// 		finalAssignment->operands[1].permutation = vp_literal;
	// 		finalAssignment->operands[1].type = selectVariableTypeForNumber(finalAssignment->operands[1].name.val);

	// 		finalAssignment->operands[2] = finalAssignment->operands[3];

	// 		memset(&finalAssignment->operands[3], 0, sizeof(struct TACOperand));
	// 	}
	// 	break;

	// 	// if not identifier or constant, assume it must be some sort of expression inside the brackets
	// 	default:
	// 	{
	// 		struct LinearizationMetadata expressionMetadata = m;
	// 		expressionMetadata.ast = arrayIndex;

	// 		m.currentTACIndex = walkExpression(expressionMetadata);

	// 		struct TACLine *finalArithmeticLine = m.currentBlock->TACList->tail->data;

	// 		finalAssignment->operands[1] = finalArithmeticLine->operands[0];
	// 	}
	// 	break;
	// 	}
	// 	finalAssignment->index = m.currentTACIndex++;
	// 	BasicBlock_append(m.currentBlock, finalAssignment);
	// }
	// break;

	// default:
	// 	ErrorWithAST(ERROR_INTERNAL, LHS, "Malformed AST on LHS of assignment");
	// }
}

void walkSubExpression(struct AST *tree,
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
		destinationOperand->type = selectVariableTypeForLiteral(tree->value);
		destinationOperand->permutation = vp_literal;
		destinationOperand->indirectionLevel = 0;
		break;

	case t_char_literal:
		// char literal
		{
			char literalAsNumber[8];
			sprintf(literalAsNumber, "%d", tree->value[0]);
			destinationOperand->name.str = Dictionary_LookupOrInsert(parseDict, literalAsNumber);
			destinationOperand->type = selectVariableTypeForLiteral(literalAsNumber);
			destinationOperand->permutation = vp_literal;
			destinationOperand->indirectionLevel = 0;
		}
		break;

	case t_string_literal:
		// string literal
		destinationOperand->type = vt_uint8;
		destinationOperand->permutation = vp_objptr;
		destinationOperand->indirectionLevel = 1;
		destinationOperand->name.str = tree->value;
		break;

	case t_lParen:
		// function call
		walkFunctionCall(tree, block, scope, TACIndex, tempNum, destinationOperand);
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
			struct TACOperand *expressionResult = walkExpression(tree, block, scope, TACIndex, tempNum);
			*destinationOperand = *expressionResult;
		}
		break;

	case t_lBracket:
		// array reference
		{
			struct TACOperand *arrayRefResult = walkArrayRef(tree, block, scope, TACIndex, tempNum);
			*destinationOperand = *arrayRefResult;
		}
		break;

	case t_star:
	{
		struct TACOperand *dereferenceResult = walkDereference(tree, block, scope, TACIndex, tempNum);
		*destinationOperand = *dereferenceResult;
	}
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
	if (tree->type != t_lParen)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkFunctionCall!\n", getTokenName(tree->type));
	}

	struct FunctionEntry *calledFunction = Scope_lookupFun(scope, tree->child);

	if (destinationOperand != NULL && calledFunction->returnType == vt_null)
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
		walkSubExpression(pushedArgument, block, scope, TACIndex, tempNum, &push->operands[0]);

		struct VariableEntry *expectedArgument = calledFunction->arguments->data[argIndex];

		if (push->operands[0].indirectionLevel != expectedArgument->indirectionLevel)
		{
			ErrorWithAST(ERROR_CODE, pushedArgument, "Error in argument %d passed to function %s! Expect %d*, got %d*\n", argIndex, calledFunction->name, expectedArgument->indirectionLevel, push->operands[0].indirectionLevel);
		}

		// allow us to automatically widen
		if(GetSizeOfPrimitive(push->operands[0].type) <= GetSizeOfPrimitive(expectedArgument->type))
		{
			struct TACLine *cast = newTACLine((*TACIndex)++, tt_cast_assign, tree);
			cast->operands[1] = push->operands[0];
			populateTACOperandFromVariable(&cast->operands[0], expectedArgument);
			cast->operands[0].permutation = vp_temp;
			cast->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
			push->operands[0] = cast->operands[0];
			BasicBlock_append(block, cast);
		}
		else
		{
			ErrorWithAST(ERROR_CODE, pushedArgument, "Potential narrowing conversion passed to argument %s of function %s\n", expectedArgument->name, calledFunction->name);
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
		call->operands[0].indirectionLevel = calledFunction->returnIndirectionLevel;
		call->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
		call->operands[0].permutation = vp_temp;

		*destinationOperand = call->operands[0];
	}
}

struct TACOperand *walkExpression(struct AST *tree,
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
			walkSubExpression(tree->child, block, scope, TACIndex, tempNum, &expression->operands[1]);
			walkSubExpression(tree->child->sibling, block, scope, TACIndex, tempNum, &expression->operands[2]);
			expression->index = (*TACIndex)++;

			struct TACOperand *operandA = &expression->operands[1];
			struct TACOperand *operandB = &expression->operands[2];
			if ((operandA->indirectionLevel > 0) && (operandB->indirectionLevel > 0))
			{
				ErrorWithAST(ERROR_CODE, tree, "Arithmetic between 2 pointers is not allowed!\n");
			}

			if (GetSizeOfPrimitive(operandA->type) > GetSizeOfPrimitive(operandB->type))
			{
				expression->operands[0].type = operandA->type;
			}
			else
			{
				expression->operands[0].type = operandB->type;
			}

			if (operandA->indirectionLevel > operandB->indirectionLevel)
			{
				expression->operands[0].indirectionLevel = operandA->indirectionLevel;
			}
			else
			{
				expression->operands[0].indirectionLevel = operandB->indirectionLevel;
			}
		}
		break;

	default:
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkExpression!\n", getTokenName(tree->type));
	}

	BasicBlock_append(block, expression);

	return &expression->operands[0];
}

struct TACOperand *walkArrayRef(struct AST *tree,
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

	arrayRefTAC->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
	arrayRefTAC->operands[0].permutation = vp_temp;

	if (arrayRefTAC->operands[0].indirectionLevel > 0)
	{
		arrayRefTAC->operands[0].indirectionLevel = arrayRefTAC->operands[0].indirectionLevel - 1;
	}
	else
	{
		arrayRefTAC->operands[0].indirectionLevel = 0;
	}
	arrayRefTAC->operands[0].type = arrayRefTAC->operands[1].type;

	if (arrayIndex->type == t_constant)
	{
		arrayRefTAC->operation = tt_memr_2;

		int indexSize = atoi(arrayIndex->value);
		if (arrayVariable->indirectionLevel == 1)
		{
			// TODO: this will cause problems with complex types
			indexSize *= GetSizeOfPrimitive(arrayVariable->type);
		}
		else
		{
			indexSize *= alignSize(Scope_getSizeOfArrayElement(scope, arrayVariable));
		}

		arrayRefTAC->operands[2].name.val = indexSize;
		arrayRefTAC->operands[2].indirectionLevel = 0;
		arrayRefTAC->operands[2].permutation = vp_literal;
		arrayRefTAC->operands[2].type = selectVariableTypeForNumber(arrayRefTAC->operands[2].name.val);
	}
	// otherwise, the index is either a variable or subexpression
	else
	{
		// set the scale for the array access
		// set scale

		arrayRefTAC->operands[3].name.val = alignSize(Scope_getSizeOfArrayElement(scope, arrayVariable));
		arrayRefTAC->operands[3].indirectionLevel = 0;
		arrayRefTAC->operands[3].permutation = vp_literal;
		arrayRefTAC->operands[3].type = selectVariableTypeForNumber(arrayRefTAC->operands[3].name.val);

		walkSubExpression(arrayIndex, block, scope, TACIndex, tempNum, &arrayRefTAC->operands[2]);
	}

	arrayRefTAC->index = (*TACIndex)++;
	BasicBlock_append(block, arrayRefTAC);
	return &arrayRefTAC->operands[0];
}

struct TACOperand *walkDereference(struct AST *tree,
								   struct BasicBlock *block,
								   struct Scope *scope,
								   int *TACIndex,
								   int *tempNum)
{
	ErrorAndExit(ERROR_INTERNAL, "walkDereference not implemented!\n");
	return NULL;
}

void walkAsmBlock(struct AST *tree,
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
		printf("GOT AN ASM [%s]\n", asmRunner->value);

		BasicBlock_append(block, asmLine);

		asmRunner = asmRunner->sibling;
	}
}

// int linearizeASMBlock(struct LinearizationMetadata m)
// {
// 	struct AST *asmRunner = m.ast->child;
// 	while (asmRunner != NULL)
// 	{
// 		struct TACLine *asmLine = newTACLine(m.currentTACIndex++, tt_asm, asmRunner);

// 		asmLine->operands[0].name.str = asmRunner->value;
// 		BasicBlock_append(m.currentBlock, asmLine);
// 		asmRunner = asmRunner->sibling;
// 	}
// 	return m.currentTACIndex;
// }

// int linearizeDereference(struct LinearizationMetadata m)
// {
// 	// TAC index set at bottom of function so it aligns properly with any lines resulting from recursive calls
// 	struct TACLine *thisDereference = newTACLine(m.currentTACIndex, tt_memr_1, m.ast);

// 	thisDereference->operands[0].name.str = TempList_Get(temps, *m.tempNum);
// 	thisDereference->operands[0].permutation = vp_temp;
// 	(*m.tempNum)++;

// 	struct AST *dereferencedExpression = m.ast;
// 	if (dereferencedExpression->type == t_plus || dereferencedExpression->type == t_minus)
// 	{
// 		if (dereferencedExpression->type == t_plus)
// 		{
// 			thisDereference->operation = tt_memr_3;
// 		}
// 		else if (dereferencedExpression->type == t_minus)
// 		{
// 			thisDereference->operation = tt_memr_3_n;
// 		}
// 		else
// 		{
// 			ErrorWithAST(ERROR_CODE, dereferencedExpression, "Illegal expression type (%s) being dereferenced!\n", getTokenName(dereferencedExpression->type));
// 		}

// 		struct LinearizationMetadata pointerArithLHS = m;
// 		pointerArithLHS.ast = dereferencedExpression->child;
// 		m.currentTACIndex = walkSubExpression(pointerArithLHS, thisDereference, 1, 1);

// 		struct LinearizationMetadata pointerArithRHS = m;
// 		pointerArithRHS.ast = dereferencedExpression->child->sibling;
// 		m.currentTACIndex = walkSubExpression(pointerArithRHS, thisDereference, 2, 1);

// 		thisDereference->operands[3].name.val = alignSize(Scope_getSizeOfVariableByString(m.scope, thisDereference->operands[1].name.str, 1));
// 		thisDereference->operands[3].indirectionLevel = 0;
// 		thisDereference->operands[3].permutation = vp_literal;
// 		thisDereference->operands[3].type = selectVariableTypeForNumber(thisDereference->operands[3].name.val);
// 	}
// 	else
// 	{
// 		// no direct pointer arithmetic to consider, just use linearizeSubExpression to populate what we are dereferencing
// 		m.currentTACIndex = walkSubExpression(m, thisDereference, 1, 0);
// 	}

// 	thisDereference->operands[0].type = thisDereference->operands[1].type;

// 	thisDereference->index = m.currentTACIndex++;
// 	int newIndirectionLevel = thisDereference->operands[1].indirectionLevel;
// 	if (newIndirectionLevel > 0)
// 	{
// 		newIndirectionLevel--;
// 	}
// 	else
// 	{
// 		printf("\n%s - ", thisDereference->operands[1].name.str);
// 		ErrorWithAST(ERROR_CODE, m.ast, "Dereference of non-indirect expression or statement!\n");
// 	}

// 	thisDereference->operands[0].indirectionLevel = newIndirectionLevel;
// 	BasicBlock_append(m.currentBlock, thisDereference);
// 	return m.currentTACIndex;
// }

// int linearizeArgumentPushes(struct LinearizationMetadata m, struct FunctionEntry *f)
// {
// 	struct Stack *argumentStack = Stack_New();

// 	struct AST *argumentRunner = m.ast;
// 	while (argumentRunner->type != t_rParen)
// 	{
// 		struct TACLine *thisArgumentPush = newTACLine(m.currentTACIndex, tt_push, m.ast);

// 		struct LinearizationMetadata pushedArgumentMetadata = m;
// 		pushedArgumentMetadata.ast = argumentRunner;
// 		m.currentTACIndex = walkSubExpression(pushedArgumentMetadata, thisArgumentPush, 0, 0);

// 		Stack_Push(argumentStack, thisArgumentPush);
// 		argumentRunner = argumentRunner->sibling;
// 	}

// 	int argumentIndex = 0;
// 	if (argumentStack->size != f->arguments->size)
// 	{
// 		ErrorWithAST(ERROR_CODE, m.ast, "Function %s expects %d arguments but %d were given!", f->name, f->arguments->size, argumentStack->size);
// 	}

// 	while (argumentStack->size > 0)
// 	{
// 		struct VariableEntry *expectedArgument = (struct VariableEntry *)f->arguments->data[argumentIndex];
// 		struct TACLine *thisArgumentPush = Stack_Pop(argumentStack);

// 		if (thisArgumentPush->operands[0].type > expectedArgument->type)
// 		{
// 			ErrorWithAST(ERROR_CODE, thisArgumentPush->correspondingTree, "Argument '%s' to function '%s' has unexpected type!\n", expectedArgument->name, f->name);
// 		}
// 		else
// 		{
// 			int thisIndirectionLevel = thisArgumentPush->operands[0].indirectionLevel;
// 			if (thisIndirectionLevel != expectedArgument->indirectionLevel)
// 			{
// 				char *providedIndirectionLevelStr = malloc(thisIndirectionLevel + 1);
// 				char *expectedIndirectionLevelStr = malloc(expectedArgument->indirectionLevel + 1);
// 				for (int i = 0; i < thisIndirectionLevel; i++)
// 				{
// 					providedIndirectionLevelStr[i] = '*';
// 				}
// 				providedIndirectionLevelStr[thisIndirectionLevel] = '\0';

// 				for (int i = 0; i < expectedArgument->indirectionLevel; i++)
// 				{
// 					expectedIndirectionLevelStr[i] = '*';
// 				}
// 				expectedIndirectionLevelStr[expectedArgument->indirectionLevel] = '\0';

// 				for (struct LinkedListNode *r = m.currentBlock->TACList->head; r != NULL; r = r->next)
// 				{
// 					printTACLine(r->data);
// 					printf("\n");
// 				}
// 				ErrorWithAST(ERROR_CODE, thisArgumentPush->correspondingTree, "Argument '%s' of funciton '%s' expects type %d%s, but is being passed a %d%s\n", expectedArgument->name, f->name, expectedArgument->type, expectedIndirectionLevelStr, thisIndirectionLevel, providedIndirectionLevelStr);
// 			}

// 			if (thisArgumentPush->operands[0].type < expectedArgument->type)
// 			{
// 				struct TACLine *castLine = newTACLine(m.currentTACIndex++, tt_cast_assign, NULL);
// 				// actually use the subexpression we placed as RHS of cast
// 				castLine->operands[1] = thisArgumentPush->operands[0];

// 				// start castTo as the same as what we are casting from
// 				struct TACOperand castTo = castLine->operands[1];

// 				castTo.type = expectedArgument->type;
// 				castTo.name.str = TempList_Get(temps, *m.tempNum);
// 				castTo.permutation = vp_temp;

// 				castLine->operands[0] = castTo;
// 				thisArgumentPush->operands[0] = castTo;

// 				(*m.tempNum)++;
// 				BasicBlock_append(m.currentBlock, castLine);
// 			}
// 		}
// 		thisArgumentPush->index = m.currentTACIndex++;
// 		BasicBlock_append(m.currentBlock, thisArgumentPush);
// 		argumentIndex++;
// 	}

// 	Stack_Free(argumentStack);

// 	return m.currentTACIndex;
// }

// // given an AST node of a function call, generate TAC to evaluate and push the arguments, then call it
// int linearizeFunctionCall(struct LinearizationMetadata m)
// {
// 	char *operand0 = TempList_Get(temps, *m.tempNum);
// 	struct FunctionEntry *calledFunction = Scope_lookupFun(m.scope, m.ast->child);

// 	if (calledFunction->returnType != vt_null)
// 	{
// 		(*m.tempNum)++;
// 	}

// 	// push arguments iff they exist
// 	if (m.ast->child->sibling != NULL)
// 	{
// 		struct LinearizationMetadata argumentMetadata = m;
// 		argumentMetadata.ast = m.ast->child->sibling;

// 		m.currentTACIndex = walkArgumentPushes(argumentMetadata, calledFunction);
// 	}

// 	struct TACLine *calltac = newTACLine(m.currentTACIndex++, tt_call, m.ast);
// 	calltac->operands[0].name.str = operand0;

// 	// always set the return permutation to temp
// 	// null vs non-null type will be the handler for whether the return value exists
// 	calltac->operands[0].permutation = vp_temp;

// 	// no type check because it contains the name of the function itself

// 	calltac->operands[1].name.str = m.ast->child->value;

// 	if (calledFunction->returnType != vt_null)
// 	{
// 		calltac->operands[0].type = calledFunction->returnType;
// 	}
// 	// TODO: set variabletype based on function return type, error if void function

// 	// struct FunctionEntry *calledFunction = symbolTableLookup_fun(table, functionName);
// 	// calltac->operands[1].type = calledFunction->returnType;

// 	BasicBlock_append(m.currentBlock, calltac);
// 	return m.currentTACIndex;
// }

// // linearize any subexpression which results in the use of a temporary variable
// int linearizeSubExpression(struct LinearizationMetadata m,
// 						   struct TACLine *parentExpression,
// 						   int operandIndex,
// 						   char forceConstantToRegister)
// {
// 	parentExpression->operands[operandIndex].name.str = TempList_Get(temps, *m.tempNum);
// 	parentExpression->operands[operandIndex].permutation = vp_temp;

// 	switch (m.ast->type)
// 	{

// 	case t_identifier:
// 	{
// 		struct VariableEntry *theVariable = Scope_lookupVar(m.scope, m.ast);
// 		parentExpression->operands[operandIndex].name.str = theVariable->name;
// 		// if the variable is just a regular variable, use its indirection level
// 		if (theVariable->localPointerTo == NULL)
// 		{
// 			parentExpression->operands[operandIndex].indirectionLevel = theVariable->indirectionLevel;
// 		}
// 		// but if it's a local pointer to something it will have indirection level of 1
// 		else
// 		{
// 			parentExpression->operands[operandIndex].indirectionLevel = 1;
// 		}
// 		parentExpression->operands[operandIndex].permutation = vp_standard;
// 		parentExpression->operands[operandIndex].type = theVariable->type;
// 	}
// 	break;

// 	case t_constant:
// 	case t_char_literal:
// 	{
// 		// we need an operand for the literal one way or another
// 		struct TACOperand literalOperand;
// 		literalOperand.indirectionLevel = 0;
// 		literalOperand.permutation = vp_literal;

// 		if (m.ast->type == t_constant)
// 		{
// 			literalOperand.name.str = m.ast->value;
// 			literalOperand.type = selectVariableTypeForLiteral(m.ast->value);
// 		}
// 		else
// 		{
// 			// memory leak, need to include dictionary in linearizationMetadata to keep proper track of these values
// 			char literalAsNumber[8];
// 			sprintf(literalAsNumber, "%d", m.ast->value[0]);
// 			literalOperand.name.str = Dictionary_LookupOrInsert(m.dict, literalAsNumber);
// 			literalOperand.type = vt_uint8;
// 		}

// 		// if this subexpressin requires a register, we will need an extra instruction to load the literal to a register
// 		if (forceConstantToRegister)
// 		{
// 			struct TACLine *literalLoadLine = newTACLine(m.currentTACIndex++, tt_assign, m.ast);
// 			literalLoadLine->operands[1] = literalOperand;

// 			// construct an operand for the temp this literal will occupy
// 			struct TACOperand tempOperand;
// 			tempOperand.name.str = TempList_Get(temps, *m.tempNum);
// 			(*m.tempNum)++;
// 			tempOperand.indirectionLevel = 0;
// 			tempOperand.permutation = vp_temp;
// 			tempOperand.type = literalOperand.type;

// 			// assign to and read from the temp
// 			literalLoadLine->operands[0] = tempOperand;
// 			parentExpression->operands[operandIndex] = tempOperand;

// 			BasicBlock_append(m.currentBlock, literalLoadLine);
// 		}
// 		else
// 		{
// 			parentExpression->operands[operandIndex] = literalOperand;
// 		}
// 	}
// 	break;

// 	case t_lParen:
// 	{
// 		struct LinearizationMetadata callMetadata = m;
// 		callMetadata.ast = m.ast;

// 		m.currentTACIndex = walkFunctionCall(callMetadata);
// 		struct TACLine *recursiveFunctionCall = m.currentBlock->TACList->tail->data;

// 		// TODO: check return type (or at least that function returns something)

// 		// using a returned value will always be a temp
// 		parentExpression->operands[operandIndex].permutation = vp_temp;
// 		parentExpression->operands[operandIndex].type = recursiveFunctionCall->operands[0].type;
// 		parentExpression->operands[operandIndex].indirectionLevel = recursiveFunctionCall->operands[1].indirectionLevel;
// 	}
// 	break;

// 	case t_plus:
// 	case t_minus:
// 	case t_lThan:
// 	// case t_bin_lThanE:
// 	case t_gThan:
// 		// case t_bin_gThanE:
// 		// case t_bin_equals:
// 		// case t_bin_notEquals:
// 		{
// 			struct LinearizationMetadata expressionMetadata = m;
// 			expressionMetadata.ast = m.ast;

// 			m.currentTACIndex = walkExpression(expressionMetadata);
// 			struct TACLine *recursiveExpression = m.currentBlock->TACList->tail->data;

// 			parentExpression->operands[operandIndex].type = recursiveExpression->operands[0].type;
// 			parentExpression->operands[operandIndex].indirectionLevel = recursiveExpression->operands[0].indirectionLevel;
// 		}
// 		break;

// 	case t_lBracket: // array reference
// 	{
// 		struct LinearizationMetadata arrayReferenceMetadata = m;
// 		arrayReferenceMetadata.ast = m.ast;

// 		m.currentTACIndex = walkArrayRef(arrayReferenceMetadata);
// 		struct TACLine *recursiveArrayRef = m.currentBlock->TACList->tail->data;

// 		parentExpression->operands[operandIndex].type = recursiveArrayRef->operands[0].type;
// 		parentExpression->operands[operandIndex].indirectionLevel = recursiveArrayRef->operands[0].indirectionLevel;
// 	}
// 	break;

// 	case t_star:
// 	{
// 		struct LinearizationMetadata dereferenceMetadata = m;
// 		dereferenceMetadata.ast = m.ast->child;

// 		m.currentTACIndex = walkDereference(dereferenceMetadata);
// 		struct TACLine *recursiveDereference = m.currentBlock->TACList->tail->data;

// 		parentExpression->operands[operandIndex].type = recursiveDereference->operands[0].type;
// 		parentExpression->operands[operandIndex].indirectionLevel = recursiveDereference->operands[0].indirectionLevel;
// 	}
// 	break;

// 	case t_string_literal:
// 	{
// 		/*
// 		struct TACLine *loadStringLiteral = newTACLine(m.currentTACIndex++, tt_assign, m.ast);
// 		loadStringLiteral->operands[0].name.str = TempList_Get(temps, *m.tempNum++);
// 		loadStringLiteral->operands[0].type = vt_uint8;
// 		loadStringLiteral->operands[0].permutation = vp_temp;
// 		loadStringLiteral->operands[0].indirectionLevel = 1;

// 		loadStringLiteral->operands[1].type = vt_uint8;
// 		loadStringLiteral->operands[1].permutation = vp_objptr;
// 		loadStringLiteral->operands[1].indirectionLevel = 1;
// 		loadStringLiteral->operands[1].name.str = m.ast->value;

// 		BasicBlock_append(m.currentBlock, loadStringLiteral);
// 		printTACLine(loadStringLiteral);

// 		parentExpression->operands[operandIndex] = loadStringLiteral->operands[0];*/
// 		parentExpression->operands[operandIndex].type = vt_uint8;
// 		parentExpression->operands[operandIndex].permutation = vp_objptr;
// 		parentExpression->operands[operandIndex].indirectionLevel = 1;
// 		parentExpression->operands[operandIndex].name.str = m.ast->value;
// 		// linearizeAssignment(
// 		// assignment->operands[1].type = vt_uint8;
// 		// assignment->operands[1].permutation = vp_objptr;
// 		// assignment->operands[0].type = vt_uint8;
// 		// assignment->operands[1].indirectionLevel = 1;
// 	}
// 	break;

// 	default:
// 		ErrorWithAST(ERROR_INTERNAL, m.ast, "Malformed AST seen while linearizing subexpression!\n");
// 	}
// 	return m.currentTACIndex;
// }

// // given an AST node of an expression, figure out how to break it down into multiple lines of three address code
// int linearizeExpression(struct LinearizationMetadata m)
// {
// 	// set to tt_assign, reassign in switch body based on operator later
// 	// also set true TAC index at bottom of function (or return point), after child expression linearizations take place
// 	struct TACLine *thisExpression = newTACLine(m.currentTACIndex, tt_assign, m.ast);

// 	// since 'cmp' doesn't generate a result, it just sets flags, no need to consume a temp for operations that become cmp's
// 	switch (m.ast->type)
// 	{
// 	case t_plus:
// 	case t_minus:
// 	case t_star:
// 	case t_lBracket: // array reference
// 		thisExpression->operands[0].name.str = TempList_Get(temps, *m.tempNum);
// 		thisExpression->operands[0].permutation = vp_temp;
// 		// increment count of temp variables, the parse of this expression will be written to a temp
// 		(*m.tempNum)++;
// 		break;

// 	case t_lThan:
// 	case t_gThan:
// 	case t_lThanE:
// 	case t_gThanE:
// 	case t_equals:
// 	case t_nEquals:
// 		break;

// 	default:
// 		break;
// 	}

// 	// these cases are handled by their own functions and return directly from within the if/else statements
// 	if (m.ast->type == t_star)
// 	{
// 		thisExpression->operation = tt_memr_1;
// 		// if simply dereferencing a name
// 		if (m.ast->child->type == t_identifier)
// 		{
// 			thisExpression->operands[1].name.str = m.ast->child->value;
// 			thisExpression->operands[1].type = Scope_lookupVar(m.scope, m.ast->child)->type;
// 		}
// 		// otherwise there's pointer arithmetic involved
// 		else
// 		{
// 			thisExpression->operands[1].name.str = TempList_Get(temps, *m.tempNum);
// 			thisExpression->operands[1].permutation = vp_temp;

// 			struct LinearizationMetadata dereferenceMetadata = m;
// 			dereferenceMetadata.ast = m.ast->child;

// 			m.currentTACIndex = walkDereference(dereferenceMetadata);
// 			struct TACLine *recursiveDereference = m.currentBlock->TACList->tail->data;

// 			thisExpression->operands[1].indirectionLevel = recursiveDereference->operands[0].indirectionLevel;
// 			thisExpression->operands[1].type = recursiveDereference->operands[0].type;
// 		}

// 		thisExpression->index = m.currentTACIndex++;
// 		BasicBlock_append(m.currentBlock, thisExpression);
// 		return m.currentTACIndex;
// 	}
// 	else if (m.ast->type == t_lBracket)
// 	{
// 		thisExpression->operands[1].name.str = TempList_Get(temps, *m.tempNum);
// 		thisExpression->operands[1].permutation = vp_temp;

// 		// can pass the metadata straight through
// 		m.currentTACIndex = walkArrayRef(m);

// 		thisExpression->index = m.currentTACIndex++;
// 		BasicBlock_append(m.currentBlock, thisExpression);
// 		return m.currentTACIndex;
// 	}

// 	// if we fall through to here, the expression contains a subexpression

// 	// handle the LHS of the expression
// 	struct LinearizationMetadata LHSMetadata = m;
// 	LHSMetadata.ast = m.ast->child;
// 	m.currentTACIndex = walkSubExpression(LHSMetadata, thisExpression, 1, 0);

// 	// assign the TAC operation based on the operator at hand
// 	switch (m.ast->type)
// 	{
// 	case t_plus:
// 	{
// 		thisExpression->reorderable = 1;
// 		thisExpression->operation = tt_add;
// 	}
// 	break;

// 	case t_minus:
// 	{
// 		thisExpression->operation = tt_subtract;
// 	}
// 	break;

// 	case t_lThan:
// 	case t_gThan:
// 	case t_lThanE:
// 	case t_gThanE:
// 	case t_equals:
// 	case t_nEquals:
// 	{
// 		thisExpression->operation = tt_cmp;
// 	}
// 	break;

// 	default:
// 	{
// 		ErrorWithAST(ERROR_INTERNAL, m.ast, "Malformed AST found for operator in linearizeExpression!\n");
// 		break;
// 	}
// 	}

// 	// handle the RHS of the expression
// 	struct LinearizationMetadata RHSMetadata = m;
// 	RHSMetadata.ast = m.ast->child->sibling;
// 	m.currentTACIndex = walkSubExpression(RHSMetadata, thisExpression, 2, 0);

// 	if (thisExpression->operation != tt_cmp)
// 	{
// 		// TODO: with signed types, error on arithmetic between different signs

// 		// if either operand is a literal, evaluate assigned size to the non-literal operand
// 		if (thisExpression->operands[1].permutation == vp_literal)
// 		{
// 			thisExpression->operands[0].type = thisExpression->operands[2].type;
// 		}
// 		else if (thisExpression->operands[2].permutation == vp_literal)
// 		{
// 			thisExpression->operands[0].type = thisExpression->operands[1].type;
// 		}
// 		else
// 		// otherwise, an expression will take on the size of the largest of its two operands
// 		{
// 			if (thisExpression->operands[1].type > thisExpression->operands[2].type)
// 			{
// 				thisExpression->operands[0].type = thisExpression->operands[1].type;
// 			}
// 			else
// 			{
// 				thisExpression->operands[0].type = thisExpression->operands[2].type;
// 			}
// 		}
// 	}

// 	// automatically scale pointer arithmetic
// 	// but only for relevant operations
// 	switch (thisExpression->operation)
// 	{
// 	case tt_add:
// 	case tt_subtract:
// 	{
// 		// op1 is a pointer, op2 isn't
// 		if (thisExpression->operands[1].indirectionLevel > 0 && thisExpression->operands[2].indirectionLevel == 0)
// 		{
// 			switch (thisExpression->operands[2].permutation)
// 			{
// 			case vp_literal:
// 			{
// 				char scaledLiteral[16];
// 				// TODO: this will cause issues when implementing objects as we only scale pointer arithmetic based on primitive sizes
// 				sprintf(scaledLiteral, "%d", atoi(thisExpression->operands[2].name.str) * GetSizeOfPrimitive(thisExpression->operands[1].type));
// 				thisExpression->operands[2].name.str = Dictionary_LookupOrInsert(m.dict, scaledLiteral);
// 				thisExpression->operands[2].indirectionLevel = thisExpression->operands[1].indirectionLevel;
// 			}
// 			break;

// 			case vp_standard:
// 			case vp_temp:
// 			{
// 				struct TACLine *scaleMultiply = newTACLine(m.currentTACIndex++, tt_mul, m.ast);
// 				scaleMultiply->operands[0].name.str = TempList_Get(temps, *m.tempNum);
// 				(*m.tempNum)++;
// 				scaleMultiply->operands[0].permutation = vp_temp;
// 				scaleMultiply->operands[0].type = thisExpression->operands[2].type;

// 				// scale multiplication result has same indirection level as the operand being scaled to
// 				scaleMultiply->operands[0].indirectionLevel = thisExpression->operands[1].indirectionLevel;

// 				// transfer the scaled operand out of the main expression
// 				scaleMultiply->operands[1] = thisExpression->operands[0];

// 				// transfer the temp into the main expression
// 				thisExpression->operands[2] = scaleMultiply->operands[0];

// 				char scalingLiteral[16];
// 				sprintf(scalingLiteral, "%d", 4);
// 				scaleMultiply->operands[2].name.str = Dictionary_LookupOrInsert(m.dict, scalingLiteral);
// 				scaleMultiply->operands[2].permutation = vp_literal;
// 				scaleMultiply->operands[2].type = selectVariableTypeForLiteral(scaleMultiply->operands[2].name.str);
// 				;
// 				BasicBlock_append(m.currentBlock, scaleMultiply);
// 			}
// 			break;

// 			case vp_objptr:
// 			{
// 				ErrorWithAST(ERROR_CODE, thisExpression->correspondingTree, "Arithmetic with object pointers not supported!\n");
// 			}
// 			break;
// 			}
// 		}
// 		else
// 		{
// 			if (thisExpression->operands[2].indirectionLevel > 0 && thisExpression->operands[1].indirectionLevel == 0)
// 			{
// 				switch (thisExpression->operands[1].permutation)
// 				{
// 				case vp_literal:
// 				{
// 					char scaledLiteral[16];
// 					// TODO: this will cause issues when implementing objects as we only scale pointer arithmetic based on primitive sizes
// 					sprintf(scaledLiteral, "%d", atoi(thisExpression->operands[1].name.str) * GetSizeOfPrimitive(thisExpression->operands[2].type));
// 					thisExpression->operands[1].name.str = Dictionary_LookupOrInsert(m.dict, scaledLiteral);
// 					thisExpression->operands[1].indirectionLevel = thisExpression->operands[2].indirectionLevel;
// 				}
// 				break;

// 				case vp_standard:
// 				case vp_temp:
// 				{
// 					struct TACLine *scaleMultiply;
// 					scaleMultiply = newTACLine(m.currentTACIndex++, tt_mul, m.ast);
// 					scaleMultiply->operands[0].name.str = TempList_Get(temps, *m.tempNum);
// 					(*m.tempNum)++;
// 					scaleMultiply->operands[0].permutation = vp_temp;
// 					scaleMultiply->operands[0].type = thisExpression->operands[2].type;

// 					// scale multiplication result has same indirection level as the operand being scaled to
// 					scaleMultiply->operands[0].indirectionLevel = thisExpression->operands[2].indirectionLevel;

// 					// transfer the scaled operand out of the main expression
// 					scaleMultiply->operands[1] = thisExpression->operands[1];

// 					// transfer the temp into the main expression
// 					thisExpression->operands[1] = scaleMultiply->operands[0];

// 					char scalingLiteral[16];
// 					sprintf(scalingLiteral, "%d", 4);
// 					scaleMultiply->operands[2].name.str = Dictionary_LookupOrInsert(m.dict, scalingLiteral);
// 					scaleMultiply->operands[2].name.str = scalingLiteral;
// 					scaleMultiply->operands[2].permutation = vp_literal;
// 					scaleMultiply->operands[2].type = selectVariableTypeForLiteral(scaleMultiply->operands[2].name.str);
// 					BasicBlock_append(m.currentBlock, scaleMultiply);
// 				}

// 				case vp_objptr:
// 				{
// 					ErrorWithAST(ERROR_CODE, thisExpression->correspondingTree, "Arithmetic with object pointers not supported!\n");
// 				}
// 				break;
// 				}
// 			}
// 		}
// 	}
// 	break;

// 	default:
// 		break;
// 	}

// 	thisExpression->index = m.currentTACIndex++;
// 	BasicBlock_append(m.currentBlock, thisExpression);
// 	return m.currentTACIndex;
// }

// // given an AST node of an array reference, generate TAC for it
// int linearizeArrayRef(struct LinearizationMetadata m)
// {
// 	struct AST *arrayBaseTree = m.ast->child;
// 	if (arrayBaseTree->type != t_identifier)
// 	{
// 		ErrorWithAST(ERROR_INTERNAL, arrayBaseTree, "Malformed AST for array reference!\n");
// 	}

// 	struct TACLine *arrayRefTAC = newTACLine(m.currentTACIndex, tt_memr_3, m.ast);

// 	// set up the base address value for the memory read TAC operation
// 	struct VariableEntry *arrayBaseEntry = Scope_lookupVar(m.scope, arrayBaseTree);
// 	arrayRefTAC->operands[1].name.str = arrayBaseEntry->name;
// 	arrayRefTAC->operands[1].indirectionLevel = arrayBaseEntry->indirectionLevel;
// 	arrayRefTAC->operands[1].permutation = vp_standard;
// 	arrayRefTAC->operands[1].type = arrayBaseEntry->type;

// 	arrayRefTAC->operands[0].name.str = TempList_Get(temps, *m.tempNum);
// 	arrayRefTAC->operands[0].permutation = vp_temp;
// 	(*m.tempNum)++;
// 	if (arrayBaseEntry->indirectionLevel > 0)
// 	{
// 		arrayRefTAC->operands[0].indirectionLevel = arrayBaseEntry->indirectionLevel - 1;
// 	}
// 	else
// 	{
// 		arrayRefTAC->operands[0].indirectionLevel = 0;
// 	}
// 	arrayRefTAC->operands[0].type = arrayRefTAC->operands[1].type;

// 	struct AST *arrayIndexTree = m.ast->child->sibling;
// 	// if the index is a constant, use addressing mode 2 and manually do the scaling at compile time
// 	if (arrayIndexTree->type == t_constant)
// 	{
// 		arrayRefTAC->operation = tt_memr_2;

// 		int indexSize = atoi(arrayIndexTree->value);
// 		struct VariableEntry *arrayVariable = Scope_lookupVar(m.scope, arrayBaseTree);
// 		if (arrayVariable->indirectionLevel == 1)
// 		{
// 			// TODO: this will cause problems with complex types
// 			indexSize *= GetSizeOfPrimitive(arrayVariable->type);
// 		}
// 		else
// 		{
// 			indexSize *= Scope_getSizeOfVariable(m.scope, arrayBaseTree);
// 		}

// 		arrayRefTAC->operands[2].name.val = indexSize;
// 		arrayRefTAC->operands[2].indirectionLevel = 0;
// 		arrayRefTAC->operands[2].permutation = vp_literal;
// 		arrayRefTAC->operands[2].type = selectVariableTypeForNumber(arrayRefTAC->operands[2].name.val);

// 		printTACLine(arrayRefTAC);
// 	}
// 	// otherwise, the index is either a variable or subexpression
// 	else
// 	{
// 		// set the scale for the array access
// 		// set scale

// 		arrayRefTAC->operands[3].name.val = alignSize(Scope_getSizeOfVariableByString(m.scope, arrayRefTAC->operands[1].name.str, 1));
// 		arrayRefTAC->operands[3].indirectionLevel = 0;
// 		arrayRefTAC->operands[3].permutation = vp_literal;
// 		arrayRefTAC->operands[3].type = selectVariableTypeForNumber(arrayRefTAC->operands[3].name.val);

// 		struct LinearizationMetadata indexExpressionMetadata = m;
// 		indexExpressionMetadata.ast = arrayIndexTree;

// 		m.currentTACIndex = walkSubExpression(indexExpressionMetadata, arrayRefTAC, 2, 0);
// 	}

// 	arrayRefTAC->index = m.currentTACIndex++;
// 	BasicBlock_append(m.currentBlock, arrayRefTAC);
// 	return m.currentTACIndex;
// }

// // given an AST node of an assignment, return the TAC block necessary to generate the correct value
// int linearizeAssignment(struct LinearizationMetadata m)
// {
// 	struct AST *LHSTree = m.ast->child;
// 	struct AST *RHSTree = LHSTree->sibling;

// 	struct TACLine *assignment = newTACLine(m.currentTACIndex, tt_assign, m.ast);

// 	// if this assignment is simply setting one thing to another
// 	if (RHSTree->child == NULL)
// 	{
// 		// pull out the relevant values and generate a single line of TAC to return
// 		assignment->operands[1].name.str = RHSTree->value;

// 		switch (RHSTree->type)
// 		{
// 		case t_constant:
// 		{
// 			assignment->operands[1].type = selectVariableTypeForLiteral(RHSTree->value);
// 			assignment->operands[0].type = assignment->operands[1].type;
// 			assignment->operands[1].permutation = vp_literal;
// 		}
// 		break;

// 		case t_char_literal:
// 		{
// 			char literalAsNumber[8];
// 			sprintf(literalAsNumber, "%d", RHSTree->value[0]);
// 			assignment->operands[1].name.str = Dictionary_LookupOrInsert(m.dict, literalAsNumber);
// 			assignment->operands[1].type = vt_uint8;
// 			assignment->operands[0].type = vt_uint8;
// 			assignment->operands[1].permutation = vp_literal;
// 		}
// 		break;

// 		// OK to hardcode these because we always know what a string literal will be
// 		case t_string_literal:
// 		{
// 			assignment->operands[1].type = vt_uint8;
// 			assignment->operands[1].permutation = vp_objptr;
// 			assignment->operands[0].type = vt_uint8;
// 			assignment->operands[1].indirectionLevel = 1;
// 		}
// 		break;

// 		// identifier with no child - identifier alone
// 		case t_identifier:
// 		{
// 			struct VariableEntry *theVariable = Scope_lookupVar(m.scope, RHSTree);
// 			assignment->operands[1].type = theVariable->type;
// 			assignment->operands[0].type = theVariable->type;
// 			assignment->operands[1].indirectionLevel = theVariable->indirectionLevel;
// 		}
// 		break;

// 		default:
// 			ErrorWithAST(ERROR_INTERNAL, RHSTree, "Malformed AST seen on right side of simple assignment\n");
// 		}
// 	}
// 	else
// 	// otherwise there is some sort of expression, which will need to be broken down into multiple lines of TAC
// 	{
// 		struct LinearizationMetadata subexpressionMetadata = m;
// 		subexpressionMetadata.ast = RHSTree;
// 		m.currentTACIndex = walkSubExpression(subexpressionMetadata, assignment, 1, 0);
// 	}
// 	assignment->index = m.currentTACIndex++;
// 	BasicBlock_append(m.currentBlock, assignment);

// 	// grab the TAC we just generated for the RHS of the expression
// 	struct TACLine *RHSTac = m.currentBlock->TACList->tail->data;

// 	struct AST *LHS = m.ast->child;

// 	// if this is a declare-and-assign, skip the type
// 	switch (LHS->type)
// 	{
// 	case t_uint8:
// 	case t_uint16:
// 	case t_uint32:
// 	{
// 		// use scrapePointers to go over to whatever is actually being declared so we can assign to it
// 		scrapePointers(LHS->child, &LHS);
// 	}
// 	break;

// 	default:
// 		break;
// 	}

// 	switch (LHS->type)
// 	{
// 	case t_identifier:
// 	{
// 		struct VariableEntry *assignedVariable = Scope_lookupVar(m.scope, LHS);
// 		RHSTac->operands[0].name.str = LHS->value;
// 		RHSTac->operands[0].type = assignedVariable->type;
// 		RHSTac->operands[0].indirectionLevel = assignedVariable->indirectionLevel;
// 		RHSTac->operands[0].permutation = vp_standard;
// 	}
// 	break;

// 	// pointer dereference - storing to
// 	case t_star:
// 	{
// 		struct TACLine *finalAssignment = newTACLine(m.currentTACIndex, tt_memw_3, LHSTree);

// 		// set RHS TAC to assign to a temp
// 		RHSTac->operands[0].name.str = TempList_Get(temps, *m.tempNum);
// 		RHSTac->operands[0].permutation = vp_temp;
// 		(*m.tempNum)++;
// 		RHSTac->operands[0].type = RHSTac->operands[1].type;
// 		RHSTac->operands[0].indirectionLevel = RHSTac->operands[1].indirectionLevel;

// 		struct TACOperand operandToWrite = RHSTac->operands[0];

// 		struct AST *assignedDereference = LHS->child;

// 		if (assignedDereference->type == t_plus || assignedDereference->type == t_minus)
// 		{
// 			if (assignedDereference->type == t_plus)
// 			{
// 				finalAssignment->operation = tt_memw_3;
// 			}
// 			else if (assignedDereference->type == t_minus)
// 			{
// 				finalAssignment->operation = tt_memw_3_n;
// 			}
// 			else
// 			{
// 				ErrorWithAST(ERROR_CODE, assignedDereference, "Illegal expression type (%s) being dereferenced for assignment to\n", getTokenName(assignedDereference->type));
// 			}

// 			// set base
// 			struct LinearizationMetadata pointerArithLHS = m;
// 			pointerArithLHS.ast = assignedDereference->child;
// 			m.currentTACIndex = walkSubExpression(pointerArithLHS, finalAssignment, 0, 1);

// 			// set offset
// 			struct LinearizationMetadata pointerArithRHS = m;
// 			pointerArithRHS.ast = assignedDereference->child->sibling;
// 			m.currentTACIndex = walkSubExpression(pointerArithRHS, finalAssignment, 1, 1);

// 			// set scale
// 			finalAssignment->operands[2].name.val = alignSize(Scope_getSizeOfVariableByString(m.scope, finalAssignment->operands[0].name.str, 1));
// 			finalAssignment->operands[2].indirectionLevel = 0;
// 			finalAssignment->operands[2].permutation = vp_literal;
// 			finalAssignment->operands[2].type = selectVariableTypeForNumber(finalAssignment->operands[2].name.val);

// 			// set source
// 			finalAssignment->operands[3] = operandToWrite;
// 		}
// 		// not plus or minus
// 		else
// 		{
// 			finalAssignment->operation = tt_memw_1;
// 			struct LinearizationMetadata LHSIdentifierMetadata = m;
// 			LHSIdentifierMetadata.ast = assignedDereference;
// 			m.currentTACIndex = walkSubExpression(LHSIdentifierMetadata, finalAssignment, 0, 0);

// 			finalAssignment->operands[1] = operandToWrite;
// 		}

// 		finalAssignment->index = m.currentTACIndex++;
// 		BasicBlock_append(m.currentBlock, finalAssignment);
// 	}
// 	break;

// 	// array element reference
// 	case t_lBracket:
// 	{
// 		struct TACLine *finalAssignment = newTACLine(m.currentTACIndex, tt_memw_3, LHS);

// 		struct VariableEntry *assignedArray = Scope_lookupVar(m.scope, LHS->child);

// 		finalAssignment->operands[0].name.str = assignedArray->name;
// 		finalAssignment->operands[0].type = assignedArray->type;
// 		finalAssignment->operands[0].indirectionLevel = assignedArray->indirectionLevel;
// 		finalAssignment->operands[0].permutation = vp_standard;

// 		RHSTac->operands[0].name.str = TempList_Get(temps, *m.tempNum);
// 		RHSTac->operands[0].permutation = vp_temp;
// 		(*m.tempNum)++;
// 		RHSTac->operands[0].type = RHSTac->operands[1].type;
// 		RHSTac->operands[0].indirectionLevel = RHSTac->operands[1].indirectionLevel;

// 		// copy the temp from the RHS to the source of the memory write operation
// 		finalAssignment->operands[3] = RHSTac->operands[0];

// 		// handle the scaling value (scale by size of array element)
// 		finalAssignment->operands[2].indirectionLevel = 0;
// 		finalAssignment->operands[2].permutation = vp_literal;
// 		finalAssignment->operands[2].name.val = alignSize(Scope_getSizeOfVariable(m.scope, LHS->child));
// 		finalAssignment->operands[2].type = selectVariableTypeForNumber(finalAssignment->operands[2].name.val);

// 		// handle the base (array start)

// 		struct AST *arrayIndex = LHS->child->sibling;
// 		switch (arrayIndex->type)
// 		{
// 		case t_identifier:
// 		{
// 			struct VariableEntry *indexVariable = Scope_lookupVar(m.scope, arrayIndex);

// 			finalAssignment->operands[1].indirectionLevel = 0;
// 			finalAssignment->operands[1].name.str = arrayIndex->value;
// 			finalAssignment->operands[1].permutation = vp_standard;
// 			finalAssignment->operands[1].type = indexVariable->type;
// 		}
// 		break;

// 		// switch to addressing mode 2 if we have a constant
// 		case t_constant:
// 		{
// 			finalAssignment->operation = tt_memw_2;
// 			// finalAssignment->operands[0].indirectionLevel--;
// 			finalAssignment->operands[1].indirectionLevel = 0;

// 			int indexSize = atoi(arrayIndex->value);
// 			// indexSize *= Scope_getSizeOfVariable(m.scope, LHS->child);

// 			if (assignedArray->indirectionLevel == 1)
// 			{
// 				// TODO: this will cause problems with complex types
// 				indexSize *= GetSizeOfPrimitive(assignedArray->type);
// 			}
// 			else
// 			{
// 				indexSize *= Scope_getSizeOfVariable(m.scope, LHS->child);
// 			}

// 			finalAssignment->operands[1].name.val = indexSize;
// 			finalAssignment->operands[1].permutation = vp_literal;
// 			finalAssignment->operands[1].type = selectVariableTypeForNumber(finalAssignment->operands[1].name.val);

// 			finalAssignment->operands[2] = finalAssignment->operands[3];

// 			memset(&finalAssignment->operands[3], 0, sizeof(struct TACOperand));
// 		}
// 		break;

// 		// if not identifier or constant, assume it must be some sort of expression inside the brackets
// 		default:
// 		{
// 			struct LinearizationMetadata expressionMetadata = m;
// 			expressionMetadata.ast = arrayIndex;

// 			m.currentTACIndex = walkExpression(expressionMetadata);

// 			struct TACLine *finalArithmeticLine = m.currentBlock->TACList->tail->data;

// 			finalAssignment->operands[1] = finalArithmeticLine->operands[0];
// 		}
// 		break;
// 		}
// 		finalAssignment->index = m.currentTACIndex++;
// 		BasicBlock_append(m.currentBlock, finalAssignment);
// 	}
// 	break;

// 	default:
// 		ErrorWithAST(ERROR_INTERNAL, LHS, "Malformed AST on LHS of assignment");
// 	}

// 	return m.currentTACIndex;
// }

// struct TACLine *linearizeConditionalJump(int currentTACIndex,
// 										 struct AST *cmpOp,
// 										 char whichCondition)
// {
// 	enum TACType jumpCondition;
// 	switch (cmpOp->type)
// 	{
// 	case t_lThanE:
// 		jumpCondition = (whichCondition ? tt_jle : tt_jg);
// 		break;

// 	case t_lThan:
// 		jumpCondition = (whichCondition ? tt_jl : tt_jge);
// 		break;

// 	case t_gThanE:
// 		jumpCondition = (whichCondition ? tt_jge : tt_jl);
// 		break;

// 	case t_gThan:
// 		jumpCondition = (whichCondition ? tt_jg : tt_jle);
// 		break;

// 	case t_nEquals:
// 		jumpCondition = (whichCondition ? tt_jne : tt_je);
// 		break;

// 	case t_equals:
// 		jumpCondition = (whichCondition ? tt_je : tt_jne);
// 		break;

// 	default:
// 		ErrorWithAST(ERROR_INTERNAL, cmpOp, "Malformed AST seen in conditional jump\n");
// 	}
// 	return newTACLine(currentTACIndex, jumpCondition, cmpOp);
// }

// int linearizeConditionCheck(struct LinearizationMetadata m,
// 							char whichCondition,
// 							int targetLabel,
// 							int *labelCount,
// 							int depth)
// {
// 	switch (m.ast->type)
// 	{
// 	case t_and:
// 	{
// 		ErrorAndExit(ERROR_INTERNAL, "Logical And of expressions in condition checks not supported yet!\n");

// 		struct LinearizationMetadata LHS = m;
// 		LHS.ast = m.ast->child;

// 		m.currentTACIndex = walkConditionCheck(LHS, 0, targetLabel, labelCount, depth + 1);

// 		struct LinearizationMetadata RHS = m;
// 		RHS.ast = m.ast->child->sibling;

// 		m.currentTACIndex = walkConditionCheck(RHS, 0, targetLabel, labelCount, depth + 1);

// 		// no need for extra logic - if either condition is false the whole condition is false
// 	}
// 	break;

// 	case t_or:
// 	{
// 		ErrorAndExit(ERROR_INTERNAL, "Logical Or of expressions in condition checks not supported yet!\n");

// 		// if either condition is true, jump to the true label, if fall through both conditions, jump to false label
// 		if (!whichCondition)
// 		{
// 			struct TACLine *condTrueLabel = NULL;
// 			condTrueLabel = newTACLine(m.currentTACIndex, tt_label, m.ast);
// 			condTrueLabel->operands[0].name.val = *labelCount;

// 			struct LinearizationMetadata LHS = m;
// 			LHS.ast = m.ast->child;

// 			m.currentTACIndex = walkConditionCheck(LHS, 1, targetLabel, labelCount, depth + 1);
// 			struct TACLine *conditionJump = m.currentBlock->TACList->tail->data;
// 			conditionJump->operands[0].name.val = *labelCount;

// 			struct LinearizationMetadata RHS = m;
// 			RHS.ast = m.ast->child->sibling;
// 			m.currentTACIndex = walkConditionCheck(RHS, 1, targetLabel, labelCount, depth + 1);
// 			conditionJump = m.currentBlock->TACList->tail->data;
// 			conditionJump->operands[0].name.val = *labelCount;

// 			struct TACLine *condFalseJump = newTACLine(m.currentTACIndex++, tt_jmp, m.ast);
// 			condFalseJump->operands[0].name.val = targetLabel;
// 			condTrueLabel->index = m.currentTACIndex++;

// 			BasicBlock_append(m.currentBlock, condFalseJump);
// 			BasicBlock_append(m.currentBlock, condTrueLabel);
// 			(*labelCount)++;
// 		}
// 		else
// 		{
// 		}
// 	}
// 	break;

// 	case t_lThan:
// 	case t_gThan:
// 	case t_lThanE:
// 	case t_gThanE:
// 	case t_equals:
// 	case t_nEquals:
// 	{
// 		m.currentTACIndex = walkExpression(m);

// 		// generate a label and figure out condition to jump when the if statement isn't executed
// 		struct TACLine *condFalseJump = walkConditionalJump(m.currentTACIndex++, m.ast, whichCondition);
// 		condFalseJump->operands[0].name.val = targetLabel;
// 		BasicBlock_append(m.currentBlock, condFalseJump);
// 	}
// 	break;

// 	default:
// 		ErrorWithAST(ERROR_INTERNAL, m.ast, "Malformed AST in condition check\n");
// 	}
// 	return m.currentTACIndex;
// }

// struct Stack *linearizeIfStatement(struct LinearizationMetadata m,
// 								   struct BasicBlock *afterIfBlock,
// 								   int *labelCount,
// 								   struct Stack *scopenesting)
// {
// 	// a stack is overkill but allows to store either 1 or 2 resulting blocks depending on if there is or isn't an else block
// 	struct Stack *results = Stack_New();

// 	// linearize the expression we will test
// 	struct LinearizationMetadata conditionCheckMetadata = m;
// 	conditionCheckMetadata.ast = m.ast->child;

// 	// if we need to generate an else statement, jump there on false
// 	// otherwise jump to afterIfBlock on false
// 	struct BasicBlock *elseBlock = NULL;
// 	if (m.ast->child->sibling->sibling != NULL)
// 	{
// 		elseBlock = BasicBlock_new((*labelCount)++);
// 		m.currentTACIndex = walkConditionCheck(conditionCheckMetadata, 0, elseBlock->labelNum, labelCount, 0);
// 	}
// 	else
// 	{
// 		m.currentTACIndex = walkConditionCheck(conditionCheckMetadata, 0, afterIfBlock->labelNum, labelCount, 0);
// 	}

// 	struct LinearizationMetadata ifMetadata = m;
// 	ifMetadata.ast = m.ast->child->sibling;

// 	struct LinearizationResult *r = walkScope(ifMetadata, afterIfBlock, labelCount, scopenesting);
// 	Stack_Push(results, r);

// 	// we need to generate an else statement, do so now
// 	if (elseBlock != NULL)
// 	{
// 		// add the else block to the scope and function table
// 		Scope_addBasicBlock(m.scope, elseBlock);

// 		// linearize the else block, it returns to the same afterIfBlock as the ifBlock does
// 		struct LinearizationMetadata elseMetadata = m;
// 		elseMetadata.ast = m.ast->child->sibling->sibling;
// 		elseMetadata.currentBlock = elseBlock;

// 		r = walkScope(elseMetadata, afterIfBlock, labelCount, scopenesting);
// 		Stack_Push(results, r);
// 	}

// 	return results;
// }

// struct LinearizationResult *linearizeWhileLoop(struct LinearizationMetadata m,
// 											   struct BasicBlock *afterWhileBlock,
// 											   int *labelCount,
// 											   struct Stack *scopenesting)
// {
// 	struct BasicBlock *beforeWhileBlock = m.currentBlock;

// 	m.currentBlock = BasicBlock_new((*labelCount)++);

// 	Scope_addBasicBlock(m.scope, m.currentBlock);

// 	struct TACLine *enterWhileJump = newTACLine(m.currentTACIndex++, tt_jmp, m.ast);
// 	enterWhileJump->operands[0].name.val = m.currentBlock->labelNum;
// 	BasicBlock_append(beforeWhileBlock, enterWhileJump);

// 	struct TACLine *whileDo = newTACLine(m.currentTACIndex, tt_do, m.ast);
// 	BasicBlock_append(m.currentBlock, whileDo);

// 	// linearize the expression we will test
// 	struct LinearizationMetadata conditionCheckMetadata = m;
// 	conditionCheckMetadata.ast = m.ast->child;

// 	m.currentTACIndex = walkConditionCheck(conditionCheckMetadata, 0, afterWhileBlock->labelNum, labelCount, 0);

// 	// create the scope for the while loop
// 	struct LinearizationMetadata whileBodyScopeMetadata = m;
// 	whileBodyScopeMetadata.ast = m.ast->child->sibling;

// 	struct LinearizationResult *r = walkScope(whileBodyScopeMetadata, m.currentBlock, labelCount, scopenesting);
// 	struct TACLine *whileLoopJump = newTACLine(r->endingTACIndex++, tt_jmp, m.ast->child);
// 	whileLoopJump->operands[0].name.val = m.currentBlock->labelNum;

// 	struct TACLine *whileEndDo = newTACLine(r->endingTACIndex, tt_enddo, m.ast);
// 	BasicBlock_append(r->block, whileLoopJump);
// 	BasicBlock_append(r->block, whileEndDo);

// 	return r;
// }

// // given the AST for a function, generate TAC and return a pointer to the head of the generated block
// struct LinearizationResult *linearizeScope(struct LinearizationMetadata m,
// 										   struct BasicBlock *controlConvergesTo,
// 										   int *labelCount,
// 										   struct Stack *scopeNesting)
// {
// 	// if we are descending into a nested scope, look up the correct scope and use it, starting a new basic block
// 	// the subscope will be used in this call and any calls generated from this one, allowing the scopes to recursively nest properly
// 	int newSubscopeIndex = 0;
// 	if (scopeNesting->size > 0)
// 	{
// 		m.scope = Scope_lookupSubScopeByNumber(m.scope, *((int *)Stack_Peek(scopeNesting)));
// 		m.currentBlock = BasicBlock_new((*labelCount)++);
// 		Scope_addBasicBlock(m.scope, m.currentBlock);
// 	}
// 	// otherwise the stack is empty so we should set it up to start at index 0
// 	else
// 	{
// 		Stack_Push(scopeNesting, &newSubscopeIndex);
// 	}

// 	// locally scope a variable for scope count at this depth, push it to the stack
// 	int depthThisScope = 0;
// 	Stack_Push(scopeNesting, &depthThisScope);

// 	// scrape along the scope
// 	struct AST *runner = m.ast->child;
// 	unsigned char currentSubScopeIndex = 0;
// 	while (runner->type != t_rCurly)
// 	{
// 		switch (runner->type)
// 		{
// 		case t_lCurly:
// 		{
// 			struct LinearizationMetadata scopeMetadata = m;
// 			scopeMetadata.ast = runner;

// 			struct BasicBlock *afterScopeBlock = BasicBlock_new((*labelCount)++);
// 			// TODO: Is throwing away the return value safe? Test with multiple arbitrary scopes in series
// 			struct LinearizationResult *scopeResult = walkScope(scopeMetadata, afterScopeBlock, labelCount, scopeNesting);
// 			m.currentTACIndex = scopeResult->endingTACIndex;
// 			free(scopeResult);
// 			currentSubScopeIndex++;

// 			m.currentBlock = afterScopeBlock;
// 			Scope_addBasicBlock(m.scope, m.currentBlock);
// 		}
// 		break;

// 		case t_asm:
// 		{
// 			struct LinearizationMetadata asmMetadata = m;
// 			asmMetadata.ast = runner;

// 			m.currentTACIndex = walkASMBlock(asmMetadata);
// 		}
// 		break;

// 		// if we see a variable being declared and then assigned
// 		// generate the code and stick it on to the end of the block
// 		case t_uint8:
// 		case t_uint16:
// 		case t_uint32:
// 		{
// 			switch (runner->child->type)
// 			{
// 			case t_single_equals:
// 			{
// 				struct LinearizationMetadata assignmentMetadata = m;
// 				assignmentMetadata.ast = runner->child;

// 				m.currentTACIndex = walkAssignment(assignmentMetadata);
// 			}
// 			break;

// 			case t_star:
// 			{
// 				struct AST *dereferenceScraper = NULL;
// 				scrapePointers(runner->child, &dereferenceScraper);
// 				struct LinearizationMetadata declarationMetadata = m;
// 				declarationMetadata.ast = runner;

// 				m.currentTACIndex = walkDeclaration(declarationMetadata);
// 			}
// 			break;

// 			// if just a declaration, linearize the declaration
// 			case t_lBracket:
// 			case t_identifier:
// 			{
// 				struct LinearizationMetadata declarationMetadata = m;
// 				declarationMetadata.ast = runner;

// 				m.currentTACIndex = walkDeclaration(declarationMetadata);
// 			}
// 			break;

// 			default:
// 				ErrorWithAST(ERROR_INTERNAL, runner->child, "Malformed AST in declaration\n");
// 			}
// 		}
// 		break;

// 		// if we see an assignment, generate the code and stick it on to the end of the block
// 		case t_single_equals:
// 		{
// 			struct LinearizationMetadata assignmentMetadata = m;
// 			assignmentMetadata.ast = runner;

// 			m.currentTACIndex = walkAssignment(assignmentMetadata);
// 		}
// 		break;

// 		case t_lParen:
// 		{
// 			struct LinearizationMetadata callMetadata = m;
// 			callMetadata.ast = runner;

// 			// for a raw call, return value is not used, null out that operand
// 			m.currentTACIndex = walkFunctionCall(callMetadata);

// 			struct TACLine *lastLine = m.currentBlock->TACList->tail->data;
// 			lastLine->operands[0].name.str = NULL;
// 			lastLine->operands[0].type = vt_null;
// 		}
// 		break;

// 		case t_return:
// 		{
// 			struct TACLine *returnTac = newTACLine(m.currentTACIndex, tt_return, runner);
// 			struct LinearizationMetadata returnMetadata = m;
// 			returnMetadata.ast = runner->child;

// 			m.currentTACIndex = walkSubExpression(returnMetadata, returnTac, 0, 0);
// 			returnTac->index = m.currentTACIndex++;

// 			BasicBlock_append(m.currentBlock, returnTac);
// 		}
// 		break;

// 		case t_if:
// 		{
// 			// this is the block that control will be passed to after the branch, regardless of what happens
// 			struct BasicBlock *afterIfBlock = BasicBlock_new((*labelCount)++);

// 			struct LinearizationMetadata ifMetadata = m;
// 			ifMetadata.ast = runner;

// 			// linearize the if statement and attached else if it exists
// 			struct Stack *results = walkIfStatement(ifMetadata, afterIfBlock, labelCount, scopeNesting);

// 			Scope_addBasicBlock(m.scope, afterIfBlock);

// 			struct Stack *beforeConvergeRestores = Stack_New();

// 			// grab all generated basic blocks generated by the if statement's linearization
// 			while (results->size > 0)
// 			{
// 				struct LinearizationResult *poppedResult = Stack_Pop(results);
// 				// skip the current TAC index as far forward as possible so code generated from here on out is always after previous code
// 				if (poppedResult->endingTACIndex > m.currentTACIndex)
// 					m.currentTACIndex = poppedResult->endingTACIndex + 1;

// 				free(poppedResult);
// 			}

// 			Stack_Free(results);

// 			while (beforeConvergeRestores->size > 0)
// 			{
// 				struct TACLine *expireLine = Stack_Pop(beforeConvergeRestores);
// 				expireLine->operands[0].name.val = m.currentTACIndex;
// 			}

// 			Stack_Free(beforeConvergeRestores);

// 			// we are now linearizing code into the block which the branches converge to
// 			m.currentBlock = afterIfBlock;
// 		}
// 		break;

// 		case t_while:
// 		{
// 			struct BasicBlock *afterWhileBlock = BasicBlock_new((*labelCount)++);

// 			struct LinearizationMetadata whileMetadata = m;
// 			whileMetadata.ast = runner;

// 			struct LinearizationResult *r = walkWhileLoop(whileMetadata, afterWhileBlock, labelCount, scopeNesting);
// 			m.currentTACIndex = r->endingTACIndex;
// 			Scope_addBasicBlock(m.scope, afterWhileBlock);
// 			free(r);

// 			m.currentBlock = afterWhileBlock;
// 		}
// 		break;

// 		default:
// 			ErrorWithAST(ERROR_INTERNAL, runner, "Malformed AST in statement\n");
// 		}
// 		runner = runner->sibling;
// 	}

// 	if (controlConvergesTo != NULL)
// 	{
// 		if (m.currentBlock->TACList->tail != NULL)
// 		{
// 			struct TACLine *lastLine = m.currentBlock->TACList->tail->data;
// 			if (lastLine->operation != tt_return)
// 			{
// 				struct TACLine *convergeControlJump = newTACLine(m.currentTACIndex++, tt_jmp, runner);
// 				convergeControlJump->operands[0].name.val = controlConvergesTo->labelNum;
// 				BasicBlock_append(m.currentBlock, convergeControlJump);
// 			}
// 		}
// 	}

// 	struct LinearizationResult *r = malloc(sizeof(struct LinearizationResult));
// 	r->block = m.currentBlock;
// 	r->endingTACIndex = m.currentTACIndex;
// 	Stack_Pop(scopeNesting);
// 	if (scopeNesting->size > 0)
// 		*((int *)Stack_Peek(scopeNesting)) += 1;

// 	return r;
// }

// // given an AST and a populated symbol table, generate three address code for the function entries
// void oldlinearizeProgram(struct AST *it, struct Scope *globalScope, struct Dictionary *dict, struct TempList *temps)
// {
// 	struct BasicBlock *globalBlock = Scope_lookup(globalScope, "globalblock")->entry;

// 	// scrape along the top level of the AST
// 	struct AST *runner = it;
// 	int tempNum = 0;
// 	// start currentTAC index for both program and functions at 1
// 	// this allows anything involved in setup to occur at index 0
// 	// the primary example of this is stating that function arguments exist at index 0, even if they aren't used in the rest of the function
// 	// (particularly applicable for functions that use only asm)
// 	int currentTACIndex = 1;
// 	while (runner != NULL)
// 	{
// 		switch (runner->type)
// 		{
// 		// if we encounter a function, lookup its symbol table entry
// 		// then generate the TAC for it and add a reference to the start of the generated code to the function entry
// 		case t_fun:
// 		{
// 			int funTempNum = 0; // track the number of temporary variables used
// 			int labelCount = 1;
// 			struct FunctionEntry *theFunction = Scope_lookupFun(globalScope, runner->child->child);

// 			// if we know the function is never defined, don't try to walk it
// 			if (!theFunction->isDefined)
// 			{
// 				break;
// 			}

// 			struct AST *functionMainScopeTree = runner->child;

// 			// skip over argument declarations
// 			while (functionMainScopeTree->type != t_pointer_op)
// 			{
// 				functionMainScopeTree = functionMainScopeTree->sibling;
// 			}
// 			// skip over return type
// 			functionMainScopeTree = functionMainScopeTree->sibling;
// 			functionMainScopeTree = functionMainScopeTree->sibling;

// 			// if this is the AST for just the declaration no function body to walk, so bail
// 			if (functionMainScopeTree == NULL)
// 			{
// 				break;
// 			}

// 			struct BasicBlock *functionBlock = BasicBlock_new(funTempNum);

// 			Scope_addBasicBlock(theFunction->mainScope, functionBlock);
// 			struct Stack *scopeStack = Stack_New();
// 			struct LinearizationMetadata functionMetadata;

// 			functionMetadata.dict = dict;
// 			functionMetadata.ast = functionMainScopeTree;
// 			functionMetadata.currentBlock = functionBlock;
// 			functionMetadata.currentTACIndex = 1;
// 			functionMetadata.scope = theFunction->mainScope;
// 			functionMetadata.tempNum = &funTempNum;
// 			struct LinearizationResult *r = walkScope(functionMetadata, NULL, &labelCount, scopeStack);
// 			free(r);
// 			Stack_Free(scopeStack);
// 			break;
// 		}
// 		break;

// 		case t_asm:
// 		{
// 			struct LinearizationMetadata asmMetadata;
// 			asmMetadata.dict = dict;
// 			asmMetadata.ast = runner;
// 			asmMetadata.currentBlock = globalBlock;
// 			asmMetadata.currentTACIndex = currentTACIndex;
// 			asmMetadata.scope = NULL;
// 			asmMetadata.tempNum = NULL;
// 			currentTACIndex = walkASMBlock(asmMetadata);
// 			// currentTACIndex = walkASMBlock(currentTACIndex, globalBlock, runner);
// 		}
// 		break;

// 		case t_uint8:
// 		case t_uint16:
// 		case t_uint32:
// 		{
// 			struct AST *declarationScraper = runner;

// 			// scrape down all pointer levels if necessary, then linearize if the variable is actually assigned
// 			while (declarationScraper->child->type == t_star)
// 				declarationScraper = declarationScraper->child;

// 			declarationScraper = declarationScraper->child;
// 			if (declarationScraper->type == t_single_equals)
// 			{
// 				struct LinearizationMetadata assignmentMetadata;
// 				assignmentMetadata.dict = dict;
// 				assignmentMetadata.ast = declarationScraper;
// 				assignmentMetadata.currentBlock = globalBlock;
// 				assignmentMetadata.currentTACIndex = currentTACIndex;
// 				assignmentMetadata.scope = globalScope;
// 				assignmentMetadata.tempNum = &tempNum;
// 				currentTACIndex = walkAssignment(assignmentMetadata);
// 			}
// 		}
// 		break;

// 		case t_single_equals:
// 		{
// 			struct LinearizationMetadata assignmentMetadata;
// 			assignmentMetadata.dict = dict;
// 			assignmentMetadata.ast = runner;
// 			assignmentMetadata.currentBlock = globalBlock;
// 			assignmentMetadata.currentTACIndex = currentTACIndex;
// 			assignmentMetadata.scope = globalScope;
// 			assignmentMetadata.tempNum = &tempNum;
// 			currentTACIndex = walkAssignment(assignmentMetadata);
// 		}
// 		break;

// 		default:
// 			ErrorWithAST(ERROR_INTERNAL, runner, "Malformed AST for statement at global scope\n");
// 		}
// 		runner = runner->sibling;
// 	}
// }
