#include "tac.h"

#include "util.h"
#include <stdio.h>

struct Type *TACOperand_GetType(struct TACOperand *operand)
{
    if (operand->castAsType.basicType != vt_null)
    {
        return &operand->castAsType;
    }

    return &operand->type;
}

struct Type *TAC_GetTypeOfOperand(struct TACLine *line, unsigned index)
{
    if (index > 3)
    {
        ErrorAndExit(ERROR_INTERNAL, "Bad index %d passed to TAC_GetTypeOfOperand!\n", index);
    }

    return TACOperand_GetType(&line->operands[index]);
}

void TACOperand_SetBasicType(struct TACOperand *operand, enum basicTypes type, int indirectionLevel)
{
    operand->type.basicType = type;
    operand->type.indirectionLevel = indirectionLevel;
}

char *getAsmOp(enum TACType tacOperation)
{
    switch (tacOperation)
    {
    case tt_asm:
        return "tac-asm";
    case tt_assign:
        return "tac-assign";
    case tt_add:
        return "add";
    case tt_subtract:
        return "sub";
    case tt_mul:
        return "mul";
    case tt_div:
        return "div";
    case tt_modulo:
        return "rem";
    case tt_bitwise_and:
        return "and";
    case tt_bitwise_or:
        return "or";
    case tt_bitwise_xor:
        return "xor";
    case tt_bitwise_not:
        return "~";
    case tt_lshift:
        return "sll";
    case tt_rshift:
        return "srl";
    case tt_load:
        return "load";
    case tt_load_off:
        return "load (literal offset)";
    case tt_load_arr:
        return "load (array indexed)";
    case tt_store:
        return "store";
    case tt_store_off:
        return "store (literal offset)";
    case tt_store_arr:
        return "store (array indexed)";
    case tt_addrof:
        return "address-of";
    case tt_lea_off:
        return "lea (literal offset)";
    case tt_lea_arr:
        return "lea (array indexed)";
    case tt_call:
        return "call";
    case tt_label:
        return ".";
    case tt_return:
        return "ret";
    case tt_do:
        return "do";
    case tt_enddo:
        return "end do";
    case tt_beq:
        return "beq";
    case tt_bne:
        return "bne";
    case tt_bgeu:
        return "bgeu";
    case tt_bltu:
        return "bltu";
    case tt_bgtu:
        return "bgtu";
    case tt_bleu:
        return "bleu";
    case tt_beqz:
        return "beqz";
    case tt_bnez:
        return "bnez";
    case tt_jmp:
        return "jmp";
    case tt_stack_reserve:
        return "stack reserve";
    case tt_stack_store:
        return "stack store";
    }
    return "";
}

struct TACLine *newTACLineFunction(int index, enum TACType operation, struct AST *correspondingTree, char *file, int line)
{
    struct TACLine *wip = malloc(sizeof(struct TACLine));
    wip->allocFile = file;
    wip->allocLine = line;
    for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
    {
        wip->operands[operandIndex].name.str = NULL;
        wip->operands[operandIndex].permutation = vp_standard;

        wip->operands[operandIndex].type.basicType = vt_null;
        wip->operands[operandIndex].type.classType.name = NULL;
        wip->operands[operandIndex].type.indirectionLevel = 0;
        wip->operands[operandIndex].type.arraySize = 0;
        wip->operands[operandIndex].type.initializeArrayTo = NULL;

        wip->operands[operandIndex].castAsType.basicType = vt_null;
        wip->operands[operandIndex].castAsType.classType.name = NULL;
        wip->operands[operandIndex].castAsType.indirectionLevel = 0;
        wip->operands[operandIndex].castAsType.arraySize = 0;
        wip->operands[operandIndex].castAsType.initializeArrayTo = NULL;
    }
    wip->correspondingTree = *correspondingTree;

    // default type of a line of TAC is assignment
    wip->operation = operation;
    // by default operands are NOT reorderable
    wip->reorderable = 0;
    wip->index = index;
    wip->asmIndex = 0;
    return wip;
}

void printTACLine(struct TACLine *line)
{
    char *printedLine = sPrintTACLine(line);
    printf("%s", printedLine);
    free(printedLine);
}

