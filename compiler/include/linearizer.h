#include "ast.h"
#include "util.h"
#include "tac.h"
#include "symtab.h"

#pragma once

struct LinearizationMetadata
{
	int currentTACIndex;
	struct BasicBlock *currentBlock;
	struct AST *ast;
	int *tempNum;
	struct TempList *temps;
	struct Scope *scope;
};

int linearizeASMBlock(struct LinearizationMetadata m);

int linearizeDereference(struct LinearizationMetadata m);

int linearizeArgumentPushes(struct LinearizationMetadata m, struct FunctionEntry *f);

int linearizeFunctionCall(struct LinearizationMetadata m);

int linearizeSubExpression(struct LinearizationMetadata m,
						   struct TACLine *parentExpression,
						   int operandIndex, 
						   char forceConstantToRegister);

int linearizeExpression(struct LinearizationMetadata m);

int linearizeArrayRef(struct LinearizationMetadata m);

int linearizeAssignment(struct LinearizationMetadata m);

struct TACLine *linearizeConditionalJump(int currentTACIndex,
										 struct AST *cmpOp,
										 char whichCondition); // jump on condition true if nonzero, jump on condition false if zero

int linearizeDeclaration(struct LinearizationMetadata m);

int linearizeConditionCheck(struct LinearizationMetadata m,
							char whichCondition, // jump on condition true if nonzero, jump on condition false if zero
							int targetLabel,
							int *labelCount, // label index tracking in starting block
							int depth);

struct Stack *linearizeIfStatement(struct LinearizationMetadata m,
								   struct BasicBlock *afterIfBlock,
								   int *labelCount,
								   struct Stack *scopenesting);

struct LinearizationResult *linearizeWhileLoop(struct LinearizationMetadata m,
											   struct BasicBlock *afterIfBlock,
											   int *labelCount,
											   struct Stack *scopenesting);

struct LinearizationResult *linearizeScope(struct LinearizationMetadata m,
										   struct BasicBlock *controlConvergesTo,
										   int *labelCount,
										   struct Stack *scopenesting);

void linearizeProgram(struct AST *it, struct Scope *globalScope, struct Dictionary *dict, struct TempList *temps);
