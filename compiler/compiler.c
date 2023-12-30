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

struct Dictionary *parseDict = NULL;

char currentVerbosity = 0;

void usage()
{
	printf("Classical language compiler: Usage\n");
	printf("-i (infile) : specify input classical file to compile\n");
	printf("-o (outfile): specify output file to generate object code to\n");
	printf("\n");
}

struct Config config;
int main(int argc, char **argv)
{
	char *inFileName = NULL;
	char *outFileName = NULL;

	int option;
	while ((option = getopt(argc, argv, "i:o:O:l:r:c:v:")) != EOF)
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

	currentVerbosity = config.stageVerbosities[STAGE_PARSE];

	parseDict = Dictionary_New(10);

	struct AST *program = NULL;

	struct ParseProgress p;
	memset(&p, 0, sizeof(struct ParseProgress));
	p.curLine = 1;
	p.curCol = 1;
	p.curFile = "FILE";
	p.charsRemainingPerLine = LinkedList_New();
	int *lineZeroChars = malloc(sizeof(int));
	*lineZeroChars = 0;
	LinkedList_Append(p.charsRemainingPerLine, lineZeroChars);

	p.f = fopen(inFileName, "rb");
	if (p.f == NULL)
	{
		ErrorAndExit(ERROR_CODE, "Unable to open input file %s!\n", inFileName);
	}

	p.dict = parseDict;

	pcc_context_t *parseContext = pcc_create(&p);


	while (pcc_parse(parseContext, &program))
	{
	}

	pcc_destroy(parseContext);

	fclose(p.f);
	LinkedList_Free(p.charsRemainingPerLine, free);

	// struct AST *program = ParseProgram("/tmp/auto.capp", parseDict);

	// serializeAST("astdump", program);
	// printf("\n");
	if (currentVerbosity > VERBOSITY_MINIMAL)
	{
		printf("Here's the AST we parsed: %p\n", program);
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
	// fprintf(outFile, "#include \"CPU.asm\"\nentry code\n");
	generateCodeForProgram(theTable, outFile);

	SymbolTable_free(theTable);

	fclose(outFile);
	AST_Free(program);

	// TempList_Free(temps);
	Dictionary_Free(parseDict);
}
