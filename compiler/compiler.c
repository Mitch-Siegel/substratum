#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include "ast.h"
#include "parser.h"
#include "tac.h"
#include "symtab.h"
#include "util.h"
#include "linearizer.h"
#include "codegen.h"
#include "serialize.h"

char verifyBasicBlock(struct BasicBlock *b)
{
	char blockHasError = 0;
	for (struct LinkedListNode *runner = b->TACList->head; runner != NULL; runner = runner->next)
	{
		struct TACLine *checkedLine = runner->data;
		char result = checkTACLine(checkedLine);
		blockHasError |= result;
		if(result)
		{
			printf("\t%s %d:%d\n", checkedLine->correspondingTree->sourceFile, checkedLine->correspondingTree->sourceLine, checkedLine->correspondingTree->sourceCol);
		}
	}
	return blockHasError;
}

char verifyTAC(struct Scope *s)
{
	char foundError = 0;
	for (int i = 0; i < s->entries->size; i++)
	{
		struct ScopeMember *m = s->entries->data[i];
		switch (m->type)
		{

		case e_basicblock:
			foundError |= verifyBasicBlock(m->entry);
			break;

		case e_scope:
			foundError |= verifyTAC(m->entry);
			break;

		case e_function:
		{
			struct FunctionEntry *f = m->entry;
			foundError |= verifyTAC(f->mainScope);
		}
		break;

		default:
			break;
		}
	}
	return foundError;
}

struct Dictionary *parseDict = NULL;
int main(int argc, char **argv)
{
	if (argc < 2)
	{
		ErrorAndExit(ERROR_INVOCATION, "Error - please specify an input and output file!\n");
	}
	else if (argc < 3)
	{
		ErrorAndExit(ERROR_INVOCATION, "Error - please specify an output file!\n");
	}

	printf("Compiling source file %s\n", argv[1]);

	printf("Output will be generated to %s\n\n", argv[2]);

	int pid, status;

	if ((pid = fork()) == -1)
	{
		ErrorAndExit(ERROR_INTERNAL, "Unable to fork!\n");
	}

	if (pid == 0)
	{
		// janky fix: write the preprocessed file to /tmp
		char *args[4] = {"./capp", argv[1], "/tmp/auto.capp", NULL};

		if (execvp("./capp", args) < 0)
		{
			perror(strerror(errno));
			ErrorAndExit(ERROR_INTERNAL, "Unable to execute preprocessor!\n");
		}
		exit(0);
	}
	else
	{
		wait(&status);
		if (status)
		{
			ErrorAndExit(ERROR_INTERNAL, "Preprocessor execution failed!\n");
		}
		else
		{
			printf("\n");
		}
	}

	parseDict = Dictionary_New(10);
	struct AST *program = ParseProgram("/tmp/auto.capp", parseDict);

	// serializeAST("astdump", program);
	// printf("\n");

	// AST_Print(program, 0);

	printf("Generating symbol table from AST");
	struct SymbolTable *theTable = linearizeProgram(program);
	printf("\n");

	if (argc > 3)
	{
		printf("Symbol table before scope collapse:\n");
		SymbolTable_print(theTable, 0);
	}

	printf("Linearizing code to basic blocks\n");
	// struct TempList *temps = TempList_New();
	// linearizeProgram(program, theTable->globalScope, parseDict, temps);

	printf("Collapsing scopes\n");
	SymbolTable_collapseScopes(theTable, parseDict);


	if (argc > 3)
	{
		printf("Symbol table after linearization/scope collapse:\n");
		SymbolTable_print(theTable, 1);
	}

	if(verifyTAC(theTable->globalScope))
	{
		ErrorAndExit(ERROR_INTERNAL, "Error(s) verifying TAC!\n");
	}

	// printf("Symbol table after linearization/scope collapse:\n");
	// SymbolTable_print(theTable, 1);

	// ensure we always end the userstart section (jumped to from entry) by calling our main functoin
	// just fudge this by calling it block number 123456 since we should never get that high
	struct BasicBlock *executeMainBlock = BasicBlock_new(123456);

	struct TACLine *asm1 = newTACLine(0, tt_asm, NULL);
	asm1->operands[0].name.str = "call main";
	struct TACLine *asm2 = newTACLine(0, tt_asm, NULL);
	asm2->operands[0].name.str = "hlt";

	BasicBlock_append(executeMainBlock, asm1);
	BasicBlock_append(executeMainBlock, asm2);

	Scope_insert(theTable->globalScope, "CALL_MAIN_BLOCK", executeMainBlock, e_basicblock);

	// BasicBlock_append(

	FILE *outFile = fopen(argv[2], "wb");

	if (outFile == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Unable to open output file %s\n", argv[2]);
	}

	printf("Generating code\n");
	// fprintf(outFile, "#include \"CPU.asm\"\nentry code\n");
	generateCode(theTable, outFile);

	SymbolTable_free(theTable);

	fclose(outFile);
	AST_Free(program);

	// TempList_Free(temps);
	Dictionary_Free(parseDict);

	printf("done!\n");
}
