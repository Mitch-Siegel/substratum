#include <stdio.h>
#include <string.h>

#include "ast.h"

extern int curLine;
extern int curCol;
extern char *curFile;

struct AST *AST_New(enum token t, char *value)
{
	struct AST *wip = malloc(sizeof(struct AST));
	wip->child = NULL;
	wip->sibling = NULL;
	wip->type = t;
	wip->value = value;
	wip->sourceLine = curLine;
	wip->sourceCol = curCol;
	wip->sourceFile = curFile;
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

int AST_Compare(struct AST *a, struct AST *b)
{
	if(a->type != b->type)
	{
		return 1;
	}

	if (strcmp(a->value, b->value))
	{
		return 2;
	}

	char recursiveResults = 0;
	if((a->child == NULL) != (b->child == NULL))
	{
		return 3;
	}
	else if(a->child != NULL)
	{
		recursiveResults |= AST_Compare(a->child, b->child);
	}

	if((a->sibling == NULL) != (b->sibling == NULL))
	{
		return 3;
	}
	else if(a->sibling != NULL)
	{
		recursiveResults |= AST_Compare(a->sibling, b->sibling);
	}

	return recursiveResults;
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