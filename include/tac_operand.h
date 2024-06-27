#ifndef TAC_OPERAND_H
#define TAC_OPERAND_H

#include "type.h"

enum VARIABLE_PERMUTATIONS
{
    VP_STANDARD,
    VP_TEMP,
    VP_LITERAL,
};

struct TACOperand
{
    union nameUnion // name of variable as char*, or literal value as int
    {
        char *str;
        ssize_t val;
    } name;

    size_t ssaNumber;
    struct Type type;
    struct Type castAsType;
    enum VARIABLE_PERMUTATIONS permutation; // enum of permutation (standard/temp/literal)
};

// Enum denoting how a particular TAC operand is used
enum TAC_OPERAND_USE
{
    U_UNUSED,
    U_READ,
    U_WRITE,
};

void print_tac_operand(void *operandData);

ssize_t tac_operand_compare(void *dataA, void *dataB);

ssize_t tac_operand_compare_ignore_ssa_number(void *dataA, void *dataB);

#endif
