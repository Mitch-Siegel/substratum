#include "tac.h"

#include "log.h"
#include "util.h"
#include <stdio.h>

#include "symtab_basicblock.h"

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
    case TT_PHI:
        return "phi";
    }
    return "";
}

struct TACLine *new_tac_line_function(enum TAC_TYPE operation, struct Ast *correspondingTree, char *file, int line)
{
    struct TACLine *wip = malloc(sizeof(struct TACLine));
    memset(wip, 0, sizeof(struct TACLine));
    wip->allocFile = file;
    wip->allocLine = line;
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
char *sprint_tac_operand_type(struct TACOperand *operand)
{
    const u32 SPRINT_TAC_OPERAND_LENGTH = 128;
    ssize_t width = 0;

    char *operandString = malloc(SPRINT_TAC_OPERAND_LENGTH * sizeof(char));
    operandString[0] = '\0';

    if (operand->permutation != VP_UNUSED)
    {
        width += sprintf(operandString + width, "[");
        switch (operand->permutation)
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

        char *typeName = type_get_name(tac_operand_get_non_cast_type(operand));
        width += sprintf(operandString + width, " %s", typeName);
        free(typeName);
        if (operand->castAsType.basicType != VT_NULL)
        {
            char *castAsTypeName = type_get_name(&operand->castAsType);
            width += sprintf(operandString + width, "(%s)", castAsTypeName);
            free(castAsTypeName);
        }
        width += sprintf(operandString + width, "]");
    }
    else
    {
        width += sprintf(operandString + width, "   -   ");
    }

    return operandString;
}

ssize_t sprint_function_arguments(char *destStr, ssize_t width, Deque *arguments)
{
    width += sprintf(destStr + width, "(");
    Iterator *argIterator = deque_front(arguments);
    while (iterator_gettable(argIterator))
    {
        struct TACOperand *argOperand = iterator_get(argIterator);
        iterator_next(argIterator);
        char *argStr = tac_operand_sprint(argOperand);
        width += sprintf(destStr + width, "%s%s", argStr, iterator_gettable(argIterator) ? ", " : "");
        free(argStr);
    }
    iterator_free(argIterator);
    width += sprintf(destStr + width, ")");

    return width;
}

char *sprint_tac_line(struct TACLine *line)
{
    const u32 SPRINT_TAC_LINE_LENGTH = 256;
    char *tacString = malloc(SPRINT_TAC_LINE_LENGTH * sizeof(char));
    ssize_t width = sprintf(tacString, "%2lx:", line->index);

    // char *operand0Str = tac_operand_sprint(&line->operands[0]);
    // char *operand1Str = tac_operand_sprint(&line->operands[1]);
    // char *operand2Str = tac_operand_sprint(&line->operands[2]);

    switch (line->operation)
    {
    case TT_ASM:
    {
        width += sprintf(tacString + width, "ASM:%s", line->operands.asm_.asmString);
    }
    break;

    case TT_ASM_LOAD:
    {
        char *sourceStr = tac_operand_sprint(&line->operands.asmLoad.sourceOperand);
        width += sprintf(tacString + width, "ASM:%s = %s", line->operands.asmLoad.destRegisterName, sourceStr);
        free(sourceStr);
    }
    break;

    case TT_ASM_STORE:
    {
        char *destStr = tac_operand_sprint(&line->operands.asmStore.destinationOperand);
        width += sprintf(tacString + width, "ASM:%s = %s", destStr, line->operands.asmStore.sourceRegisterName);
        free(destStr);
    }
    break;

    case TT_ASSIGN:
    {
        char *sourceStr = tac_operand_sprint(&line->operands.assign.source);
        char *destStr = tac_operand_sprint(&line->operands.assign.destination);
        width += sprintf(tacString + width, "%s = %s", destStr, sourceStr);
        free(sourceStr);
        free(destStr);
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
    case TT_LSHIFT:
    case TT_RSHIFT:
    case TT_BITWISE_NOT:
    {
        char *destStr = tac_operand_sprint(&line->operands.arithmetic.destination);
        char *sourceAStr = tac_operand_sprint(&line->operands.arithmetic.sourceA);
        char *sourceBStr = tac_operand_sprint(&line->operands.arithmetic.sourceB);
        width += sprintf(tacString + width, "%s = %s %s %s", destStr, sourceAStr, tac_operation_get_name(line->operation), sourceBStr);
        free(destStr);
        free(sourceAStr);
        free(sourceBStr);
    }
    break;

    case TT_LOAD:
    {
        char *addressStr = tac_operand_sprint(&line->operands.load.address);
        char *destinationStr = tac_operand_sprint(&line->operands.load.destination);
        width += sprintf(tacString + width, "%s = *%s", destinationStr, addressStr);
        free(addressStr);
        free(destinationStr);
    }
    break;

    case TT_STORE:
    {
        char *addressStr = tac_operand_sprint(&line->operands.store.address);
        char *sourceStr = tac_operand_sprint(&line->operands.store.source);
        width += sprintf(tacString + width, "*%s = %s", addressStr, sourceStr);
        free(addressStr);
        free(sourceStr);
    }
    break;

    case TT_ADDROF:
    {
        char *sourceStr = tac_operand_sprint(&line->operands.addrof.destination);
        char *destinationStr = tac_operand_sprint(&line->operands.addrof.destination);
        width += sprintf(tacString + width, "%s = &%s", destinationStr, sourceStr);
        free(sourceStr);
        free(destinationStr);
    }
    break;

    case TT_ARRAY_LOAD:
    case TT_ARRAY_LEA:
    {
        char *destinationStr = tac_operand_sprint(&line->operands.arrayLoad.destination);
        char *arrayStr = tac_operand_sprint(&line->operands.arrayLoad.array);
        char *indexStr = tac_operand_sprint(&line->operands.arrayLoad.index);
        char *addrOfStr = "";
        if (line->operation == TT_ARRAY_LEA)
        {
            addrOfStr = "&";
        }
        width += sprintf(tacString + width, "%s = %s%s[%s]", destinationStr, addrOfStr, arrayStr, indexStr);
        free(destinationStr);
        free(arrayStr);
        free(indexStr);
    }
    break;

    case TT_ARRAY_STORE:
    {
        char *arrayStr = tac_operand_sprint(&line->operands.arrayStore.array);
        char *indexStr = tac_operand_sprint(&line->operands.arrayStore.index);
        char *sourceStr = tac_operand_sprint(&line->operands.arrayStore.source);
        width += sprintf(tacString + width, "%s[%s] = %s", arrayStr, indexStr, sourceStr);
        free(arrayStr);
        free(indexStr);
        free(sourceStr);
    }
    break;

    case TT_FIELD_LOAD:
    case TT_FIELD_LEA:
    {
        char *destinationStr = tac_operand_sprint(&line->operands.fieldLoad.destination);
        char *sourceStr = tac_operand_sprint(&line->operands.fieldLoad.source);
        char *addrOfStr = "";
        if (line->operation == TT_FIELD_LEA)
        {
            addrOfStr = "&";
        }
        width += sprintf(tacString + width, "%s = %s%s.%s", destinationStr, addrOfStr, sourceStr, line->operands.fieldLoad.fieldName);
        free(destinationStr);
        free(sourceStr);
    }
    break;

    case TT_FIELD_STORE:
    {
        char *destinationStr = tac_operand_sprint(&line->operands.fieldStore.destination);
        char *sourceStr = tac_operand_sprint(&line->operands.fieldStore.source);
        width += sprintf(tacString + width, "%s.%s = %s", destinationStr, line->operands.fieldLoad.fieldName, sourceStr);
        free(destinationStr);
        free(sourceStr);
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
        char *sourceAStr = tac_operand_sprint(&line->operands.conditionalBranch.sourceA);
        char *sourceBStr = tac_operand_sprint(&line->operands.conditionalBranch.sourceB);
        width += sprintf(tacString + width, "%s %s, %s, basicblock %ld",
                         tac_operation_get_name(line->operation),
                         sourceAStr,
                         sourceBStr,
                         line->operands.conditionalBranch.label);
        free(sourceAStr);
        free(sourceBStr);
    }
    break;

    case TT_JMP:
    {
        width += sprintf(tacString + width, "%s basicblock %ld", tac_operation_get_name(line->operation), line->operands.jump.label);
    }
    break;

    case TT_FUNCTION_CALL:
    {
        if (tac_operand_get_type(&line->operands.functionCall.returnValue)->basicType != VT_NULL)
        {
            char *returnValueStr = tac_operand_sprint(&line->operands.functionCall.returnValue);
            width += sprintf(tacString + width, "%s = ", returnValueStr);
            free(returnValueStr);
        }
        width += sprintf(tacString + width, "call %s", line->operands.functionCall.functionName);
        width = sprint_function_arguments(tacString, width, line->operands.functionCall.arguments);
    }
    break;

    case TT_METHOD_CALL:
    {
        if (tac_operand_get_type(&line->operands.methodCall.returnValue)->basicType != VT_NULL)
        {
            char *returnValueStr = tac_operand_sprint(&line->operands.methodCall.returnValue);
            width += sprintf(tacString + width, "%s = ", returnValueStr);
            free(returnValueStr);
        }
        char *calledOnStr = tac_operand_sprint(&line->operands.methodCall.calledOn);
        width += sprintf(tacString + width, "call %s.%s", calledOnStr, line->operands.methodCall.methodName);
        free(calledOnStr);
        width = sprint_function_arguments(tacString, width, line->operands.methodCall.arguments);
    }
    break;

    case TT_ASSOCIATED_CALL:
    {
        if (tac_operand_get_type(&line->operands.associatedCall.returnValue)->basicType != VT_NULL)
        {
            char *returnValueStr = tac_operand_sprint(&line->operands.associatedCall.returnValue);
            width += sprintf(tacString + width, "%s = ", returnValueStr);
            free(returnValueStr);
        }
        char *structName = type_get_name(&line->operands.associatedCall.associatedWith);
        width += sprintf(tacString + width, "call %s::%s", structName, line->operands.associatedCall.functionName);
        free(structName);
        width = sprint_function_arguments(tacString, width, line->operands.associatedCall.arguments);
    }
    break;

    // TODO: remove?
    case TT_LABEL:
    {
        width += sprintf(tacString + width, "~label %ld:", line->operands.label.labelNumber);
    }
    break;

    case TT_RETURN:
    {
        width += sprintf(tacString + width, "return");
        if (tac_operand_get_type(&line->operands.return_.returnValue)->basicType != VT_NULL)
        {
            char *returnValueStr = tac_operand_sprint(&line->operands.associatedCall.returnValue);
            width += sprintf(tacString + width, " %s", returnValueStr);
            free(returnValueStr);
        }
    }
    break;

    case TT_DO:
        width += sprintf(tacString + width, "do");
        break;

    case TT_ENDDO:
        width += sprintf(tacString + width, "end do");
        break;

    case TT_PHI:
    {
        char *destStr = tac_operand_sprint(&line->operands.phi.destination);
        width += sprintf(tacString + width, "%s = phi", destStr);
        sprint_function_arguments(tacString, width, line->operands.phi.sources);
    }
    break;
    }

    const u32 SPRINT_TAC_LINE_ALIGN_WIDTH = 40;
    while (width < SPRINT_TAC_LINE_ALIGN_WIDTH)
    {
        width += sprintf(tacString + width, " ");
    }

    // TODO: print operand types in switch cases
    // char *operandString = sprint_tac_operand_types(line);
    // if (width + strlen(operandString) + 1 > SPRINT_TAC_LINE_LENGTH)
    // {
    //     InternalError("sPrinTTACLINE length limit exceeded!");
    // }
    // width += sprintf(tacString + width, "\t%s", operandString);
    // free(operandString);

    char *trimmedString = malloc(width + 1);
    sprintf(trimmedString, "%s", tacString);
    free(tacString);
    return trimmedString;
}

bool tac_line_is_jump(struct TACLine *line)
{
    switch (line->operation)
    {
    case TT_BEQ:
    case TT_BNE:
    case TT_BGEU:
    case TT_BLTU:
    case TT_BGTU:
    case TT_BLEU:
    case TT_BEQZ:
    case TT_BNEZ:
    case TT_JMP:
    case TT_RETURN:
        return true;
    default:
        return false;
    }
}

ssize_t tac_get_jump_target(struct TACLine *line)
{
    ssize_t target = -1;
    switch (line->operation)
    {
    case TT_BEQ:
    case TT_BNE:
    case TT_BGEU:
    case TT_BLTU:
    case TT_BGTU:
    case TT_BLEU:
    case TT_BEQZ:
    case TT_BNEZ:
        target = line->operands.conditionalBranch.label;
        break;
    case TT_JMP:
        target = line->operands.jump.label;
        break;
    case TT_RETURN:
        target = FUNCTION_EXIT_BLOCK_LABEL;
        break;
    default:
        break;
    }

    return target;
}

void free_tac(struct TACLine *line)
{
    switch (line->operation)
    {
    case TT_PHI:
    {
        while (line->operands.phi.sources->size > 0)
        {
            struct TACOperand *source = deque_pop_front(line->operands.phi.sources);
            free(source);
        }
        deque_free(line->operands.phi.sources);
    }
    break;

    case TT_FUNCTION_CALL:
    {
        while (line->operands.functionCall.arguments->size > 0)
        {
            struct TACOperand *argument = deque_pop_front(line->operands.functionCall.arguments);
            free(argument);
        }
        deque_free(line->operands.functionCall.arguments);
    }
    break;

    case TT_METHOD_CALL:
    {
        while (line->operands.methodCall.arguments->size > 0)
        {
            struct TACOperand *argument = deque_pop_front(line->operands.methodCall.arguments);
            free(argument);
        }
        deque_free(line->operands.methodCall.arguments);
    }
    break;

    case TT_ASSOCIATED_CALL:
    {
        while (line->operands.associatedCall.arguments->size > 0)
        {
            struct TACOperand *argument = deque_pop_front(line->operands.associatedCall.arguments);
            free(argument);
        }
        deque_free(line->operands.associatedCall.arguments);
    }
    break;

    case TT_ASM:
    case TT_ASM_LOAD:
    case TT_ASM_STORE:
    case TT_ASSIGN:
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
    case TT_LOAD:
    case TT_STORE:
    case TT_ADDROF:
    case TT_ARRAY_LOAD:
    case TT_ARRAY_LEA:
    case TT_ARRAY_STORE:
    case TT_FIELD_LOAD:
    case TT_FIELD_LEA:
    case TT_FIELD_STORE:
    case TT_BEQ:
    case TT_BNE:
    case TT_BGEU:
    case TT_BLTU:
    case TT_BGTU:
    case TT_BLEU:
    case TT_BEQZ:
    case TT_BNEZ:
    case TT_JMP:
    case TT_LABEL:
    case TT_RETURN:
    case TT_DO:
    case TT_ENDDO:
        break;
    }

    free(line);
}

struct OperandUsages get_operand_usages(struct TACLine *line) // NOLINT (forgive me)
{
    struct OperandUsages usages = {0};
    usages.reads = deque_new(NULL);
    usages.writes = deque_new(NULL);

    switch (line->operation)
    {
    case TT_DO:
    case TT_ENDDO:
    case TT_ASM:
        break;

    case TT_ASM_LOAD:
        deque_push_back(usages.reads, &line->operands.asmLoad.sourceOperand);
        break;

    case TT_ASM_STORE:
        deque_push_back(usages.writes, &line->operands.asmStore.destinationOperand);
        break;

    case TT_FUNCTION_CALL:
        if ((tac_operand_get_type(&line->operands.functionCall.returnValue)->basicType != VT_NULL))
        {
            deque_push_back(usages.writes, &line->operands.functionCall.returnValue);
        }
        for (size_t argIdx = 0; argIdx < line->operands.functionCall.arguments->size; argIdx++)
        {
            struct TACOperand *argument = deque_at(line->operands.functionCall.arguments, argIdx);
            deque_push_back(usages.reads, argument);
        }
        break;

    case TT_METHOD_CALL:
        if ((tac_operand_get_type(&line->operands.methodCall.returnValue)->basicType != VT_NULL))
        {
            deque_push_back(usages.writes, &line->operands.methodCall.returnValue);
        }
        for (size_t argIdx = 0; argIdx < line->operands.methodCall.arguments->size; argIdx++)
        {
            struct TACOperand *argument = deque_at(line->operands.methodCall.arguments, argIdx);
            deque_push_back(usages.reads, argument);
        }
        deque_push_back(usages.reads, &line->operands.methodCall.calledOn);
        break;

    case TT_ASSOCIATED_CALL:
        if ((tac_operand_get_type(&line->operands.associatedCall.returnValue)->basicType != VT_NULL))
        {
            deque_push_back(usages.writes, &line->operands.associatedCall.returnValue);
        }
        for (size_t argIdx = 0; argIdx < line->operands.associatedCall.arguments->size; argIdx++)
        {
            struct TACOperand *argument = deque_at(line->operands.associatedCall.arguments, argIdx);
            deque_push_back(usages.reads, argument);
        }
        break;

    case TT_ASSIGN:
        deque_push_back(usages.writes, &line->operands.assign.destination);
        deque_push_back(usages.reads, &line->operands.assign.source);
        break;

    // single operand in slot 0
    case TT_RETURN:
        if ((tac_operand_get_type(&line->operands.return_.returnValue)->basicType != VT_NULL))
        {
            deque_push_back(usages.writes, &line->operands.return_.returnValue);
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
        deque_push_back(usages.writes, &line->operands.arithmetic.destination);
        deque_push_back(usages.writes, &line->operands.arithmetic.sourceA);
        deque_push_back(usages.writes, &line->operands.arithmetic.sourceB);
        break;

    // loading writes the destination, while reading from the pointer
    case TT_LOAD:
        deque_push_back(usages.writes, &line->operands.load.destination);
        deque_push_back(usages.reads, &line->operands.load.address);
        break;

    // storing actually reads the variable containing the pionter to the location which the data is written
    case TT_STORE:
        deque_push_back(usages.reads, &line->operands.store.address);
        deque_push_back(usages.reads, &line->operands.store.source);
        break;

    case TT_ADDROF:
        deque_push_back(usages.writes, &line->operands.addrof.destination);
        deque_push_back(usages.reads, &line->operands.addrof.source);
        break;

    case TT_ARRAY_LOAD:
    case TT_ARRAY_LEA:
        deque_push_back(usages.writes, &line->operands.arrayLoad.destination);
        deque_push_back(usages.reads, &line->operands.arrayLoad.array);
        deque_push_back(usages.reads, &line->operands.arrayLoad.index);
        break;
        break;

    case TT_ARRAY_STORE:
        deque_push_back(usages.reads, &line->operands.arrayStore.array);
        deque_push_back(usages.reads, &line->operands.arrayStore.index);
        deque_push_back(usages.reads, &line->operands.arrayStore.source);
        break;

    case TT_FIELD_LOAD:
    case TT_FIELD_LEA:
        deque_push_back(usages.writes, &line->operands.fieldLoad.destination);
        deque_push_back(usages.reads, &line->operands.fieldLoad.source);
        break;

    case TT_FIELD_STORE:
        deque_push_back(usages.writes, &line->operands.fieldLoad.destination);
        deque_push_back(usages.reads, &line->operands.fieldLoad.source);
        break;

    case TT_BEQ:
    case TT_BNE:
    case TT_BGEU:
    case TT_BLTU:
    case TT_BGTU:
    case TT_BLEU:
    case TT_BEQZ:
    case TT_BNEZ:
        deque_push_back(usages.reads, &line->operands.conditionalBranch.sourceA);
        deque_push_back(usages.reads, &line->operands.conditionalBranch.sourceB);
        break;

    case TT_PHI:
    {
        deque_push_back(usages.writes, &line->operands.phi.destination);
        Iterator *sourceIterator = NULL;
        for (sourceIterator = deque_front(line->operands.phi.sources); iterator_gettable(sourceIterator);)
        {
            struct TACOperand *source = iterator_get(sourceIterator);
            deque_push_back(usages.reads, source);
            iterator_next(sourceIterator);
        }
        iterator_free(sourceIterator);
    }
    break;

    case TT_JMP:
    case TT_LABEL:
        break;
    }

    return usages;
}