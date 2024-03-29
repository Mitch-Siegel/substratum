#include "tac.h"

struct Type *TACOperand_GetType(struct TACOperand *o)
{
	if (o->castAsType.basicType != vt_null)
	{
		return &o->castAsType;
	}

	return &o->type;
}

struct Type *TAC_GetTypeOfOperand(struct TACLine *t, unsigned index)
{
	if (index > 3)
	{
		ErrorAndExit(ERROR_INTERNAL, "Bad index %d passed to TAC_GetTypeOfOperand!\n", index);
	}

	return TACOperand_GetType(&t->operands[index]);
}

int Type_Compare(struct Type *a, struct Type *b)
{
	if (a->basicType != b->basicType)
	{
		return 1;
	}

	if (a->indirectionLevel != b->indirectionLevel)
	{
		return 2;
	}

	if (a->basicType == vt_class)
	{
		return strcmp(a->classType.name, b->classType.name);
	}

	return 0;
}

int Type_CompareAllowImplicitWidening(struct Type *a, struct Type *b)
{
	if (a->basicType != b->basicType)
	{
		switch (a->basicType)
		{
		case vt_null:
			return 1;

		case vt_any:
			switch (b->basicType)
			{
			case vt_null:
				return 1;
			case vt_class:
			case vt_any:
			case vt_u8:
			case vt_u16:
			case vt_u32:
				break;

			default:
				break;
			}
			break;

		case vt_u8:
			switch (b->basicType)
			{
			case vt_null:
			case vt_class:
				return 1;
			case vt_any:
			case vt_u8:
			case vt_u16:
			case vt_u32:
			case vt_u64:
				break;

			default:
				break;
			}
			break;

		case vt_u16:
			switch (b->basicType)
			{
			case vt_null:
			case vt_u8:
			case vt_class:
				return 1;
			case vt_any:
			case vt_u16:
			case vt_u32:
			case vt_u64:
				break;
			}
			break;

		case vt_u32:
			switch (b->basicType)
			{
			case vt_null:
			case vt_u8:
			case vt_u16:
			case vt_class:
				return 1;

			case vt_any:
			case vt_u32:
			case vt_u64:
				break;
			}
			break;

		case vt_u64:
			switch (b->basicType)
			{
			case vt_null:
			case vt_u8:
			case vt_u16:
			case vt_u32:
			case vt_class:
				return 1;

			case vt_any:
			case vt_u64:
				break;
			}
			break;

		case vt_class:
			switch (b->basicType)
			{
			case vt_null:
			case vt_u8:
			case vt_u16:
			case vt_u32:
			case vt_u64:
				return 1;

			case vt_any:
			case vt_class:
				break;
			}
		}
	}

	// allow implicit conversion from any type of pointer to 'any *' or 'any **', etc
	if ((a->indirectionLevel > 0) && (b->indirectionLevel > 0) && (b->basicType == vt_any))
	{
		return 0;
	}
	else if (a->indirectionLevel != b->indirectionLevel)
	{

		// both are arrays or both are not arrays
		if ((a->arraySize > 0) == (b->arraySize > 0))
		{
			return 2;
		}
		// only a is an array
		else if (a->arraySize > 0)
		{
			// b's indirection level should be a's + 1
			if (b->indirectionLevel != (a->indirectionLevel + 1))
			{
				return 2;
			}
		}
		else
		{
			// a's indirection level should be b's + 1
			if ((b->indirectionLevel + 1) != a->indirectionLevel)
			{
				return 2;
			}
		}
	}

	if (a->basicType == vt_class)
	{
		return strcmp(a->classType.name, b->classType.name);
	}
	return 0;
}

