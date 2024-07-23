#ifndef TAC_H
#define TAC_H

#include "ast.h"
#include "tac_operand.h"
#include "type.h"

#define N_TAC_OPERANDS_IN_LINE 3

enum TAC_TYPE
{
    TT_ASM,
    TT_ASM_LOAD,
    TT_ASM_STORE,
    TT_ASSIGN,
    TT_ADD,
    TT_SUBTRACT,
    TT_MUL,
    TT_DIV,
    TT_MODULO,
    TT_BITWISE_AND,
    TT_BITWISE_OR,
    TT_BITWISE_XOR,
    TT_BITWISE_NOT,
    TT_LSHIFT,
    TT_RSHIFT,
    TT_LOAD,
    TT_STORE,
    TT_ADDROF,
    TT_ARRAY_LOAD,  // load an array element
    TT_ARRAY_LEA,   // load a pointer to an array element
    TT_ARRAY_STORE, // store an array element
    TT_FIELD_LOAD,  // load a field of a struct
    TT_FIELD_LEA,   // load a pointer to a field of a struct
    TT_FIELD_STORE, // store a field of a struct
    TT_BEQ,         // branch equal
    TT_BNE,         // branch not equal
    TT_BGEU,        // branch greater than or equal unsigned
    TT_BLTU,        // branch less than unsigned
    TT_BGTU,        // branch greater than unsigned
    TT_BLEU,        // branch less than or equal unsigned
    TT_BEQZ,        // branch equal zero
    TT_BNEZ,        // branch not equal zero
    TT_JMP,
    TT_ARG_STORE,       // store a value at a (positive) offset from the stack pointer
    TT_FUNCTION_CALL,   // call a function
    TT_METHOD_CALL,     // call a method of a struct
    TT_ASSOCIATED_CALL, // call an associated function of a struct
    TT_LABEL,
    TT_RETURN,
    TT_DO,
    TT_ENDDO,
    TT_PHI,
};

struct TACLine
{
    char *allocFile;
    int allocLine;
    // store the actual tree because some trees are manually generated and do not exist in the true parse tree
    // such as the += operator (a += b is transformed into a tree corresponding to a = a + b)
    struct Ast correspondingTree;
    struct TACOperand operands[N_TAC_OPERANDS_IN_LINE];
    enum TAC_TYPE operation;
    // numerical index relative to other TAC lines
    size_t index;
    // numerical index in terms of emitted instructions (from function entry point, populated during code generation)
    size_t asmIndex;
    u8 reorderable;
};

struct Type *tac_get_type_of_operand(struct TACLine *line, unsigned index);

char *tac_operation_get_name(enum TAC_TYPE tacOperation);

void print_tac_line(struct TACLine *line);

char *sprint_tac_line(struct TACLine *line);

struct TACLine *new_tac_line_function(enum TAC_TYPE operation, struct Ast *correspondingTree, char *file, int line);
#define new_tac_line(operation, correspondingTree) new_tac_line_function((operation), (correspondingTree), __FILE__, __LINE__)

void free_tac(struct TACLine *line);

enum TAC_OPERAND_USE get_use_of_operand(struct TACLine *line, u8 operandIndex);

struct LinearizationResult
{
    struct BasicBlock *block;
    int endingTACIndex;
};

#endif
