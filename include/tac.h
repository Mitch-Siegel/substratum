#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "util.h"

#pragma once

enum basicTypes
{
	vt_null, // type information describing no type at all (only results from declaration of functions with no return)
	vt_any, // type information describing an pointer to indistinct type (a la c's void pointer, but to avoid use of the 'void' keyword, must have indirection level > 0)
	vt_u8,
	vt_u16,
	vt_u32,
	vt_u64,
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
	tt_modulo,
	tt_bitwise_and,
	tt_bitwise_or,
	tt_bitwise_xor,
	tt_bitwise_not,
	tt_lshift,
	tt_rshift,
	tt_load,
	tt_load_off,
	tt_load_arr,
	tt_store,
	tt_store_off,
	tt_store_arr,
	tt_addrof,
	tt_lea_off,
	tt_lea_arr,
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
	tt_pop,
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

// return 0 if 'a' is the same type as 'b', or if it can implicitly be widened to become equivalent
int Type_CompareAllowImplicitWidening(struct Type *a, struct Type *b);

char *Type_GetName(struct Type *t);

void TACOperand_SetBasicType(struct TACOperand *o, enum basicTypes t, int indirectionLevel);

struct TACLine
{
	char *allocFile;
	int allocLine;
	// store the actual tree because some trees are manually generated and do not exist in the true parse tree
	// such as the += operator (a += b is transformed into a tree corresponding to a = a + b)
	struct AST correspondingTree;
	struct TACOperand operands[4];
	enum TACType operation;
	// numerical index relative to other TAC lines
	int index; 
	// numerical index in terms of emitted instructions (from function entry point, populated during code generation)
	int asmIndex;
	char reorderable;
};

struct Type *TACOperand_GetType(struct TACOperand *o);

struct Type *TAC_GetTypeOfOperand(struct TACLine *t, unsigned index);

char *getAsmOp(enum TACType t);

void printTACLine(struct TACLine *it);

char *sPrintTACLine(struct TACLine *it);

struct TACLine *newTACLineFunction(int index, enum TACType operation, struct AST *correspondingTree, char *file, int line);
#define newTACLine(index, operation, correspondingTree) newTACLineFunction((index), (operation), (correspondingTree), __FILE__, __LINE__)

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

void printBasicBlock(struct BasicBlock *b, int indentLevel);

struct LinearizationResult
{
	struct BasicBlock *block;
	int endingTACIndex;
};
