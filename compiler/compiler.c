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
	printf("\n");

	AST_Print(program, 0);
	
	printf("Generating symbol table from AST");
	struct SymbolTable *theTable = walkAST(program);
	printf("\n");


	printf("Symbol table before scope collapse:\n");
	SymbolTable_print(theTable, 0);

	printf("Linearizing code to basic blocks\n");
	linearizeProgram(program, theTable->globalScope, parseDict);

	printf("Collapsing scopes\n");
	SymbolTable_collapseScopes(theTable, parseDict);

	printf("Symbol table after linearization/scope collapse:\n");
	SymbolTable_print(theTable, 0);

	

	FILE *outFile = fopen(argv[2], "wb");

	printf("Generating code\n");
	struct Stack *outputBlocks;
	outputBlocks = generateCode(theTable, outFile);
	fprintf(outFile, "#include \"CPU.asm\"\n#include \"INT.asm\"\n");
	for(int i = 0; i < outputBlocks->size; i++)
	{
		struct LinkedList *thisBlock = outputBlocks->data[i];
		for(struct LinkedListNode *asmLine = thisBlock->head; asmLine != NULL; asmLine = asmLine->next)
		{
			char *s = asmLine->data;
			if(s[strlen(s) - 1] != ':')
			{
				fprintf(outFile, "\t");	
			}

			fprintf(outFile, "%s\n", s);
		}
		// ASM_output(outputBlocks->data[i], outFile);
		LinkedList_Free(thisBlock, free);
	}
	SymbolTable_free(theTable);

	Stack_Free(outputBlocks);

	fclose(outFile);
	// freeDictionary(parseDict);
	AST_Free(program);

	printf("done printing\n");
}