char *Type_GetName(struct Type *t)
{
	char *typeName = malloc(1024);
	int len = 0;
	switch (t->basicType)
	{
	case vt_null:
		len = sprintf(typeName, "NOTYPE");
		break;

	case vt_any:
		len = sprintf(typeName, "any");
		break;

	case vt_u8:
		len = sprintf(typeName, "u8");
		break;

	case vt_u16:
		len = sprintf(typeName, "u16");
		break;

	case vt_u32:
		len = sprintf(typeName, "u32");
		break;

	case vt_u64:
		len = sprintf(typeName, "u64");
		break;

	case vt_class:
		len = sprintf(typeName, "%s", t->classType.name);
		break;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Unexpected enum basicTypes value %d seen in Type_GetName!\n", t->basicType);
	}

	int i = 0;
	for (i = 0; i < t->indirectionLevel; i++)
	{
		typeName[len + i] = '*';
		len += sprintf(typeName + len, "*");
	}
	typeName[len + i] = '\0';

	if (t->arraySize > 0)
	{
		sprintf(typeName + len, "[%d]", t->arraySize);
	}

	return typeName;
}

void TACOperand_SetBasicType(struct TACOperand *o, enum basicTypes t, int indirectionLevel)
{
	o->type.basicType = t;
	o->type.indirectionLevel = indirectionLevel;
}

