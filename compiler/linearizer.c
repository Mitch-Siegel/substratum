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

enum basicTypes selectVariableTypeForNumber(int num)
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

enum basicTypes selectVariableTypeForLiteral(char *literal)
{
	int literalAsNumber = atoi(literal);
	return selectVariableTypeForNumber(literalAsNumber);
}

void populateTACOperandFromVariable(struct TACOperand *o, struct VariableEntry *e)
{
	o->type = e->type;
	o->name.str = e->name;
	o->permutation = vp_standard;
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
	int declaredArraySize = 1;

	struct Type declaredType;
	declaredType.basicType = declaredBasicType;
	declaredType.indirectionLevel = declaredIndirectionLevel;

	// if we are declaring an array, set the string with the size as the second operand
	if (declaredTree->type == t_lBracket)
	{
		declaredTree = declaredTree->child;
		char *arraySizeString = declaredTree->sibling->value;
		declarationLine->operands[1].name.str = arraySizeString;
		declaredArraySize = atoi(arraySizeString);
		declarationLine->operands[1].permutation = vp_literal;
		declarationLine->operands[1].type.basicType = selectVariableTypeForLiteral(arraySizeString);
	}

	struct VariableEntry *declaredVariable = Scope_createVariable(scope,
																  declaredTree,
																  &declaredType,
																  declaredArraySize,
																  (scope->parentScope == NULL),
																  declarationLine->index,
																  isArgument);
	declarationLine->operands[0].name.str = declaredTree->value;
	TACOperand_SetBasicType(&declarationLine->operands[0], declaredBasicType, declaredIndirectionLevel);

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
			TACOperand_SetBasicType(&compareOperation->operands[2], vt_uint32, 0);
		}
		break;

	default:
		// any other sort of condition - just some expression
		{
			walkSubExpression(tree, block, scope, TACIndex, tempNum, &compareOperation->operands[1]);
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

		if (arrayVariable->type.indirectionLevel < 1)
		{
			ErrorWithAST(ERROR_CODE, arrayName, "Use of non-pointer variable %s as array!\n", arrayName->value);
		}

		assignment->operation = tt_memw_3;
		populateTACOperandFromVariable(&assignment->operands[0], arrayVariable);

		assignment->operands[2].permutation = vp_literal;
		assignment->operands[2].type.indirectionLevel = 0;
		assignment->operands[2].type.basicType = vt_uint8;
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
		destinationOperand->type.basicType = selectVariableTypeForLiteral(tree->value);
		destinationOperand->permutation = vp_literal;
		break;

	case t_char_literal:
		// char literal
		{
			char literalAsNumber[8];
			sprintf(literalAsNumber, "%d", tree->value[0]);
			destinationOperand->name.str = Dictionary_LookupOrInsert(parseDict, literalAsNumber);
			destinationOperand->type.basicType = selectVariableTypeForLiteral(literalAsNumber);
			destinationOperand->permutation = vp_literal;
		}
		break;

	case t_string_literal:
		// string literal
		walkStringLiteral(tree, block, scope, destinationOperand);
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
		walkSubExpression(pushedArgument, block, scope, TACIndex, tempNum, &push->operands[0]);

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
		if (Scope_getSizeOfType(scope, &push->operands[0].type) <= Scope_getSizeOfType(scope, &expectedArgument->type))
		{
			push->operands[0].castAsType = expectedArgument->type;
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
		call->operands[0].type.indirectionLevel = calledFunction->returnType.indirectionLevel;
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
			if ((operandA->type.indirectionLevel > 0) && (operandB->type.indirectionLevel > 0))
			{
				ErrorWithAST(ERROR_CODE, tree, "Arithmetic between 2 pointers is not allowed!\n");
			}

			if (Scope_getSizeOfType(scope, &operandA->type) > Scope_getSizeOfType(scope, &operandB->type))
			{
				expression->operands[0].type = operandA->type;
			}
			else
			{
				expression->operands[0].type = operandB->type;
			}

			if (operandA->type.indirectionLevel > operandB->type.indirectionLevel)
			{
				expression->operands[0].type.indirectionLevel = operandA->type.indirectionLevel;
			}
			else
			{
				expression->operands[0].type.indirectionLevel = operandB->type.indirectionLevel;
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

	arrayRefTAC->operands[0].type = arrayRefTAC->operands[1].type;
	if (arrayRefTAC->operands[1].type.indirectionLevel > 0)
	{
		arrayRefTAC->operands[0].type.indirectionLevel = arrayRefTAC->operands[1].type.indirectionLevel - 1;
	}
	else
	{
		ErrorWithAST(ERROR_CODE, tree, "Use of non-pointer variable %s in array reference!\n", arrayVariable->name);
	}

	if (arrayIndex->type == t_constant)
	{
		arrayRefTAC->operation = tt_memr_2;

		int indexSize = atoi(arrayIndex->value);
		if (arrayVariable->type.indirectionLevel == 1)
		{
			indexSize *= Scope_getSizeOfType(scope, &arrayVariable->type);
		}
		else
		{
			indexSize *= alignSize(Scope_getSizeOfArrayElement(scope, arrayVariable));
		}

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
	if (tree->type != t_star)
	{
		ErrorWithAST(ERROR_INTERNAL, tree, "Invalid AST type (%s) passed to walkDereference!\n", getTokenName(tree->type));
	}

	struct TACLine *dereference = newTACLine((*tempNum), tt_dereference, tree);

	walkSubExpression(tree->child, block, scope, TACIndex, tempNum, &dereference->operands[1]);

	dereference->operands[0].type = dereference->operands[1].type;
	dereference->operands[0].type.indirectionLevel = dereference->operands[1].type.indirectionLevel - 1;
	dereference->operands[0].name.str = TempList_Get(temps, (*tempNum)++);
	dereference->operands[0].permutation = vp_temp;

	dereference->index = (*tempNum)++;
	BasicBlock_append(block, dereference);

	return &dereference->operands[0];
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

void walkStringLiteral(struct AST *tree,
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
	Scope_createStringLiteral(scope, stringName);

	TACOperand_SetBasicType(destinationOperand, vt_uint8, 1);
	destinationOperand->permutation = vp_objptr;
	destinationOperand->name.str = stringName;
}
