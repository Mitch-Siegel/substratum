#include "tac.h"

#include "log.h"
#include "util.h"
#include <stdio.h>

struct Type *tac_get_type_of_operand(struct TACLine *line, unsigned index)
{
    if (index > 3)
    {
        InternalError("Bad index %d passed to TacGetTypeOfOperand!", index);
    }

    return tac_operand_get_type(&line->operands[index]);
}

char *get_asm_op(enum TAC_TYPE tacOperation)
{
    switch (tacOperation)
    {
    case TT_ASM:
        return "tac-asm";
    case TT_ASSIGN:
        return "tac-assign";
    case TT_ADD:
        return "add";
    case TT_SUBTRACT:
        return "sub";
    case TT_MUL:
        return "mul";
    case TT_DIV:
        return "div";
    case TT_MODULO:
        return "rem";
    case TT_BITWISE_AND:
        return "and";
    case TT_BITWISE_OR:
        return "or";
    case TT_BITWISE_XOR:
        return "xor";
    case TT_BITWISE_NOT:
        return "~";
    case TT_LSHIFT:
        return "sll";
    case TT_RSHIFT:
        return "srl";
    case TT_LOAD:
        return "load";
    case TT_LOAD_OFF:
        return "load (literal offset)";
    case TT_LOAD_ARR:
        return "load (array indexed)";
    case TT_STORE:
        return "store";
    case TT_STORE_OFF:
        return "store (literal offset)";
    case TT_STORE_ARR:
        return "store (array indexed)";
    case TT_ADDROF:
        return "address-of";
    case TT_LEA_OFF:
        return "lea (literal offset)";
    case TT_LEA_ARR:
        return "lea (array indexed)";
    case TT_FIELD_LOAD:
        return "struct field load";
    case TT_FIELD_LEA:
        return "struct field load pointer";
    case TT_FIELD_STORE:
        return "struct field store";
    case TT_FUNCTION_CALL:
        return "function call";
    case TT_METHOD_CALL:
        return "method call";
    case TT_ASSOCIATED_CALL:
        return "associated call";
    case TT_LABEL:
        return ".";
    case TT_RETURN:
        return "ret";
    case TT_DO:
        return "do";
    case TT_ENDDO:
        return "end do";
    case TT_BEQ:
        return "beq";
    case TT_BNE:
        return "bne";
    case TT_BGEU:
        return "bgeu";
    case TT_BLTU:
        return "bltu";
    case TT_BGTU:
        return "bgtu";
    case TT_BLEU:
        return "bleu";
    case TT_BEQZ:
        return "beqz";
    case TT_BNEZ:
        return "bnez";
    case TT_JMP:
        return "jmp";
    case TT_ARG_STORE:
        return "arg store";
    case TT_PHI:
        return "phi";
    }
    return "";
}

struct TACLine *new_tac_line_function(enum TAC_TYPE operation, struct Ast *correspondingTree, char *file, int line)
{
    struct TACLine *wip = malloc(sizeof(struct TACLine));
    wip->allocFile = file;
    wip->allocLine = line;
    for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
    {
        wip->operands[operandIndex].name.str = NULL;
        wip->operands[operandIndex].ssaNumber = 0;
        wip->operands[operandIndex].permutation = VP_STANDARD;

        type_init(&wip->operands[operandIndex].type);
        type_init(&wip->operands[operandIndex].castAsType);
    }
    wip->correspondingTree = *correspondingTree;

    wip->operation = operation;
    // by default operands are NOT reorderable
    wip->reorderable = 0;
    wip->index = 0;
    wip->asmIndex = 0;
    return wip;
}

void print_tac_line(struct TACLine *line)
{
    char *printedLine = sprint_tac_line(line);
    printf("%s", printedLine);
    free(printedLine);
}

char *s_print_tac_operands(struct TACLine *line)
{
    const u32 SPRINT_TAC_OPERAND_LENGTH = 128;
    ssize_t width = 0;

    char *operandString = malloc(SPRINT_TAC_OPERAND_LENGTH * sizeof(char));

    for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
    {
        if (line->operands[operandIndex].type.basicType != VT_NULL)
        {
            width += sprintf(operandString + width, "[");
            switch (line->operands[operandIndex].permutation)
            {
            case VP_STANDARD:
                width += sprintf(operandString + width, " ");
                break;

            case VP_TEMP:
                width += sprintf(operandString + width, "T");
                break;

            case VP_LITERAL:
                width += sprintf(operandString + width, "L");
                break;
            }

            char *typeName = type_get_name(&line->operands[operandIndex].type);
            width += sprintf(operandString + width, " %s", typeName);
            free(typeName);
            if (line->operands[operandIndex].castAsType.basicType != VT_NULL)
            {
                char *castAsTypeName = type_get_name(&line->operands[operandIndex].castAsType);
                width += sprintf(operandString + width, "(%s)", castAsTypeName);
                free(castAsTypeName);
            }
            width += sprintf(operandString + width, "]");
        }
        else
        {
            width += sprintf(operandString + width, "   -   ");
        }
    }

    return operandString;
}