char *getAsmOp(enum TACType t)
{
	switch (t)
	{
	case tt_asm:
		return "";
	case tt_assign:
		return "";
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
	for (int i = 0; i < 4; i++)
	{
		wip->operands[i].name.str = NULL;
		wip->operands[i].permutation = vp_standard;

		wip->operands[i].type.basicType = vt_null;
		wip->operands[i].type.classType.name = NULL;
		wip->operands[i].type.indirectionLevel = 0;
		wip->operands[i].type.arraySize = 0;
		wip->operands[i].type.initializeArrayTo = NULL;

		wip->operands[i].castAsType.basicType = vt_null;
		wip->operands[i].castAsType.classType.name = NULL;
		wip->operands[i].castAsType.indirectionLevel = 0;
		wip->operands[i].castAsType.arraySize = 0;
		wip->operands[i].castAsType.initializeArrayTo = NULL;
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

void printTACLine(struct TACLine *it)
{
	char *printedLine = sPrintTACLine(it);
	printf("%s", printedLine);
	free(printedLine);
}

char *sPrintTACLine(struct TACLine *it)
{
	char *operationStr;
	char *tacString = malloc(128);
	char fallingThrough = 0;
	int width = sprintf(tacString, "%2x:", it->index);
	switch (it->operation)
	{
	case tt_asm:
		width += sprintf(tacString + width, "ASM:%s", it->operands[0].name.str);
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

		width += sprintf(tacString + width, "%s = %s %s %s", it->operands[0].name.str, it->operands[1].name.str, operationStr, it->operands[2].name.str);
		break;

	case tt_bitwise_not:
		width += sprintf(tacString + width, "%s = ~%s", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_load:
		width += sprintf(tacString + width, "%s = *%s", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_load_off:
		// operands: dest base offset
		width += sprintf(tacString + width, "%s = (%s + %d)", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.val);
		break;

	case tt_load_arr:
		// operands: dest base offset scale
		width += sprintf(tacString + width, "%s = (%s + %s*2^%d)", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.str, it->operands[3].name.val);
		break;

	case tt_store:
		width += sprintf(tacString + width, "*%s = %s", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_store_off:
		// operands: base offset source
		width += sprintf(tacString + width, "(%s + %d) = %s", it->operands[0].name.str, it->operands[1].name.val, it->operands[2].name.str);
		break;

	case tt_store_arr:
		// operands base offset scale source
		width += sprintf(tacString + width, "(%s + %s*2^%d) = %s", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.val, it->operands[3].name.str);
		break;

	case tt_addrof:
		width += sprintf(tacString + width, "%s = &%s", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_lea_off:
		// operands: dest base offset scale
		width += sprintf(tacString + width, "%s = &(%s + %d)", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.val);
		break;

	case tt_lea_arr:
		// operands: dest base offset scale
		width += sprintf(tacString + width, "%s = &(%s + %s*2^%d)", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.str, it->operands[3].name.val);
		break;

	case tt_beq:
	case tt_bne:
	case tt_bgeu:
	case tt_bltu:
	case tt_bgtu:
	case tt_bleu:
	case tt_beqz:
	case tt_bnez:
		width += sprintf(tacString + width, "%s %s, %s, basicblock %d",
						 getAsmOp(it->operation),
						 it->operands[1].name.str,
						 it->operands[2].name.str,
						 it->operands[0].name.val);
		break;

	case tt_jmp:
		width += sprintf(tacString + width, "%s basicblock %d", getAsmOp(it->operation), it->operands[0].name.val);
		break;

	case tt_assign:
		width += sprintf(tacString + width, "%s = %s", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_stack_reserve:
		width += sprintf(tacString + width, "reserve %d bytes stack", it->operands[0].name.val);
		break;

	case tt_stack_store:
		width += sprintf(tacString + width, "store %s at stack offset %d", it->operands[0].name.str, it->operands[1].name.val);
		break;

	case tt_call:
		if (it->operands[0].name.str == NULL)
			width += sprintf(tacString + width, "call %s", it->operands[1].name.str);
		else
			width += sprintf(tacString + width, "%s = call %s", it->operands[0].name.str, it->operands[1].name.str);

		break;

	case tt_label:
		width += sprintf(tacString + width, "~label %d:", it->operands[0].name.val);
		break;

	case tt_return:
		width += sprintf(tacString + width, "ret %s", it->operands[0].name.str);
		break;

	case tt_do:
		width += sprintf(tacString + width, "do");
		break;

	case tt_enddo:
		width += sprintf(tacString + width, "end do");
		break;
	}

	while (width < 40)
	{
		width += sprintf(tacString + width, " ");
	}

	width += sprintf(tacString + width, "\t");
	for (int i = 0; i < 4; i++)
	{
		if (it->operands[i].type.basicType != vt_null)
		{
			width += sprintf(tacString + width, "[");
			switch (it->operands[i].permutation)
			{
			case vp_standard:
				width += sprintf(tacString + width, " ");
				break;

			case vp_temp:
				width += sprintf(tacString + width, "T");
				break;

			case vp_literal:
				width += sprintf(tacString + width, "L");
				break;

			case vp_objptr:
				width += sprintf(tacString + width, "O");
				break;
			}

			char *typeName = Type_GetName(&it->operands[i].type);
			width += sprintf(tacString + width, " %s", typeName);
			free(typeName);
			if (it->operands[i].castAsType.basicType != vt_null)
			{
				char *castAsTypeName = Type_GetName(&it->operands[i].castAsType);
				width += sprintf(tacString + width, "(%s)", castAsTypeName);
				free(castAsTypeName);
			}
			width += sprintf(tacString + width, "]");
		}
		else
		{
			width += sprintf(tacString + width, "   -   ");
		}
	}

	char *trimmedString = malloc(width + 1);
	sprintf(trimmedString, "%s", tacString);
	free(tacString);
	return trimmedString;
}

void freeTAC(struct TACLine *it)
{
	free(it);
}

char TACLine_isEffective(struct TACLine *it)
{
	switch (it->operation)
	{
	case tt_do:
	case tt_enddo:
		return 0;

	default:
		return 1;
	}
}

struct BasicBlock *BasicBlock_new(int labelNum)
{
	struct BasicBlock *wip = malloc(sizeof(struct BasicBlock));
	wip->TACList = LinkedList_New();
	wip->labelNum = labelNum;
	wip->containsEffectiveCode = 0;
	return wip;
}

void BasicBlock_free(struct BasicBlock *b)
{
	LinkedList_Free(b->TACList, freeTAC);
	free(b);
}

void BasicBlock_append(struct BasicBlock *b, struct TACLine *l)
{
	b->containsEffectiveCode |= TACLine_isEffective(l);
	LinkedList_Append(b->TACList, l);
}

void printBasicBlock(struct BasicBlock *b, int indentLevel)
{
	for (int i = 0; i < indentLevel; i++)
	{
		printf("\t");
	}
	printf("BASIC BLOCK %d\n", b->labelNum);
	for (struct LinkedListNode *runner = b->TACList->head; runner != NULL; runner = runner->next)
	{
		struct TACLine *this = runner->data;
		for (int i = 0; i < indentLevel; i++)
		{
			printf("\t");
		}

		if (runner->data != NULL)
		{
			printTACLine(this);
			printf("\n");
		}
	}
	printf("\n");
}
