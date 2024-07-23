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

char *tac_operation_get_name(enum TAC_TYPE tacOperation)
{
    switch (tacOperation)
    {
    case TT_ASM:
        return "tac-asm";
    case TT_ASM_LOAD:
        return "tac-asm-load";
    case TT_ASM_STORE:
        return "tac-asm-store";
    case TT_ASSIGN:
        return "tac-assign";
    case TT_ADD:
        return "+";
    case TT_SUBTRACT:
        return "-";
    case TT_MUL:
        return "*";
    case TT_DIV:
        return "/";
    case TT_MODULO:
        return "%";
    case TT_BITWISE_AND:
        return "&";
    case TT_BITWISE_OR:
        return "|";
    case TT_BITWISE_XOR:
        return "^";
    case TT_BITWISE_NOT:
        return "~";
    case TT_LSHIFT:
        return "<<";
    case TT_RSHIFT:
        return ">>";
    case TT_LOAD:
        return "load";
    case TT_STORE:
        return "store";
    case TT_ADDROF:
        return "address-of";
    case TT_ARRAY_LOAD:
        return "array load";
    case TT_ARRAY_LEA:
        return "array load pointer";
    case TT_ARRAY_STORE:
        return "array store";
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
    for (u8 operandIndex = 0; operandIndex < N_TAC_OPERANDS_IN_LINE; operandIndex++)
    {
        wip->operands[operandIndex].name.str = NULL;
        wip->operands[operandIndex].ssaNumber = 0;
        wip->operands[operandIndex].permutation = VP_UNUSED;

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

const u8 TAC_OPERAND_TYPE_MIN_WIDTH = 10;
char *sprint_tac_operand_types(struct TACLine *line)
{
    const u32 SPRINT_TAC_OPERAND_LENGTH = 128;
    ssize_t width = 0;

    char *operandString = malloc(SPRINT_TAC_OPERAND_LENGTH * sizeof(char));

    for (u8 operandIndex = 0; operandIndex < N_TAC_OPERANDS_IN_LINE; operandIndex++)
    {
        if (line->operands[operandIndex].permutation != VP_UNUSED)
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

            case VP_LITERAL_STR:
            case VP_LITERAL_VAL:
                width += sprintf(operandString + width, "L");
                break;

            case VP_UNUSED:
                break;
            }

            char *typeName = type_get_name(tac_operand_get_non_cast_type(&line->operands[operandIndex]));
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
        while ((width / (operandIndex + 1)) < TAC_OPERAND_TYPE_MIN_WIDTH)
        {
            width += sprintf(operandString + width, " ");
        }
    }

    return operandString;
}

char *sprint_tac_line(struct TACLine *line)
{
    const u32 SPRINT_TAC_LINE_LENGTH = 256;
    char *tacString = malloc(SPRINT_TAC_LINE_LENGTH * sizeof(char));
    ssize_t width = sprintf(tacString, "%2lx:", line->index);

    char *operand0Str = tac_operand_sprint(&line->operands[0]);
    char *operand1Str = tac_operand_sprint(&line->operands[1]);
    char *operand2Str = tac_operand_sprint(&line->operands[2]);

    switch (line->operation)
    {
    case TT_ASM:
        width += sprintf(tacString + width, "ASM:%s", line->operands[0].name.str);
        break;

    case TT_ASM_LOAD:
        width += sprintf(tacString + width, "ASM:%s = %s", line->operands[0].name.str, operand1Str);
        break;

    case TT_ASM_STORE:
        width += sprintf(tacString + width, "ASM:%s = %s", operand0Str, line->operands[1].name.str);
        break;

    case TT_ADD:
    case TT_SUBTRACT:
    case TT_MUL:
    case TT_DIV:
    case TT_MODULO:
    case TT_BITWISE_AND:
    case TT_BITWISE_OR:
    case TT_BITWISE_XOR:
    case TT_LSHIFT:
    case TT_RSHIFT:
    case TT_BITWISE_NOT:
        width += sprintf(tacString + width, "%s = %s %s %s", operand0Str, operand1Str, tac_operation_get_name(line->operation), operand2Str);
        break;

    case TT_LOAD:
        width += sprintf(tacString + width, "%s = *%s", operand0Str, operand1Str);
        break;

    case TT_STORE:
        width += sprintf(tacString + width, "*%s = %s", operand0Str, operand1Str);
        break;

    case TT_ADDROF:
        width += sprintf(tacString + width, "%s = &%s", operand0Str, operand1Str);
        break;

    case TT_ARRAY_LOAD:
        // operands: dest base index
        width += sprintf(tacString + width, "%s = %s[%s]", operand0Str, operand1Str, operand2Str);
        break;

    case TT_ARRAY_LEA:
        // operands: dest base index
        width += sprintf(tacString + width, "%s = &%s[%s]", operand0Str, operand1Str, operand2Str);
        break;

    case TT_ARRAY_STORE:
        // operands: dest index source
        width += sprintf(tacString + width, "%s[%s] = %s", operand0Str, operand1Str, operand2Str);
        break;

    case TT_FIELD_LOAD:
        width += sprintf(tacString + width, "%s = %s.%s", operand0Str, operand1Str, line->operands[2].name.str);
        break;

    case TT_FIELD_LEA:
        width += sprintf(tacString + width, "%s = &(%s.%s)", operand0Str, operand1Str, line->operands[2].name.str);
        break;

    case TT_FIELD_STORE:
        width += sprintf(tacString + width, "%s.%s = %s", operand0Str, line->operands[1].name.str, operand2Str);
        break;

    case TT_BEQ:
    case TT_BNE:
    case TT_BGEU:
    case TT_BLTU:
    case TT_BGTU:
    case TT_BLEU:
    case TT_BEQZ:
    case TT_BNEZ:
        width += sprintf(tacString + width, "%s %s, %s, basicblock %ld",
                         tac_operation_get_name(line->operation),
                         operand1Str,
                         operand2Str,
                         line->operands[0].name.val);
        break;

    case TT_JMP:
        width += sprintf(tacString + width, "%s basicblock %ld", tac_operation_get_name(line->operation), line->operands[0].name.val);
        break;

    case TT_ASSIGN:
        width += sprintf(tacString + width, "%s = %s", operand0Str, operand1Str);
        break;

    case TT_ARG_STORE:
        width += sprintf(tacString + width, "store argument %s", operand0Str);
        break;

    case TT_FUNCTION_CALL:
        if (line->operands[0].name.str == NULL)
        {
            width += sprintf(tacString + width, "call %s", line->operands[1].name.str);
        }
        else
        {
            width += sprintf(tacString + width, "%s = call %s", operand0Str, line->operands[1].name.str);
        }
        break;

    case TT_METHOD_CALL:
        if (line->operands[0].name.str == NULL)
        {
            width += sprintf(tacString + width, "call %s.%s", line->operands[2].castAsType.nonArray.complexType.name, line->operands[1].name.str);
        }
        else
        {
            width += sprintf(tacString + width, "%s = call %s.%s", operand0Str, line->operands[2].castAsType.nonArray.complexType.name, line->operands[1].name.str);
        }
        break;

    case TT_ASSOCIATED_CALL:
        if (line->operands[0].name.str == NULL)
        {
            width += sprintf(tacString + width, "call %s::%s", line->operands[2].castAsType.nonArray.complexType.name, line->operands[1].name.str);
        }
        else
        {
            width += sprintf(tacString + width, "%s = call %s::%s", operand0Str, line->operands[2].castAsType.nonArray.complexType.name, line->operands[1].name.str);
        }
        break;

    case TT_LABEL:
        width += sprintf(tacString + width, "~label %ld:", line->operands[0].name.val);
        break;

    case TT_RETURN:
        if (tac_get_type_of_operand(line, 0)->basicType != VT_NULL)
        {
            width += sprintf(tacString + width, "ret %s", operand0Str);
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
        width += sprintf(tacString + width, "%s = phi(%s, %s)", operand0Str, operand1Str, operand2Str);
        break;
    }

    free(operand0Str);
    free(operand1Str);
    free(operand2Str);

    const u32 SPRINT_TAC_LINE_ALIGN_WIDTH = 40;
    while (width < SPRINT_TAC_LINE_ALIGN_WIDTH)
    {
        width += sprintf(tacString + width, " ");
    }

    char *operandString = sprint_tac_operand_types(line);
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

    case TT_ASM_LOAD:
        if (operandIndex == 1)
        {
            use = U_READ;
        }
        break;

    case TT_ASM_STORE:
        if (operandIndex == 0)
        {
            use = U_WRITE;
        }
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
        if (operandIndex == 0)
        {
            use = U_WRITE;
        }
        else if (operandIndex == 1)
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

    case TT_ADDROF:
        if ((operandIndex == 0) || (operandIndex == 1))
        {
            use = U_WRITE;
        }
        break;

    case TT_ARRAY_LOAD:
    case TT_ARRAY_LEA:
    case TT_ARRAY_STORE:
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
        else if (operandIndex == 2)
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