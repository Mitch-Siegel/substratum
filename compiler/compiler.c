#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "parser.h"
#include "tac.h"
#include "symtab.h"
#include "util.h"
#include "linearizer.h"
#include "codegen.h"
#include "serialize.h"

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

	printf("Parsing program from %s\n", argv[1]);

	printf("Output will be generated to %s\n\n", argv[2]);
	parseDict = Dictionary_New(10);
	struct AST *program = ParseProgram(argv[1], parseDict);

	// serializeAST("astdump", program);
	// printf("\n");

	// AST_Print(program, 0);

	printf("Generating symbol table from AST");
	struct SymbolTable *theTable = walkAST(program);
	printf("\n");

	if (argc > 3)
	{
		printf("Symbol table before scope collapse:\n");
		SymbolTable_print(theTable, 0);
	}

	printf("Linearizing code to basic blocks\n");
	struct TempList *temps = TempList_New();
	linearizeProgram(program, theTable->globalScope, parseDict, temps);

	printf("Collapsing scopes\n");
	SymbolTable_collapseScopes(theTable, parseDict);

	if (argc > 3)
	{
		printf("Symbol table after linearization/scope collapse:\n");
		SymbolTable_print(theTable, 1);
	}

	FILE *outFile = fopen(argv[2], "wb");

	printf("Generating code\n");
	fprintf(outFile, "#include \"CPU.asm\"\nentry code\n");
	generateCode(theTable, outFile);

	SymbolTable_free(theTable);

	fclose(outFile);
	AST_Free(program);

	TempList_Free(temps);
	Dictionary_Free(parseDict);

	printf("done!\n");
}