ssize_t s_print_arithmetic_operation(char *tacString, ssize_t width, char *operationStr, struct TACOperand operands[3])
{
    return sprintf(tacString + width, "%s!%zu = %s!%zu %s %s!%zu", operands[0].name.str, operands[0].ssaNumber, operands[1].name.str, operands[1].ssaNumber, operationStr, operands[2].name.str, operands[2].ssaNumber);
}

char *sprint_tac_line(struct TACLine *line)
{
    const u32 SPRINT_TAC_LINE_LENGTH = 256;
    char *tacString = malloc(SPRINT_TAC_LINE_LENGTH * sizeof(char));
    ssize_t width = sprintf(tacString, "%2lx:", line->index);
    switch (line->operation)
    {
    case TT_ASM:
        width += sprintf(tacString + width, "ASM:%s", line->operands[0].name.str);
        break;

    case TT_ADD:
        width += s_print_arithmetic_operation(tacString, width, "+", line->operands);
        break;

    case TT_SUBTRACT:
        width += s_print_arithmetic_operation(tacString, width, "-", line->operands);
        break;

    case TT_MUL:
        width += s_print_arithmetic_operation(tacString, width, "*", line->operands);
        break;

    case TT_DIV:
        width += s_print_arithmetic_operation(tacString, width, "/", line->operands);
        break;

    case TT_MODULO:
        width += s_print_arithmetic_operation(tacString, width, "%", line->operands);
        break;

    case TT_BITWISE_AND:
        width += s_print_arithmetic_operation(tacString, width, "&", line->operands);
        break;

    case TT_BITWISE_OR:
        width += s_print_arithmetic_operation(tacString, width, "|", line->operands);
        break;

    case TT_BITWISE_XOR:
        width += s_print_arithmetic_operation(tacString, width, "^", line->operands);
        break;

    case TT_LSHIFT:
        width += s_print_arithmetic_operation(tacString, width, "<<", line->operands);
        break;

    case TT_RSHIFT:
        width += s_print_arithmetic_operation(tacString, width, ">>", line->operands);
        break;

    case TT_BITWISE_NOT:
        width += sprintf(tacString + width, "%s!%zu = ~%s!%zu", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.str, line->operands[1].ssaNumber);
        break;

    case TT_LOAD:
        width += sprintf(tacString + width, "%s!%zu = *%s!%zu", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.str, line->operands[1].ssaNumber);
        break;

    case TT_LOAD_OFF:
        // operands: dest base offset
        width += sprintf(tacString + width, "%s!%zu = (%s!%zu + %ld)", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.str, line->operands[1].ssaNumber, line->operands[2].name.val);
        break;

    case TT_LOAD_ARR:
        // operands: dest base offset scale
        width += sprintf(tacString + width, "%s!%zu = (%s!%zu + %s!%zu*2^%ld)", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.str, line->operands[1].ssaNumber, line->operands[2].name.str, line->operands[2].ssaNumber, line->operands[3].name.val);
        break;

    case TT_STORE:
        width += sprintf(tacString + width, "*%s!%zu = %s!%zu", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.str, line->operands[1].ssaNumber);
        break;

    case TT_STORE_OFF:
        // operands: base offset source
        width += sprintf(tacString + width, "(%s!%zu + %ld) = %s!%zu", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.val, line->operands[2].name.str, line->operands[2].ssaNumber);
        break;

    case TT_STORE_ARR:
        // operands base offset scale source
        width += sprintf(tacString + width, "(%s!%zu + %s!%zu*2^%ld) = %s!%zu", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.str, line->operands[1].ssaNumber, line->operands[2].name.val, line->operands[3].name.str, line->operands[3].ssaNumber);
        break;

    case TT_ADDROF:
        width += sprintf(tacString + width, "%s!%zu = &%s!%zu", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.str, line->operands[1].ssaNumber);
        break;

    case TT_LEA_OFF:
        // operands: dest base offset
        width += sprintf(tacString + width, "%s!%zu = &(%s!%zu + %ld)", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.str, line->operands[1].ssaNumber, line->operands[2].name.val);
        break;

    case TT_LEA_ARR:
        // operands: dest base offset scale
        width += sprintf(tacString + width, "%s!%zu = &(%s!%zu + %s!%zu*2^%ld)", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.str, line->operands[1].ssaNumber, line->operands[2].name.str, line->operands[2].ssaNumber, line->operands[3].name.val);
        break;

    case TT_FIELD_LOAD:
        width += sprintf(tacString + width, "%s!%zu = %s!%zu.%s", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.str, line->operands[1].ssaNumber, line->operands[2].name.str);
        break;

    case TT_FIELD_LEA:
        width += sprintf(tacString + width, "%s!%zu = &(%s!%zu.%s)", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.str, line->operands[1].ssaNumber, line->operands[2].name.str);
        break;

    case TT_FIELD_STORE:
        width += sprintf(tacString + width, "%s!%zu.%s = %s!%zu", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.str, line->operands[2].name.str, line->operands[2].ssaNumber);
        break;

    case TT_BEQ:
    case TT_BNE:
    case TT_BGEU:
    case TT_BLTU:
    case TT_BGTU:
    case TT_BLEU:
    case TT_BEQZ:
    case TT_BNEZ:
        width += sprintf(tacString + width, "%s %s!%zu, %s!%zu, basicblock %ld",
                         get_asm_op(line->operation),
                         line->operands[1].name.str,
                         line->operands[1].ssaNumber,
                         line->operands[2].name.str,
                         line->operands[2].ssaNumber,
                         line->operands[0].name.val);
        break;

    case TT_JMP:
        width += sprintf(tacString + width, "%s basicblock %ld", get_asm_op(line->operation), line->operands[0].name.val);
        break;

    case TT_ASSIGN:
        width += sprintf(tacString + width, "%s!%zu = %s!%zu", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.str, line->operands[1].ssaNumber);
        break;

    case TT_ARG_STORE:
        width += sprintf(tacString + width, "store argument %s!%zu", line->operands[0].name.str, line->operands[0].ssaNumber);
        break;

    case TT_FUNCTION_CALL:
        if (line->operands[0].name.str == NULL)
        {
            width += sprintf(tacString + width, "call %s", line->operands[1].name.str);
        }
        else
        {
            width += sprintf(tacString + width, "%s!%zu = call %s", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.str);
        }
        break;

    case TT_METHOD_CALL:
        if (line->operands[0].name.str == NULL)
        {
            width += sprintf(tacString + width, "call %s.%s", line->operands[2].type.nonArray.complexType.name, line->operands[1].name.str);
        }
        else
        {
            width += sprintf(tacString + width, "%s!%zu = call %s.%s", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[2].type.nonArray.complexType.name, line->operands[1].name.str);
        }
        break;

    case TT_ASSOCIATED_CALL:
        if (line->operands[0].name.str == NULL)
        {
            width += sprintf(tacString + width, "call %s::%s", line->operands[2].type.nonArray.complexType.name, line->operands[1].name.str);
        }
        else
        {
            width += sprintf(tacString + width, "%s!%zu = call %s::%s", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[2].type.nonArray.complexType.name, line->operands[1].name.str);
        }
        break;

    case TT_LABEL:
        width += sprintf(tacString + width, "~label %ld:", line->operands[0].name.val);
        break;

    case TT_RETURN:
        if (tac_get_type_of_operand(line, 0)->basicType != VT_NULL)
        {
            width += sprintf(tacString + width, "ret %s!%zu", line->operands[0].name.str, line->operands[0].ssaNumber);
        }
        else
        {
            width += sprintf(tacString + width, "ret");
        }
        break;

    case TT_DO:
        width += sprintf(tacString + width, "do");
        break;

    case TT_ENDDO:
        width += sprintf(tacString + width, "end do");
        break;

    case TT_PHI:
        width += sprintf(tacString + width, "%s!%zu = phi(%s!%zu, %s!%zu)", line->operands[0].name.str, line->operands[0].ssaNumber, line->operands[1].name.str, line->operands[1].ssaNumber, line->operands[2].name.str, line->operands[2].ssaNumber);
        break;
    }

    const u32 SPRINT_TAC_LINE_ALIGN_WIDTH = 40;
    while (width < SPRINT_TAC_LINE_ALIGN_WIDTH)
    {
        width += sprintf(tacString + width, " ");
    }

    char *operandString = s_print_tac_operands(line);
    if (width + strlen(operandString) + 1 > SPRINT_TAC_LINE_LENGTH)
    {
        InternalError("sPrinTTACLINE length limit exceeded!");
    }
    width += sprintf(tacString + width, "\t%s", operandString);
    free(operandString);

    char *trimmedString = malloc(width + 1);
    sprintf(trimmedString, "%s", tacString);
    free(tacString);
    return trimmedString;
}

