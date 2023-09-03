#include "ast.h"
#include "util.h"
#include "tac.h"
#include "symtab.h"

#pragma once

struct LinearizationMetadata
{
	struct Dictionary *dict; // include the dict for literals and other things that require processing during linearization
	int currentTACIndex;
	struct BasicBlock *currentBlock;
	struct AST *ast;
	int *tempNum;
	struct Scope *scope;
};

struct SymbolTable *linearizeProgram(struct AST *program);

struct VariableEntry *walkVariableDeclaration(struct AST *tree,
											  struct BasicBlock *block,
											  struct Scope *scope,
											  int *TACIndex,
											  int *tempNum,
											  char isArgument);

void walkArgumentDeclaration(struct AST *tree,
							 struct BasicBlock *block,
							 int *TACIndex,
							 int *tempNum,
							 struct FunctionEntry *fun);

void walkFunctionDeclaration(struct AST *tree,
							 struct Scope *scope);

void walkFunctionDefinition(struct AST *tree,
							struct FunctionEntry *fun);

void walkScope(struct AST *tree,
			   struct BasicBlock *block,
			   struct Scope *scope,
			   int *TACIndex,
			   int *tempNum,
			   int *labelNum,
			   int controlConvergesToLabel);

void walkConditionCheck(struct AST *tree,
						struct BasicBlock *block,
						struct Scope *scope,
						int *TACIndex,
						int *tempNum,
						int falseJumpLabelNum);

void walkWhileLoop(struct AST *tree,
				   struct BasicBlock *block,
				   struct Scope *scope,
				   int *TACIndex,
				   int *tempNum,
				   int *labelNum,
				   int controlConvergesToLabel);

void walkIfStatement(struct AST *tree,
					 struct BasicBlock *block,
					 struct Scope *scope,
					 int *TACIndex,
					 int *tempNum,
					 int *labelNum,
					 int controlConvergesToLabel);

void walkAssignment(struct AST *tree,
					struct BasicBlock *block,
					struct Scope *scope,
					int *TACIndex,
					int *tempNum);

void walkSubExpression(struct AST *tree,
					   struct BasicBlock *block,
					   struct Scope *scope,
					   int *TACIndex,
					   int *tempNum,
					   struct TACOperand *destinationOperand);

void walkFunctionCall(struct AST *tree,
					  struct BasicBlock *block,
					  struct Scope *scope,
					  int *TACIndex,
					  int *tempNum,
					  struct TACOperand *destinationOperand);

struct TACOperand *walkExpression(struct AST *tree,
								  struct BasicBlock *block,
								  struct Scope *scope,
								  int *TACIndex,
								  int *tempNum);

struct TACOperand *walkArrayRef(struct AST *tree,
								struct BasicBlock *block,
								struct Scope *scope,
								int *TACIndex,
								int *tempNum);

struct TACOperand *walkDereference(struct AST *tree,
								   struct BasicBlock *block,
								   struct Scope *scope,
								   int *TACIndex,
								   int *tempNum);

void walkAsmBlock(struct AST *tree,
				  struct BasicBlock *block,
				  struct Scope *scope,
				  int *TACIndex,
				  int *tempNum);

// int walkASMBlock(struct LinearizationMetadata m);

// int walkDereference(struct LinearizationMetadata m);

// int walkArgumentPushes(struct LinearizationMetadata m, struct FunctionEntry *f);

// int walkFunctionCall(struct LinearizationMetadata m);

// int walkSubExpression(struct LinearizationMetadata m,
// 					  struct TACLine *parentExpression,
// 					  int operandIndex,
// 					  char forceConstantToRegister);

// int walkExpression(struct LinearizationMetadata m);

// int walkArrayRef(struct LinearizationMetadata m);

// int walkAssignment(struct LinearizationMetadata m);

// struct TACLine *walkConditionalJump(int currentTACIndex,
// 									struct AST *cmpOp,
// 									char whichCondition); // jump on condition true if nonzero, jump on condition false if zero

// int walkConditionCheck(struct LinearizationMetadata m,
// 					   char whichCondition, // jump on condition true if nonzero, jump on condition false if zero
// 					   int targetLabel,
// 					   int *labelCount, // label index tracking in starting block
// 					   int depth);

// struct Stack *walkIfStatement(struct LinearizationMetadata m,
// 							  struct BasicBlock *afterIfBlock,
// 							  int *labelCount,
// 							  struct Stack *scopenesting);

// struct LinearizationResult *walkWhileLoop(struct LinearizationMetadata m,
// 										  struct BasicBlock *afterIfBlock,
// 										  int *labelCount,
// 										  struct Stack *scopenesting);

// struct LinearizationResult *walkScope(struct LinearizationMetadata m,
// 									  struct BasicBlock *controlConvergesTo,
// 									  int *labelCount,
// 									  struct Stack *scopenesting);

// void linearizeProgram(struct AST *it, struct Scope *globalScope, struct Dictionary *dict, struct TempList *temps);
