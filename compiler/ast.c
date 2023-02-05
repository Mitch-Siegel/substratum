#include <stdio.h>
#include <string.h>

#include "ast.h"

extern int curLine;
extern int curCol;

struct AST *AST_New(enum token t, char *value)
{
	struct AST *wip = malloc(sizeof(struct AST));
	wip->child = NULL;
	wip->sibling = NULL;
	wip->type = t;
	wip->value = value;
	wip->sourceLine = curLine;
	wip->sourceCol = curCol;
	return wip;
}

void AST_InsertSibling(struct AST *it, struct AST *newSibling)
{
	struct AST *runner = it;
	while (runner->sibling != NULL)
		runner = runner->sibling;

	runner->sibling = newSibling;
}

void AST_InsertChild(struct AST *it, struct AST *newChild)
{
	if (it->child == NULL)
		it->child = newChild;
	else
		AST_InsertSibling(it->child, newChild);
}

void AST_Print(struct AST *it, int depth)
{
	if (it->sibling != NULL)
		AST_Print(it->sibling, depth);

	for (int i = 0; i < depth; i++)
		printf("\t");

	printf("%s\n", it->value);
	if (it->child != NULL)
		AST_Print(it->child, depth + 1);
}

void AST_PrintHorizontalRec(struct AST *it){
	if (it->sibling != NULL)
	{
		AST_PrintHorizontalRec(it->sibling);
		printf(" ");
	}

	printf("(");
	printf("%s", it->value);

	if (it->child != NULL)
	{
		printf(" ");
		AST_PrintHorizontalRec(it->child);
	}

	printf(")");
}

void AST_PrintHorizontal(struct AST *it)
{

	printf("(");
	printf("%s", it->value);

	if (it->child != NULL)
	{
		printf(" ");
		AST_PrintHorizontalRec(it->child);
	}

	printf(")");
}

void AST_Free(struct AST *it)
{
	struct AST *runner = it;
	while (runner != NULL)
	{
		if (runner->child != NULL)
		{
			AST_Free(runner->child);
		}
		struct AST *old = runner;
		runner = runner->sibling;
		free(old);
	}
}