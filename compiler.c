#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ast.h"
#include "codegen.h"
#include "linearizer.h"
#include "log.h"
#include "ssa.h"
#include "substratum_defs.h"
#include "symtab.h"
#include "tac.h"
#include "util.h"

#include "codegen_riscv.h"

struct Dictionary *parseDict = NULL;

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

    if (execvp(preprocessorArgv[0], preprocessorArgv) < 0)
    {
        InternalError("Failed to execute preprocessor: %s", strerror(errno));
    }
    InternalError("Returned from exec of preprocessor!");
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
    size_t *lineZeroChars = malloc(sizeof(size_t));
    *lineZeroChars = 0;
    LinkedList_Append(fileProgress.charsRemainingPerLine, lineZeroChars);

    fileProgress.dict = parseDict;

    pcc_context_t *parseContext = pcc_create(&fileProgress);

    int pid;
    int preprocessorPipe[2] = {0};
    if (pipe(preprocessorPipe) < 0)
    {
        InternalError("Unable to make pipe for preprocessor: %s", strerror(errno));
    }

    pid = fork();
    if (pid == -1)
    {
        InternalError("Couldn't fork to run preprocessor!");
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
        if (waitpid(pid, NULL, 0) != pid)
        {
            InternalError("Error waiting for preprocessor (pid %d): %s", pid, strerror(errno));
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
            int level = atoi(optarg);
            switch (level)
            {
            case LOG_DEBUG:
            case LOG_INFO:
            case LOG_WARNING:
            case LOG_ERROR:
            case LOG_FATAL:
                setLogLevel(level);
                break;

            default:
                Log(LOG_ERROR, "Unexpected log level %d - expected %d-%d\n", level, LOG_DEBUG, LOG_FATAL);
                usage();
                exit(1);
            }
        }
        break;

        case 'I':
        {
            LinkedList_Append(includePath, strdup(optarg));
        }
        break;

        default:
            Log(LOG_ERROR, "Invalid argument flag \"%c\"", option);
            usage();
            exit(1);
        }
    }

    Log(LOG_INFO, "Output will be generated to %s", outFileName);

    parseProgressStack = Stack_New();

    const int nParseDictBuckets = 10;
    parseDict = Dictionary_New(nParseDictBuckets, (void *(*)(void *))strdup, hashString, (ssize_t(*)(void *, void *))strcmp, free);

    struct AST *program = parseFile(inFileName);
    LinkedList_Free(includePath, free);

    // TODO: option to enable/disable ast dump
    /*printf("Here's the AST(s) we parsed: %p\n", program);
    AST_Print(program, 0);*/
    {
        FILE *astOutFile = NULL;
        astOutFile = fopen("ast.dot", "wb");
        if (astOutFile == NULL)
        {
            InternalError("Unable to open output file ast.dot");
        }
        AST_Dump(astOutFile, program);
    }

    Log(LOG_INFO, "Generating symbol table from AST");
    struct SymbolTable *theTable = walkProgram(program);

    // TODO: option to enable/disable symtab dump
    /*Log(LOG_DEBUG, "Symbol table before scope collapse:");
    SymbolTable_print(theTable, stderr, 1);*/

    Log(LOG_INFO, "Collapsing scopes");

    SymbolTable_collapseScopes(theTable, parseDict);

    generateSsa(theTable);

    // TODO: option to enable/disable symtab dump
    Log(LOG_DEBUG, "Symbol table after linearization/scope collapse:");
    SymbolTable_print(theTable, stderr, 1);

    FILE *outFile = stdout;

    if (strcmp(outFileName, "stdout") != 0)
    {
        outFile = fopen(outFileName, "wb");
        if (outFile == NULL)
        {
            InternalError("Unable to open output file %s", outFileName);
        }
    }

    Log(LOG_INFO, "Generating code");

    {
        char *boilerplateAsm1[] = {
            "\t.Ltext0:",
            "\t.cfi_sections\t.debug_frame",
            NULL};

        for (size_t asmIndex = 0; boilerplateAsm1[asmIndex] != NULL; asmIndex++)
        {
            fprintf(outFile, "%s\n", boilerplateAsm1[asmIndex]);
        }

        fprintf(outFile, "\t.file 1 \"%s\"\n", inFileName);

        char *boilerplateAsm2[] = {
            "\t.attribute unaligned_access, 0",
            NULL};

        for (size_t asmIndex = 0; boilerplateAsm2[asmIndex] != NULL; asmIndex++)
        {
            fprintf(outFile, "%s\n", boilerplateAsm2[asmIndex]);
        }

        fprintf(outFile, "\t.file 2 \"%s\"\n", inFileName);
    }
    generateCodeForProgram(theTable, outFile, riscv_emitPrologue, riscv_emitEpilogue);

    SymbolTable_free(theTable);

    fclose(outFile);
    AST_Free(program);

    // TempList_Free(temps);
    Dictionary_Free(parseDict);

    return 0;
}
