#ifndef TAC_OPERAND_H
#define TAC_OPERAND_H

#include "type.h"

#include <mbcl/deque.h>

struct VariableEntry;
struct EnumDesc;
struct Ast;

enum VARIABLE_PERMUTATIONS
{
    VP_UNUSED,
    VP_STANDARD,
    VP_TEMP,
    VP_LITERAL_STR,
    VP_LITERAL_VAL,
};

struct TACOperand
{
    union nameUnion // name of variable as char*, or literal value as int
    {
        // TODO: come up with a better naming scheme than this...
        struct VariableEntry *variable;
        char *str;
        size_t val;
    } name;

    size_t ssaNumber;
    struct Type castAsType;
    enum VARIABLE_PERMUTATIONS permutation; // enum of permutation (standard/temp/literal)
};

char *tac_operand_sprint(void *operandData);

struct Type *tac_operand_get_type(struct TACOperand *operand);

struct Type *tac_operand_get_non_cast_type(struct TACOperand *operand);

ssize_t tac_operand_compare(void *dataA, void *dataB);

ssize_t tac_operand_compare_ignore_ssa_number(void *dataA, void *dataB);

void tac_operand_populate_from_variable(struct TACOperand *operandToPopulate, struct VariableEntry *populateFrom);

void tac_operand_populate_as_temp(struct Scope *scope, struct TACOperand *operandToPopulate, struct Type *type);

// copy over the entire TACOperand, all fields are changed
void tac_operand_copy_decay_arrays(struct TACOperand *dest, struct TACOperand *src);

// copy over only the type and castAsType fields, decaying array sizes to simple pointer types
void tac_operand_copy_type_decay_arrays(struct TACOperand *dest, struct TACOperand *src);

#endif
