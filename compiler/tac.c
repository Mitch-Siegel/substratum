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

		case vt_uint8:
			switch (b->basicType)
			{
			case vt_null:
			case vt_class:
				return 1;
			case vt_uint8:
			case vt_uint16:
			case vt_uint32:
				break;

			default:
			}
			break;

		case vt_uint16:
			switch (b->basicType)
			{
			case vt_null:
			case vt_uint8:
			case vt_class:
				return 1;
			case vt_uint16:
			case vt_uint32:
				break;
			}
			break;

		case vt_uint32:
			switch (b->basicType)
			{
			case vt_null:
			case vt_uint8:
			case vt_uint16:
			case vt_class:
				return 1;

			case vt_uint32:
				break;
			}
			break;

		case vt_class:
			switch (b->basicType)
			{
			case vt_null:
			case vt_uint8:
			case vt_uint16:
			case vt_uint32:
				return 1;

			case vt_class:
				break;
			}
		}
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

char *Type_GetName(struct Type *t)
{
	char *typeName = malloc(1024);
	int len = 0;
	switch (t->basicType)
	{
	case vt_null:
		len = sprintf(typeName, "NOTYPE");
		break;

	case vt_uint8:
		len = sprintf(typeName, "uint8");
		break;

	case vt_uint16:
		len = sprintf(typeName, "uint16");
		break;

	case vt_uint32:
		len = sprintf(typeName, "uint32");
		break;

	case vt_class:
		sprintf(typeName, "%s", t->classType.name);

	default:
		ErrorAndExit(ERROR_INTERNAL, "Unexpected enum basicTypes value %d seen in Type_GetName!\n", t->basicType);
	}

	int i;
	for (i = 0; i < t->indirectionLevel; i++)
	{
		typeName[len + i] = '*';
		len += sprintf(typeName + len, "*");
	}
	typeName[len + i] = '\0';
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

	case tt_cast_assign:
		return "";

	case tt_declare:
		return "";

	case tt_add:
		return "add";

	case tt_subtract:
		return "sub";

	case tt_mul:
		return "mul";

	case tt_div:
		return "div";

	case tt_dereference:
		return "dereference";

	case tt_reference:
		return "reference";

	case tt_memw_1:
		return "mov (reg), reg";

	case tt_memw_2:
		return "mov basereg + offset, reg";

	case tt_memw_3:
		return "mov basereg + (offreg * 2^sclpow), reg";

	case tt_memw_2_n:
		return "mov basereg - offset, reg";

	case tt_memw_3_n:
		return "mov basereg - (offreg * 2^sclpow), reg";

	case tt_memr_1:
		return "mov reg, (reg)";

	case tt_memr_2:
		return "mov reg, basereg + offset";

	case tt_memr_3:
		return "mov reg, basereg + (offreg * 2^sclpow)";

	case tt_memr_2_n:
		return "mov reg, basereg - offset";

	case tt_memr_3_n:
		return "mov reg, basereg - (offreg * 2^sclpow)";

	case tt_cmp:
		return "cmp";

	case tt_push:
		return "push";

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

	case tt_jg:
		return "jg";

	case tt_jge:
		return "jge";

	case tt_jl:
		return "jl";

	case tt_jle:
		return "jle";

	case tt_je:
		return "je";

	case tt_jne:
		return "jne";

	case tt_jz:
		return "jz";

	case tt_jnz:
		return "jnz";

	case tt_jmp:
		return "jmp";
	}
	return "";
}

struct TACLine *newTACLine(int index, enum TACType operation, struct AST *correspondingTree)
{
	struct TACLine *wip = malloc(sizeof(struct TACLine));
	for (int i = 0; i < 4; i++)
	{
		wip->operands[i].name.str = NULL;
		wip->operands[i].permutation = vp_standard;

		wip->operands[i].type.basicType = vt_null;
		wip->operands[i].type.classType.name = NULL;
		wip->operands[i].type.indirectionLevel = 0;

		wip->operands[i].castAsType.basicType = vt_null;
		wip->operands[i].castAsType.classType.name = NULL;
		wip->operands[i].castAsType.indirectionLevel = 0;
	}
	wip->correspondingTree = correspondingTree;

	// default type of a line of TAC is assignment
	wip->operation = operation;
	// by default operands are NOT reorderable
	wip->reorderable = 0;
	wip->index = index;
	return wip;
}

