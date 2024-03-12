#include "ast.h"
#include "util.h"
#include "tac.h"
#include "symtab.h"

#pragma once

struct SymbolTable *walkProgram(struct AST *program);

void walkTypeName(struct AST *tree, struct Scope *scope, struct Type *populateTypeTo);

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

void walkClassDeclaration(struct AST *tree,
						  struct BasicBlock *block,
						  struct Scope *scope);

void walkStatement(struct AST *tree,
				   struct BasicBlock **blockP,
				   struct Scope *scope,
				   int *TACIndex,
				   int *tempNum,
				   int *labelNum,
				   int controlConvergesToLabel);

void walkScope(struct AST *tree,
			   struct BasicBlock *block,
			   struct Scope *scope,
			   int *TACIndex,
			   int *tempNum,
			   int *labelNum,
			   int controlConvergesToLabel);

// walk the logical operator pointed to by the AST
// returns the basic block which will be executed if the condition is met
// jumps to falseJumpLabelNum if the condition is not met
struct BasicBlock *walkLogicalOperator(struct AST *tree,
									   struct BasicBlock *block,
									   struct Scope *scope,
									   int *TACIndex,
									   int *tempNum,
									   int *labelNum,
									   int falseJumpLabelNum);

// walk the condition check pointed to by the AST
// returns the basic block which will be executed if the condition is met
// jumps to falseJumpLabelNum if the condition is not met
struct BasicBlock *walkConditionCheck(struct AST *tree,
									  struct BasicBlock *block,
									  struct Scope *scope,
									  int *TACIndex,
									  int *tempNum,
									  int *labelNum,
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

void walkArithmeticAssignment(struct AST *tree,
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

struct TACLine *walkMemberAccess(struct AST *tree,
								 struct BasicBlock *block,
								 struct Scope *scope,
								 int *TACIndex,
								 int *tempNum,
								 struct TACOperand *srcDestOperand,
								 int depth);

struct TACOperand *walkExpression(struct AST *tree,
								  struct BasicBlock *block,
								  struct Scope *scope,
								  int *TACIndex,
								  int *tempNum);

struct TACLine *walkArrayRef(struct AST *tree,
							 struct BasicBlock *block,
							 struct Scope *scope,
							 int *TACIndex,
							 int *tempNum);

struct TACOperand *walkDereference(struct AST *tree,
								   struct BasicBlock *block,
								   struct Scope *scope,
								   int *TACIndex,
								   int *tempNum);

struct TACOperand *walkAddrOf(struct AST *tree,
							  struct BasicBlock *block,
							  struct Scope *scope,
							  int *TACIndex,
							  int *tempNum);

void walkPointerArithmetic(struct AST *tree,
						   struct BasicBlock *block,
						   struct Scope *scope,
						   int *TACIndex,
						   int *tempNum,
						   struct TACOperand *destinationOperand);

void walkAsmBlock(struct AST *tree,
				  struct BasicBlock *block,
				  struct Scope *scope,
				  int *TACIndex,
				  int *tempNum);

void walkStringLiteral(struct AST *tree,
					   struct BasicBlock *block,
					   struct Scope *scope,
					   struct TACOperand *destinationOperand);
