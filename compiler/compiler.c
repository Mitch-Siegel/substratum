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

struct Dictionary *parseDict = NULL;

#define MAX_LINEARIAZTION_OPT 0
#define MAX_REGALLOC_OPT 0
#define MAX_CODEGEN_OPT 0
#define MAX_GENERIC_OPT 0
int linearizationOpt = 0;
int regAllocOpt = 0;
int codegenOpt = 0;

void usage()
{
	printf("Classical language compiler: Usage\n");
	printf("-i (infile) : specify input classical file to compile\n");
	printf("-o (outfile): specify output file to generate object code to\n");
	printf("-O (number) : set generic optimization tier level: max %d\n              (auto-combines l/r/c optimizations)\n", MAX_GENERIC_OPT);
	printf("-l (number) : linearization (IR generation) optimization level: max %d\n", MAX_LINEARIAZTION_OPT);
	printf("-r (number) : register allocation optimization level: max %d\n", MAX_REGALLOC_OPT);
	printf("-c (number) : code generation optimization level: max %d\n", MAX_CODEGEN_OPT);
	printf("\n");
}

void setOptimizations(int level)
{
	switch (level)
	{
	case 0:
		linearizationOpt = 0;
		regAllocOpt = 0;
		codegenOpt = 0;
		break;

	default:
		usage();
		ErrorAndExit(ERROR_INVOCATION, "Invalid value (%d) provided to -O flag (max %d)!\n", level, MAX_GENERIC_OPT);
	}
}

void checkOptimizations()
{
	if (linearizationOpt > MAX_LINEARIAZTION_OPT)
	{
		ErrorAndExit(ERROR_INVOCATION, "Provided value (%d) for linearization optimiaztion exceeds max (%d)!\n", linearizationOpt, MAX_LINEARIAZTION_OPT);
		usage();
	}

	if (regAllocOpt > MAX_REGALLOC_OPT)
	{
		ErrorAndExit(ERROR_INVOCATION, "Provided value (%d) for linearization optimiaztion exceeds max (%d)!\n", regAllocOpt, MAX_REGALLOC_OPT);
		usage();
	}

	if (codegenOpt > MAX_CODEGEN_OPT)
	{
		ErrorAndExit(ERROR_INVOCATION, "Provided value (%d) for linearization optimiaztion exceeds max (%d)!\n", codegenOpt, MAX_CODEGEN_OPT);
		usage();
	}
}

int main(int argc, char **argv)
{
	char *inFileName = NULL;
	char *outFileName = NULL;

	int option;
	while ((option = getopt(argc, argv, "i:o:O:l:r:c:")) != EOF)
	{
		switch (option)
		{
		case 'i':
			inFileName = optarg;
			break;

		case 'o':
			outFileName = optarg;
			break;

		case 'O':
			setOptimizations(atoi(optarg));
			break;

		case 'l':
			linearizationOpt = atoi(optarg);
			break;

		case 'r':
			regAllocOpt = atoi(optarg);
			break;

		case 'c':
			codegenOpt = atoi(optarg);
			break;

		default:
			usage();
			ErrorAndExit(ERROR_INVOCATION, ":(");
		}
	}

	checkOptimizations();

	if (inFileName == NULL)
	{
		usage();
		ErrorAndExit(ERROR_INVOCATION, "No input file provided!\n");
	}

	if (outFileName == NULL)
	{
		usage();
		ErrorAndExit(ERROR_INVOCATION, "No input file provided!\n");
	}

	printf("Compiling source file %s\n", inFileName);

	printf("Output will be generated to %s\n\n", outFileName);

	int pid, status;

	if ((pid = fork()) == -1)
	{
		ErrorAndExit(ERROR_INTERNAL, "Unable to fork!\n");
	}

	if (pid == 0)
	{
		// janky fix: write the preprocessed file to /tmp
		char *args[4] = {"./capp", inFileName, "/tmp/auto.capp", NULL};

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

	AST_Print(program, 0);

	printf("Generating symbol table from AST");
	struct SymbolTable *theTable = linearizeProgram(program, linearizationOpt);
	printf("\n");

	if (argc > 5)
	{
		printf("Symbol table before scope collapse:\n");
		SymbolTable_print(theTable, 1);
	}

	// printf("Linearizing code to basic blocks\n");
	// struct TempList *temps = TempList_New();
	// linearizeProgram(program, theTable->globalScope, parseDict, temps);

	printf("Collapsing scopes\n");
	SymbolTable_collapseScopes(theTable, parseDict);

	if (argc > 5)
	{
		printf("Symbol table after linearization/scope collapse:\n");
		SymbolTable_print(theTable, 1);
	}

	return 0;
	/*if(verifyTAC(theTable->globalScope))
	{
		ErrorAndExit(ERROR_INTERNAL, "Error(s) verifying TAC!\n");
	}*/

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

	FILE *outFile = fopen(outFileName, "wb");

	if (outFile == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Unable to open output file %s\n", outFileName);
	}

	printf("Generating code\n");
	// fprintf(outFile, "#include \"CPU.asm\"\nentry code\n");
	generateCode(theTable, outFile, regAllocOpt, codegenOpt);

	SymbolTable_free(theTable);

	fclose(outFile);
	AST_Free(program);

	// TempList_Free(temps);
	Dictionary_Free(parseDict);

	printf("done!\n");
}
