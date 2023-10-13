#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "util.h"

#pragma once

enum basicTypes
{
	vt_null,
	vt_uint8,
	vt_uint16,
	vt_uint32,
	vt_class
};

enum variablePermutations
{
	vp_standard,
	vp_temp,
	vp_literal,
	vp_objptr, // TODO: clean up after this
};

enum TACType
{
	tt_asm,
	tt_assign,
	tt_add,
	tt_subtract,
	tt_mul,
	tt_div,
	tt_load,
	tt_store,
	tt_addrof,
	tt_beq,	 // branch equal
	tt_bne,	 // branch not equal
	tt_bgeu, // branch greater than or equal unsigned
	tt_bltu, // branch less than unsigned
	tt_bgtu, // branch greater than unsigned
	tt_bleu, // branch less than or equal unsigned
	tt_beqz, // branch equal zero
	tt_bnez, // branch not equal zero
	tt_jmp,
	tt_push,
	tt_call,
	tt_label,
	tt_return,
	tt_do,
	tt_enddo,
};

struct Type
{
	enum basicTypes basicType;
	int indirectionLevel;
	int arraySize;
	union
	{
		char *initializeTo;
		char **initializeArrayTo;
	};
	struct classType
	{
		char *name;
	} classType;
};

struct TACOperand
{
	union nameUnion // name of variable as char*, or literal value as int
	{
		char *str;
		int val;
	} name;

	struct Type type;
	struct Type castAsType;
	enum variablePermutations permutation; // enum of permutation (standard/temp/literal)
};

int Type_Compare(struct Type *a, struct Type *b);

int Type_CompareAllowImplicitWidening(struct Type *a, struct Type *b);

char *Type_GetName(struct Type *t);

void TACOperand_SetBasicType(struct TACOperand *o, enum basicTypes t, int indirectionLevel);

struct TACLine
{
	char *allocFile;
	int allocLine;
	struct AST *correspondingTree;
	struct TACOperand operands[4];
	enum TACType operation;
	int index;
	char reorderable;
};

struct Type *TACOperand_GetType(struct TACOperand *o);

struct Type *TAC_GetTypeOfOperand(struct TACLine *t, unsigned index);

char *getAsmOp(enum TACType t);

void printTACLine(struct TACLine *it);

char *sPrintTACLine(struct TACLine *it);

struct TACLine *newTACLineFunction(int index, enum TACType operation, struct AST *correspondingTree, char *file, int line);
#define newTACLine(index, operation, correspondingTree) newTACLineFunction((index), (operation), (correspondingTree), __FILE__, __LINE__)

char checkTACLine(struct TACLine *it);

void freeTAC(struct TACLine *it);

char TACLine_isEffective(struct TACLine *it);

struct BasicBlock
{
	struct LinkedList *TACList;
	int labelNum;
	// only set when the block contains TAC lines containing operations other than code generator directives
	char containsEffectiveCode;
};

struct BasicBlock *BasicBlock_new(int labelNum);

void BasicBlock_free(struct BasicBlock *b);

void BasicBlock_append(struct BasicBlock *b, struct TACLine *l);

void BasicBlock_prepend(struct BasicBlock *b, struct TACLine *l);

struct TACLine *findLastEffectiveTAC(struct BasicBlock *b);

void printBasicBlock(struct BasicBlock *b, int indentLevel);

struct LinearizationResult
{
	struct BasicBlock *block;
	int endingTACIndex;
};
