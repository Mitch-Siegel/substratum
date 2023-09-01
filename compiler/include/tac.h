#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "util.h"

#pragma once

enum variableTypes
{
	vt_null,
	vt_uint8,
	vt_uint16,
	vt_uint32,
};

enum variablePermutations
{
	vp_standard,
	vp_temp,
	vp_literal,
	vp_objptr,
};

enum TACType
{
	tt_asm,
	tt_assign,
	tt_cast_assign,
	tt_declare,
	tt_add,
	tt_subtract,
	tt_mul,
	tt_div,
	tt_dereference,
	tt_reference,
	tt_memw_1, // mov (reg), reg
	tt_memw_2, // mov offset(reg), reg
	tt_memw_3, // mov offset(reg, scale), reg
	tt_memw_2_n, // mov offset(reg), reg - offset is subtracted rather than added
	tt_memw_3_n, // mov offset(reg, scale), reg - offset is subtracted rather than added
	tt_memr_1, // same addressing modes as write
	tt_memr_2,
	tt_memr_3,
	tt_memr_2_n,
	tt_memr_3_n,
	tt_cmp,
	tt_jg,
	tt_jge,
	tt_jl,
	tt_jle,
	tt_je,
	tt_jne,
	tt_jmp,
	tt_push,
	tt_call,
	tt_label,
	tt_return,
	tt_do,
	tt_enddo,
};

struct TACOperand
{
	union nameUnion // name of variable as char*, or literal value as int
	{
		char *str;
		int val;
	} name;

	// union nameUnion name;
	enum variableTypes type;			   // enum of type
	enum variablePermutations permutation; // enum of permutation (standard/temp/literal)
	unsigned indirectionLevel;
};

struct TACLine
{
	struct AST *correspondingTree;
	struct TACOperand operands[4];
	enum TACType operation;
	int index;
	char reorderable;
};

char *getAsmOp(enum TACType t);

void printTACLine(struct TACLine *it);

char *sPrintTACLine(struct TACLine *it);

struct TACLine *newTACLine(int index, enum TACType operation, struct AST *correspondingTree);

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