void printTACLine(struct TACLine *it)
{
	char *operationStr;
	char fallingThrough = 0;
	int width;
	if (TACLine_isEffective(it))
	{
		width = printf("%2x: ", it->index);
	}
	else
	{
		width = printf(" ~  ");
	}

	switch (it->operation)
	{
	case tt_asm:
		width += printf("ASM:%s", it->operands[0].name.str);
		break;

	case tt_add:
		if (!fallingThrough)
			operationStr = "+";
		fallingThrough = 1;
	case tt_subtract:
		if (!fallingThrough)
			operationStr = "-";
		fallingThrough = 1;
	case tt_mul:
		if (!fallingThrough)
			operationStr = "*";
		fallingThrough = 1;
	case tt_div:
		if (!fallingThrough)
			operationStr = "/";
		fallingThrough = 1;

		width += printf("%s = %s %s %s", it->operands[0].name.str, it->operands[1].name.str, operationStr, it->operands[2].name.str);
		break;

	case tt_dereference:
		width += printf("%s = *%s", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_reference:
		width += printf("%s = &%s", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_memw_1:
		// operands: base source
		width += printf("(%s) = %s", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_memw_2:
		// operands: base offset source
		width += printf("(%s + %d) = %s", it->operands[0].name.str, it->operands[1].name.val, it->operands[2].name.str);
		break;

	case tt_memw_3:
		// operands base offset scale source
		width += printf("(%s + %s*2^%d) = %s", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.val, it->operands[3].name.str);
		break;

	case tt_memw_2_n:
		// operands: base offset source
		width += printf("(%s - %d) = %s", it->operands[0].name.str, it->operands[1].name.val, it->operands[2].name.str);
		break;

	case tt_memw_3_n:
		// operands base offset scale source
		width += printf("(%s - %s*2^%d) = %s", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.val, it->operands[3].name.str);
		break;

	case tt_memr_1:
		// operands: dest base
		width += printf("%s = (%s)", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_memr_2:
		// operands: dest base offset
		width += printf("%s = (%s + %d)", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.val);
		break;

	case tt_memr_3:
		// operands: dest base offset scale
		width += printf("%s = (%s + %s*2^%d)", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.str, it->operands[3].name.val);
		break;

	case tt_memr_2_n:
		// operands: dest base offset
		width += printf("%s = (%s - %d)", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.val);
		break;

	case tt_memr_3_n:
		// operands: dest base offset scale
		width += printf("%s = (%s - %s*2^%d)", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.str, it->operands[3].name.val);
		break;

	case tt_jg:
	case tt_jge:
	case tt_jl:
	case tt_jle:
	case tt_je:
	case tt_jne:
	case tt_jz:
	case tt_jnz:
	case tt_jmp:
		width += printf("%s basicblock %d", getAsmOp(it->operation), it->operands[0].name.val);
		break;

	case tt_cmp:
		width += printf("cmp %s %s", it->operands[1].name.str, it->operands[2].name.str);
		break;

	case tt_assign:
		width += printf("%s = %s", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_cast_assign:
		width += printf("%s CAST= %s", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_declare:
		width += printf("declare %s", it->operands[0].name.str);

		// also deal with printing the size of the array it is declared as
		if (it->operands[1].name.str != NULL)
		{
			width += printf("[%s]", it->operands[1].name.str);
		}
		break;

	case tt_push:
		width += printf("push %s", it->operands[0].name.str);
		break;

	case tt_call:
		if (it->operands[0].name.str == NULL)
		{
			width += printf("call %s", it->operands[1].name.str);
		}
		else
		{
			width += printf("%s = call %s", it->operands[0].name.str, it->operands[1].name.str);
		}

		break;

	case tt_label:
		width += printf("~label %d:", it->operands[0].name.val);
		break;

	case tt_return:
		width += printf("ret %s", it->operands[0].name.str);
		break;

	case tt_do:
		width += printf("do");
		break;

	case tt_enddo:
		width += printf("end do");
		break;
	}
	while (width++ < 40)
	{
		printf(" ");
	}
	printf("\t");
	for (int i = 0; i < 4; i++)
	{
		if (it->operands[i].type.basicType != vt_null)
		{
			printf("[%d", it->operands[i].type.basicType);
			switch (it->operands[i].permutation)
			{
			case vp_standard:
				printf(" ");
				break;

			case vp_temp:
				printf("T");
				break;

			case vp_literal:
				printf("L");
				break;

			case vp_objptr:
				printf("O");
				break;
			}
			printf(" %d*", it->operands[i].type.indirectionLevel);

			if (it->operands[i].castAsType.basicType != vt_null)
			{
				printf("(%d %d*)", it->operands[i].castAsType.basicType, it->operands[i].castAsType.indirectionLevel);
			}
			printf("]");
		}
		else
		{
			printf("   -   ");
		}
	}
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
		width += sprintf(tacString, "ASM:%s", it->operands[0].name.str);
		break;

	case tt_add:
		if (!fallingThrough)
			operationStr = "+";
		fallingThrough = 1;
	case tt_subtract:
		if (!fallingThrough)
			operationStr = "-";
		fallingThrough = 1;
	case tt_mul:
		if (!fallingThrough)
			operationStr = "*";
		fallingThrough = 1;
	case tt_div:
		if (!fallingThrough)
			operationStr = "/";

		width += sprintf(tacString, "%s = %s %s %s", it->operands[0].name.str, it->operands[1].name.str, operationStr, it->operands[2].name.str);
		break;

	case tt_dereference:
		width += sprintf(tacString, "%s = *%s", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_reference:
		width += sprintf(tacString, "%s = &%s", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_memw_1:
		// operands: base source
		width += sprintf(tacString, "(%s) = %s", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_memw_2:
		// operands: base offset source
		width += sprintf(tacString, "(%s + %d) = %s", it->operands[0].name.str, it->operands[1].name.val, it->operands[2].name.str);
		break;

	case tt_memw_3:
		// operands base offset scale source
		width += sprintf(tacString, "(%s + %s * 2^%d) = %s", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.val, it->operands[3].name.str);
		break;

	case tt_memw_2_n:
		// operands: base offset source
		width += sprintf(tacString, "(%s - %d) = %s", it->operands[0].name.str, it->operands[1].name.val, it->operands[2].name.str);
		break;

	case tt_memw_3_n:
		// operands base offset scale source
		width += sprintf(tacString, "(%s - %s * 2^%d) = %s", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.val, it->operands[3].name.str);
		break;

	case tt_memr_1:
		// operands: dest base
		width += sprintf(tacString, "%s = (%s)", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_memr_2:
		// operands: dest base offset
		width += sprintf(tacString, "%s = (%s + %d)", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.val);
		break;

	case tt_memr_3:
		// operands: dest base offset scale
		width += sprintf(tacString, "%s = (%s + %s * 2^%d)", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.str, it->operands[3].name.val);
		break;

	case tt_memr_2_n:
		// operands: dest base offset
		width += sprintf(tacString, "%s = (%s - %d)", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.val);
		break;

	case tt_memr_3_n:
		// operands: dest base offset scale
		width += sprintf(tacString, "%s = (%s - %s * 2^%d)", it->operands[0].name.str, it->operands[1].name.str, it->operands[2].name.str, it->operands[3].name.val);
		break;

	case tt_jg:
	case tt_jge:
	case tt_jl:
	case tt_jle:
	case tt_je:
	case tt_jne:
	case tt_jz:
	case tt_jnz:
	case tt_jmp:
		width += sprintf(tacString, "%s basicblock %d", getAsmOp(it->operation), it->operands[0].name.val);
		break;

	case tt_cmp:
		width += sprintf(tacString, "cmp %s %s", it->operands[1].name.str, it->operands[2].name.str);
		break;

	case tt_assign:
		width += sprintf(tacString, "%s = %s", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_cast_assign:
		width += sprintf(tacString, "%s CAST= %s", it->operands[0].name.str, it->operands[1].name.str);
		break;

	case tt_declare:
		width += sprintf(tacString, "declare %s", it->operands[0].name.str);
		break;

	case tt_push:
		width += sprintf(tacString, "push %s", it->operands[0].name.str);
		break;

	case tt_call:
		if (it->operands[0].name.str == NULL)
			width += sprintf(tacString, "call %s", it->operands[1].name.str);
		else
			width += sprintf(tacString, "%s = call %s", it->operands[0].name.str, it->operands[1].name.str);

		break;

	case tt_label:
		width += sprintf(tacString, "~label %d:", it->operands[0].name.val);
		break;

	case tt_return:
		width += sprintf(tacString, "ret %s", it->operands[0].name.str);
		break;

	case tt_do:
		width += sprintf(tacString, "do");
		break;

	case tt_enddo:
		width += sprintf(tacString, "end do");
		break;
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

void BasicBlock_prepend(struct BasicBlock *b, struct TACLine *l)
{
	b->containsEffectiveCode |= TACLine_isEffective(l);
	LinkedList_Prepend(b->TACList, l);
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
	printf("\n\n");
}