char *sPrintTACOperands(struct TACLine *line)
{
    const u32 sprintTacOperandLength = 128;
    ssize_t width = 0;

    char *operandString = malloc(sprintTacOperandLength * sizeof(char));

    for (u8 operandIndex = 0; operandIndex < 4; operandIndex++)
    {
        if (line->operands[operandIndex].type.basicType != vt_null)
        {
            width += sprintf(operandString + width, "[");
            switch (line->operands[operandIndex].permutation)
            {
            case vp_standard:
                width += sprintf(operandString + width, " ");
                break;

            case vp_temp:
                width += sprintf(operandString + width, "T");
                break;

            case vp_literal:
                width += sprintf(operandString + width, "L");
                break;

            }

            char *typeName = Type_GetName(&line->operands[operandIndex].type);
            width += sprintf(operandString + width, " %s", typeName);
            free(typeName);
            if (line->operands[operandIndex].castAsType.basicType != vt_null)
            {
                char *castAsTypeName = Type_GetName(&line->operands[operandIndex].castAsType);
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

char *sPrintTACLine(struct TACLine *line)
{
    const u32 sprintTacLineLength = 128;
    char *operationStr;
    char *tacString = malloc(sprintTacLineLength * sizeof(char));
    char fallingThrough = 0;
    ssize_t width = sprintf(tacString, "%2lx:", line->index);
    switch (line->operation)
    {
    case tt_asm:
        width += sprintf(tacString + width, "ASM:%s", line->operands[0].name.str);
        break;

    case tt_add:
        if (!fallingThrough)
        {
            operationStr = "+";
            fallingThrough = 1;
        }
    case tt_subtract:
        if (!fallingThrough)
        {
            operationStr = "-";
            fallingThrough = 1;
        }
    case tt_mul:
        if (!fallingThrough)
        {
            operationStr = "*";
            fallingThrough = 1;
        }
    case tt_div:
        if (!fallingThrough)
        {
            operationStr = "/";
            fallingThrough = 1;
        }
    case tt_modulo:
        if (!fallingThrough)
        {
            operationStr = "%";
            fallingThrough = 1;
        }
    case tt_bitwise_and:
        if (!fallingThrough)
        {
            operationStr = "&";
            fallingThrough = 1;
        }
    case tt_bitwise_or:
        if (!fallingThrough)
        {
            operationStr = "|";
            fallingThrough = 1;
        }
    case tt_bitwise_xor:
        if (!fallingThrough)
        {
            operationStr = "^";
            fallingThrough = 1;
        }
    case tt_lshift:
        if (!fallingThrough)
        {
            operationStr = "<<";
            fallingThrough = 1;
        }
    case tt_rshift:
        if (!fallingThrough)
        {
            operationStr = ">>";
        }

        width += sprintf(tacString + width, "%s = %s %s %s", line->operands[0].name.str, line->operands[1].name.str, operationStr, line->operands[2].name.str);
        break;

    case tt_bitwise_not:
        width += sprintf(tacString + width, "%s = ~%s", line->operands[0].name.str, line->operands[1].name.str);
        break;

    case tt_load:
        width += sprintf(tacString + width, "%s = *%s", line->operands[0].name.str, line->operands[1].name.str);
        break;

    case tt_load_off:
        // operands: dest base offset
        width += sprintf(tacString + width, "%s = (%s + %ld)", line->operands[0].name.str, line->operands[1].name.str, line->operands[2].name.val);
        break;

    case tt_load_arr:
        // operands: dest base offset scale
        width += sprintf(tacString + width, "%s = (%s + %s*2^%ld)", line->operands[0].name.str, line->operands[1].name.str, line->operands[2].name.str, line->operands[3].name.val);
        break;

    case tt_store:
        width += sprintf(tacString + width, "*%s = %s", line->operands[0].name.str, line->operands[1].name.str);
        break;

    case tt_store_off:
        // operands: base offset source
        width += sprintf(tacString + width, "(%s + %ld) = %s", line->operands[0].name.str, line->operands[1].name.val, line->operands[2].name.str);
        break;

    case tt_store_arr:
        // operands base offset scale source
        width += sprintf(tacString + width, "(%s + %s*2^%ld) = %s", line->operands[0].name.str, line->operands[1].name.str, line->operands[2].name.val, line->operands[3].name.str);
        break;

    case tt_addrof:
        width += sprintf(tacString + width, "%s = &%s", line->operands[0].name.str, line->operands[1].name.str);
        break;

    case tt_lea_off:
        // operands: dest base offset scale
        width += sprintf(tacString + width, "%s = &(%s + %ld)", line->operands[0].name.str, line->operands[1].name.str, line->operands[2].name.val);
        break;

    case tt_lea_arr:
        // operands: dest base offset scale
        width += sprintf(tacString + width, "%s = &(%s + %s*2^%ld)", line->operands[0].name.str, line->operands[1].name.str, line->operands[2].name.str, line->operands[3].name.val);
        break;

    case tt_beq:
    case tt_bne:
    case tt_bgeu:
    case tt_bltu:
    case tt_bgtu:
    case tt_bleu:
    case tt_beqz:
    case tt_bnez:
        width += sprintf(tacString + width, "%s %s, %s, basicblock %ld",
                         getAsmOp(line->operation),
                         line->operands[1].name.str,
                         line->operands[2].name.str,
                         line->operands[0].name.val);
        break;

    case tt_jmp:
        width += sprintf(tacString + width, "%s basicblock %ld", getAsmOp(line->operation), line->operands[0].name.val);
        break;

    case tt_assign:
        width += sprintf(tacString + width, "%s = %s", line->operands[0].name.str, line->operands[1].name.str);
        break;

    case tt_stack_reserve:
        width += sprintf(tacString + width, "reserve %ld bytes stack", line->operands[0].name.val);
        break;

    case tt_stack_store:
        width += sprintf(tacString + width, "store %s at stack offset %ld", line->operands[0].name.str, line->operands[1].name.val);
        break;

    case tt_call:
        if (line->operands[0].name.str == NULL)
        {
            width += sprintf(tacString + width, "call %s", line->operands[1].name.str);
        }
        else
        {
            width += sprintf(tacString + width, "%s = call %s", line->operands[0].name.str, line->operands[1].name.str);
        }

        break;

    case tt_label:
        width += sprintf(tacString + width, "~label %ld:", line->operands[0].name.val);
        break;

    case tt_return:
        width += sprintf(tacString + width, "ret %s", line->operands[0].name.str);
        break;

    case tt_do:
        width += sprintf(tacString + width, "do");
        break;

    case tt_enddo:
        width += sprintf(tacString + width, "end do");
        break;
    }

    const u32 sprintTacLineAlignWidth = 40;
    while (width < sprintTacLineAlignWidth)
    {
        width += sprintf(tacString + width, " ");
    }

    char *operandString = sPrintTACOperands(line);
    if(width + strlen(operandString) + 1 > sprintTacLineLength)
    {
        ErrorAndExit(ERROR_INTERNAL, "sPrintTacLine length limit exceeded!\n");
    }
    width += sprintf(tacString + width, "\t%s", operandString);
    

    char *trimmedString = malloc(width + 1);
    sprintf(trimmedString, "%s", tacString);
    free(tacString);
    return trimmedString;
}

void freeTAC(struct TACLine *line)
{
    free(line);
}
