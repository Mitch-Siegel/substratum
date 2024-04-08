#ifndef TAC_H
#define TAC_H

#include "ast.h"
#include "type.h"

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
    tt_beq,  // branch equal
    tt_bne,  // branch not equal
    tt_bgeu, // branch greater than or equal unsigned
    tt_bltu, // branch less than unsigned
    tt_bgtu, // branch greater than unsigned
    tt_bleu, // branch less than or equal unsigned
    tt_beqz, // branch equal zero
    tt_bnez, // branch not equal zero
    tt_jmp,
    tt_stack_reserve, // decrement the stack pointer before a series of stack_store ops
    tt_stack_store,   // store a value at a (positive) offset from the stack pointer
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
        ssize_t val;
    } name;

    struct Type type;
    struct Type castAsType;
    enum variablePermutations permutation; // enum of permutation (standard/temp/literal)
};

void TACOperand_SetBasicType(struct TACOperand *operand, enum basicTypes type, int indirectionLevel);

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

struct Type *TACOperand_GetType(struct TACOperand *operand);

struct Type *TAC_GetTypeOfOperand(struct TACLine *line, unsigned index);

char *getAsmOp(enum TACType tacOperation);

void printTACLine(struct TACLine *line);

char *sPrintTACLine(struct TACLine *line);

struct TACLine *newTACLineFunction(int index, enum TACType operation, struct AST *correspondingTree, char *file, int line);
#define newTACLine(index, operation, correspondingTree) newTACLineFunction((index), (operation), (correspondingTree), __FILE__, __LINE__)

void freeTAC(struct TACLine *line);

struct LinearizationResult
{
    struct BasicBlock *block;
    int endingTACIndex;
};

#endif
