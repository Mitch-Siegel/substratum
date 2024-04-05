#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include "substratum_defs.h"
#include "ast.h"
#include "tac.h"
#include "symtab.h"
#include "util.h"
#include "linearizer.h"
#include "codegen.h"

struct Dictionary *parseDict = NULL;

u8 currentVerbosity = 0;

void usage()
{
	printf("Substratum language compiler: Usage\n");
	printf("-i (infile) : specify input substratum file to compile\n");
	printf("-o (outfile): specify output file to generate object code to\n");
	printf("\n");
}

struct Stack *parseProgressStack = NULL;
struct Stack *parsedAsts = NULL;
struct LinkedList *includePath = NULL;
struct LinkedList *inputFiles = NULL;

struct Config config;

void runPreprocessor(char *inFileName)
{
	const u32 basePreprocessorParamCount = 6;
	char **preprocessorArgv = malloc(((includePath->size * 2) + basePreprocessorParamCount) * sizeof(char *));
	u32 preprocessorArgI = 0;

	preprocessorArgv[preprocessorArgI++] = "gcc";
	preprocessorArgv[preprocessorArgI++] = "-x";
	preprocessorArgv[preprocessorArgI++] = "c";
	preprocessorArgv[preprocessorArgI++] = "-E";

	if (strcmp(inFileName, "stdin") != 0)
	{
		preprocessorArgv[preprocessorArgI++] = inFileName;
	}
	else
	{
		preprocessorArgv[preprocessorArgI++] = "- ";
	}

	for (struct LinkedListNode *includePathRunner = includePath->head; includePathRunner != NULL; includePathRunner = includePathRunner->next)
	{
		preprocessorArgv[preprocessorArgI++] = "-I";
		preprocessorArgv[preprocessorArgI++] = includePathRunner->data;
	}

	preprocessorArgv[preprocessorArgI++] = NULL;

	if(execvp(preprocessorArgv[0], preprocessorArgv) < 0)
	{
		ErrorAndExit(ERROR_INTERNAL, "Failed to execute preprocessor: %s\n", strerror(errno));
	}
	ErrorAndExit(ERROR_INTERNAL, "Returned from exec of preprocessor!\n");
}

struct AST *parseFile(char *inFileName)
{
	struct ParseProgress fileProgress;
	memset(&fileProgress, 0, sizeof(struct ParseProgress));
	fileProgress.curLine = 0;
	fileProgress.curCol = 0;
	fileProgress.curLineRaw = 0;
	fileProgress.curColRaw = 0;
	fileProgress.curFile = NULL;
	fileProgress.charsRemainingPerLine = LinkedList_New();
	i32 *lineZeroChars = malloc(sizeof(i32));
	*lineZeroChars = 0;
	LinkedList_Append(fileProgress.charsRemainingPerLine, lineZeroChars);

	fileProgress.dict = parseDict;

	pcc_context_t *parseContext = pcc_create(&fileProgress);

	int pid;
	int preprocessorPipe[2] = {0};
	if (pipe(preprocessorPipe) < 0)
	{
		ErrorAndExit(ERROR_INTERNAL, "Unable to make pipe for preprocessor: %s\n", strerror(errno));
	}
	
	pid = fork();
	if (pid == -1)
	{
		ErrorAndExit(ERROR_INTERNAL, "Couldn't fork to run preprocessor!\n");
	}
	else if (pid == 0)
	{
		dup2(preprocessorPipe[1], STDOUT_FILENO);
		close(preprocessorPipe[0]); // duplicated
		close(preprocessorPipe[1]); // not needed - we don't read from stdout
		runPreprocessor(inFileName);
	}
	else
	{
		close(preprocessorPipe[1]); // we won't write to the preprocessor
		dup2(preprocessorPipe[0], STDIN_FILENO);
		close(preprocessorPipe[0]); // duplicated
		if(waitpid(pid, NULL, 0) != pid)
		{
			ErrorAndExit(ERROR_INTERNAL, "Error waiting for preprocessor (pid %d): %s", pid, strerror(errno));
		}
	}

	fileProgress.f = stdin;

	struct AST *parsed = NULL;
	struct AST *translationUnit = NULL;
	while (pcc_parse(parseContext, &translationUnit))
	{
		parsed = AST_S(parsed, translationUnit);
	}

	pcc_destroy(parseContext);

	LinkedList_Free(fileProgress.charsRemainingPerLine, free);

	return parsed;
}

int main(int argc, char **argv)
{
	char *inFileName = "stdin";
	char *outFileName = "stdout";

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
			size_t nVFlags = strlen(optarg);
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
				printf("Unexpected number of verbosities (%lu) provided\nExpected either 1 to set all levels, or %d to set each level independently\n", nVFlags, STAGE_MAX);
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

	printf("Output will be generated to %s\n\n", outFileName);

	parseProgressStack = Stack_New();

	currentVerbosity = config.stageVerbosities[STAGE_PARSE];

	const int nParseDictBuckets = 10;
	parseDict = Dictionary_New(nParseDictBuckets);

	struct AST *program = parseFile(inFileName);
	LinkedList_Free(includePath, free);

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

	FILE *outFile = stdout;

	if (strcmp(outFileName, "stdout") != 0)
	{
		outFile = fopen(outFileName, "wb");
		if (outFile == NULL)
		{
			ErrorAndExit(ERROR_INTERNAL, "Unable to open output file %s\n", outFileName);
		}
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
			NULL};

		for (int i = 0; boilerplateAsm1[i] != NULL; i++)
		{
			fprintf(outFile, "%s\n", boilerplateAsm1[i]);
		}

		fprintf(outFile, "\t.file 0 \"%s\"\n", inFileName);

		char *boilerplateAsm2[] = {
			"\t.attribute unaligned_access, 0",
			NULL};

		for (int i = 0; boilerplateAsm2[i] != NULL; i++)
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

	return 0;
}