void free_tac(struct TACLine *line)
{
    free(line);
}

enum TAC_OPERAND_USE get_use_of_operand(struct TACLine *line, u8 operandIndex) // NOLINT (forgive me)
{
    enum TAC_OPERAND_USE use = U_UNUSED;
    switch (line->operation)
    {
    case TT_DO:
    case TT_ENDDO:
    case TT_ASM:
        break;

    case TT_FUNCTION_CALL:
    case TT_METHOD_CALL:
    case TT_ASSOCIATED_CALL:
        if ((operandIndex == 0) && (tac_get_type_of_operand(line, 0)->basicType != VT_NULL))
        {
            use = U_WRITE;
        }
        break;

    case TT_ASSIGN:
    {
        if (operandIndex == 0)
        {
            use = U_WRITE;
        }
        else if (operandIndex == 1)
        {
            use = U_READ;
        }
    }
    break;

    // single operand in slot 0
    case TT_RETURN:
    case TT_ARG_STORE:
        if (operandIndex == 0)
        {
            use = U_READ;
        }
        break;

    case TT_ADD:
    case TT_SUBTRACT:
    case TT_MUL:
    case TT_DIV:
    case TT_MODULO:
    case TT_BITWISE_AND:
    case TT_BITWISE_OR:
    case TT_BITWISE_XOR:
    case TT_BITWISE_NOT:
    case TT_LSHIFT:
    case TT_RSHIFT:
        if (operandIndex == 0)
        {
            use = U_WRITE;
        }
        else if ((operandIndex == 1) || (operandIndex == 2))
        {
            use = U_READ;
        }
        break;

    // loading writes the destination, while reading from the pointer
    case TT_LOAD:
    case TT_LOAD_OFF: // load_off uses a literal for operands[2]
        if (operandIndex == 0)
        {
            use = U_WRITE;
        }
        else if (operandIndex == 1)
        {
            use = U_READ;
        }
        break;

    case TT_LOAD_ARR:
        if (operandIndex == 0)
        {
            use = U_WRITE;
        }
        else if ((operandIndex == 1) || (operandIndex == 2))
        {
            use = U_READ;
        }
        break;

    // storing actually reads the variable containing the pionter to the location which the data is wriTTEN
    case TT_STORE:
        if ((operandIndex == 0) || (operandIndex == 1))
        {
            use = U_READ;
        }
        break;

    case TT_STORE_OFF:
        if ((operandIndex == 0) || (operandIndex == 2))
        {
            use = U_READ;
        }
        break;

    case TT_STORE_ARR:
        if ((operandIndex == 0) || (operandIndex == 1) || (operandIndex == 3))
        {
            use = U_READ;
        }
        break;

    case TT_ADDROF:
        if ((operandIndex == 0) || (operandIndex == 1))
        {
            use = U_WRITE;
        }
        break;

    case TT_LEA_OFF:
        if (operandIndex == 0)
        {
            use = U_WRITE;
        }
        else if (operandIndex == 1)
        {
            use = U_READ;
        }
        break;

    case TT_LEA_ARR:
        if (operandIndex == 0)
        {
            use = U_WRITE;
        }
        else if ((operandIndex == 1) || (operandIndex == 2))
        {
            use = U_READ;
        }
        break;

    case TT_FIELD_LOAD:
        if (operandIndex == 0)
        {
            use = U_WRITE;
        }
        else if (operandIndex == 1)
        {
            use = U_READ;
        }
        break;

    case TT_FIELD_LEA:
        if (operandIndex == 0)
        {
            use = U_WRITE;
        }
        else if (operandIndex == 1)
        {
            use = U_READ;
        }
        break;

    case TT_FIELD_STORE:
        if (operandIndex == 0)
        {
            use = U_WRITE;
        }
        else if (operandIndex == 1)
        {
            use = U_READ;
        }
        break;

    case TT_BEQ:
    case TT_BNE:
    case TT_BGEU:
    case TT_BLTU:
    case TT_BGTU:
    case TT_BLEU:
    case TT_BEQZ:
    case TT_BNEZ:
    {
        if ((operandIndex == 1) || (operandIndex == 2))
        {
            use = U_READ;
        }
    }
    break;

    case TT_PHI:
    {
        if (operandIndex == 0)
        {
            use = U_WRITE;
        }
        else if ((operandIndex == 1) || (operandIndex == 2))
        {
            use = U_READ;
        }
    }
    break;

    case TT_JMP:
    case TT_LABEL:
        break;
    }

    return use;
}