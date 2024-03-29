#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include "ast.h"
#include "tac.h"
#include "symtab.h"
#include "util.h"
#include "linearizer.h"
#include "codegen.h"

struct Dictionary *parseDict = NULL;

char currentVerbosity = 0;


void usage()
{
	printf("Classical language compiler: Usage\n");
	printf("-i (infile) : specify input classical file to compile\n");
	printf("-o (outfile): specify output file to generate object code to\n");
	printf("\n");
}

struct Stack *parseProgressStack = NULL;
struct Stack *parsedAsts = NULL;
struct LinkedList *includePath = NULL;


struct Config config;

int main(int argc, char **argv)
{
	char *inFileName = NULL;
	char *outFileName = NULL;

	parsedAsts = Stack_New();
	includePath = LinkedList_New();

	int option;
	while ((option = getopt(argc, argv, "i:o:O:l:r:c:v:I:")) != EOF)
	{
		switch (option)
		{
		case 'i':
			inFileName = optarg;
			break;

		case 'o':
			outFileName = optarg;
			break;

		case 'v':
		{
			int nVFlags = strlen(optarg);
			if (nVFlags == 1)
			{
				int stageVerbosities = atoi(optarg);
				if (stageVerbosities < 0 || stageVerbosities > VERBOSITY_MAX)
				{
					printf("Illegal value %d specified for verbosity!\n", stageVerbosities);
					usage();
					ErrorAndExit(ERROR_INVOCATION, ":(");
				}

				for (int i = 0; i < STAGE_MAX; i++)
				{
					config.stageVerbosities[i] = stageVerbosities;
				}
			}
			else if (nVFlags == STAGE_MAX)
			{
				for (int i = 0; i < STAGE_MAX; i++)
				{
					char thisVerbosityStr[2] = {'\0', '\0'};
					thisVerbosityStr[0] = optarg[i];
					config.stageVerbosities[i] = atoi(thisVerbosityStr);
				}
			}
			else
			{
				printf("Unexpected number of verbosities (%d) provided\nExpected either 1 to set all levels, or %d to set each level independently\n", nVFlags, STAGE_MAX);
				usage();
				ErrorAndExit(ERROR_INVOCATION, ":(");
			}
		}
		break;

		case 'I':
		{
			LinkedList_Append(includePath, strdup(optarg));
		}
			break;

		default:
			usage();
			ErrorAndExit(ERROR_INVOCATION, ":(");
		}
	}

	printf("Running with verbosity: ");
	for (int i = 0; i < STAGE_MAX; i++)
	{
		printf("%d ", config.stageVerbosities[i]);
	}
	printf("\n");

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

	parseProgressStack = Stack_New();

	currentVerbosity = config.stageVerbosities[STAGE_PARSE];

	parseDict = Dictionary_New(10);

	parseFile(inFileName);

	LinkedList_Free(includePath, free);

	// struct AST *program = ParseProgram("/tmp/auto.capp", parseDict);

	// serializeAST("astdump", program);
	// printf("\n");

	struct AST *program = NULL;
	for(int i = 0; i < parsedAsts->size; i++)
	{
		program = AST_ConstructAddSibling(program, parsedAsts->data[i]);
	}

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Here's the AST(s) we parsed: %p\n", program);
		AST_Print(program, 0);
	}

	currentVerbosity = config.stageVerbosities[STAGE_LINEARIZE];

	if (currentVerbosity > VERBOSITY_SILENT)
	{
		printf("Generating symbol table from AST");
	}
	struct SymbolTable *theTable = walkProgram(program);

	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("\nSymbol table before scope collapse:\n");
		SymbolTable_print(theTable, 1);
		printf("Collapsing scopes\n");
	}

	SymbolTable_collapseScopes(theTable, parseDict);

	if (currentVerbosity > VERBOSITY_SILENT)
	{
		printf("Symbol table after linearization/scope collapse:\n");
		SymbolTable_print(theTable, 1);
	}

	FILE *outFile = fopen(outFileName, "wb");

	if (outFile == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Unable to open output file %s\n", outFileName);
	}

	currentVerbosity = config.stageVerbosities[STAGE_CODEGEN];

	if (currentVerbosity > VERBOSITY_SILENT)
	{
		printf("Generating code\n");
	}
	
	{
		char *boilerplateAsm1[] = {
			"\t.Ltext0:",
			"\t.cfi_sections\t.debug_frame",
			NULL
		};

		for(int i = 0; boilerplateAsm1[i] != NULL; i++)
		{
			fprintf(outFile, "%s\n", boilerplateAsm1[i]);
		}

		fprintf(outFile, "\t.file 0 \"%s\"\n", inFileName);

		char *boilerplateAsm2[] = {
			"\t.attribute unaligned_access, 0",
			NULL
		};

		for(int i = 0; boilerplateAsm2[i] != NULL; i++)
		{
			fprintf(outFile, "%s\n", boilerplateAsm2[i]);
		}

		fprintf(outFile, "\t.file 1 \"%s\"\n", inFileName);
	}
	generateCodeForProgram(theTable, outFile);

	SymbolTable_free(theTable);

	fclose(outFile);
	AST_Free(program);

	// TempList_Free(temps);
	Dictionary_Free(parseDict);
}
