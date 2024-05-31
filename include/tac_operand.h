#ifndef TAC_OPERAND_H
#define TAC_OPERAND_H

#include "type.h"

enum variablePermutations
{
    vp_standard,
    vp_temp,
    vp_literal,
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
    enum variablePermutations permutation; // enum of permutation (standard/temp/literal)
};

// Enum denoting how a particular TAC operand is used
enum TACOperandUse
{
    u_unused,
    u_read,
    u_write,
};

void printTACOperand(void *operandData);

ssize_t TACOperand_Compare(void *dataA, void *dataB);

ssize_t TACOperand_CompareIgnoreSsaNumber(void *dataA, void *dataB);

#endif
