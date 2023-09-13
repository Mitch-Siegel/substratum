#include "ast.h"
#include "util.h"
#include "tac.h"
#include "symtab.h"
#include "linearizer.h"

#pragma once

struct SymbolTable *walkProgram_0(struct AST *program);

struct VariableEntry *walkVariableDeclaration_0(struct AST *tree,
												struct BasicBlock *block,
												struct Scope *scope,
												int *TACIndex,
												int *tempNum,
												char isArgument);

void walkArgumentDeclaration_0(struct AST *tree,
							   struct BasicBlock *block,
							   int *TACIndex,
							   int *tempNum,
							   struct FunctionEntry *fun);

void walkFunctionDeclaration_0(struct AST *tree,
							   struct Scope *scope);

void walkFunctionDefinition_0(struct AST *tree,
							  struct FunctionEntry *fun);

void walkScope_0(struct AST *tree,
				 struct BasicBlock *block,
				 struct Scope *scope,
				 int *TACIndex,
				 int *tempNum,
				 int *labelNum,
				 int controlConvergesToLabel);

void walkConditionCheck_0(struct AST *tree,
						  struct BasicBlock *block,
						  struct Scope *scope,
						  int *TACIndex,
						  int *tempNum,
						  int falseJumpLabelNum);

void walkWhileLoop_0(struct AST *tree,
					 struct BasicBlock *block,
					 struct Scope *scope,
					 int *TACIndex,
					 int *tempNum,
					 int *labelNum,
					 int controlConvergesToLabel);

void walkIfStatement_0(struct AST *tree,
					   struct BasicBlock *block,
					   struct Scope *scope,
					   int *TACIndex,
					   int *tempNum,
					   int *labelNum,
					   int controlConvergesToLabel);

void walkAssignment_0(struct AST *tree,
					  struct BasicBlock *block,
					  struct Scope *scope,
					  int *TACIndex,
					  int *tempNum);

void walkArithmeticAssignment_0(struct AST *tree,
					  struct BasicBlock *block,
					  struct Scope *scope,
					  int *TACIndex,
					  int *tempNum);

void walkSubExpression_0(struct AST *tree,
						 struct BasicBlock *block,
						 struct Scope *scope,
						 int *TACIndex,
						 int *tempNum,
						 struct TACOperand *destinationOperand);

void walkFunctionCall_0(struct AST *tree,
						struct BasicBlock *block,
						struct Scope *scope,
						int *TACIndex,
						int *tempNum,
						struct TACOperand *destinationOperand);

struct TACOperand *walkExpression_0(struct AST *tree,
									struct BasicBlock *block,
									struct Scope *scope,
									int *TACIndex,
									int *tempNum);

struct TACOperand *walkArrayRef_0(struct AST *tree,
								  struct BasicBlock *block,
								  struct Scope *scope,
								  int *TACIndex,
								  int *tempNum);

struct TACOperand *walkDereference_0(struct AST *tree,
									 struct BasicBlock *block,
									 struct Scope *scope,
									 int *TACIndex,
									 int *tempNum);

void walkPointerArithmetic_0(struct AST *tree,
							 struct BasicBlock *block,
							 struct Scope *scope,
							 int *TACIndex,
							 int *tempNum,
							 struct TACOperand *destinationOperand);

void walkAsmBlock_0(struct AST *tree,
					struct BasicBlock *block,
					struct Scope *scope,
					int *TACIndex,
					int *tempNum);

void walkStringLiteral_0(struct AST *tree,
						 struct BasicBlock *block,
						 struct Scope *scope,
						 struct TACOperand *destinationOperand);
