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
#include "regalloc.h"
#include "ssa.h"
#include "substratum_defs.h"
#include "symtab.h"
#include "tac.h"
#include "util.h"

#include "codegen_riscv.h"
#include "regalloc_riscv.h"

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

void run_preprocessor(char *inFileName)
{
    const u32 BASE_PREPROCESSOR_PARAM_COUNT = 6;
    char **preprocessorArgv = malloc(((includePath->size * 2) + BASE_PREPROCESSOR_PARAM_COUNT) * sizeof(char *));
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

struct Ast *parse_file(char *inFileName)
{
    struct ParseProgress fileProgress;
    memset(&fileProgress, 0, sizeof(struct ParseProgress));
    fileProgress.curLine = 1;
    fileProgress.curCol = 1;
    fileProgress.curLineRaw = 1;
    fileProgress.curColRaw = 1;
    fileProgress.curFile = dictionary_lookup_or_insert(parseDict, inFileName);
    fileProgress.charsRemainingPerLine = linked_list_new();

    size_t *firstLineChars = malloc(sizeof(size_t));
    *firstLineChars = 0;
    linked_list_append(fileProgress.charsRemainingPerLine, firstLineChars);

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
        run_preprocessor(inFileName);
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

    struct Ast *parsed = NULL;
    struct Ast *translationUnit = NULL;
    while (pcc_parse(parseContext, &translationUnit))
    {
        parsed = AST_S(parsed, translationUnit);
    }

    pcc_destroy(parseContext);

    linked_list_free(fileProgress.charsRemainingPerLine, free);

    return parsed;
}

int main(int argc, char **argv)
{
    char *inFileName = "stdin";
    char *outFileName = "stdout";

    includePath = linked_list_new();

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
                set_log_level(level);
                break;

            default:
                log(LOG_ERROR, "Unexpected log level %d - expected %d-%d\n", level, LOG_DEBUG, LOG_FATAL);
                usage();
                exit(1);
            }
        }
        break;

        case 'I':
        {
            linked_list_append(includePath, strdup(optarg));
        }
        break;

        default:
            log(LOG_ERROR, "Invalid argument flag \"%c\"", option);
            usage();
            exit(1);
        }
    }

    log(LOG_INFO, "Output will be generated to %s", outFileName);

    parseProgressStack = stack_new();

    const int N_PARSE_DICT_BUCKETS = 10;
    parseDict = dictionary_new(N_PARSE_DICT_BUCKETS, (void *(*)(void *))strdup, hash_string, (ssize_t(*)(void *, void *))strcmp, free);

    struct Ast *program = parse_file(inFileName);
    linked_list_free(includePath, free);

    // TODO: option to enable/disable ast dump
    {
        FILE *astOutFile = NULL;
        astOutFile = fopen("ast.dot", "wb");
        if (astOutFile == NULL)
        {
            InternalError("Unable to open output file ast.dot");
        }
        ast_dump(astOutFile, program);
    }

    log(LOG_INFO, "Generating symbol table from AST");
    struct SymbolTable *theTable = walk_program(program);

    // TODO: option to enable/disable symtab dump
    /*log(LOG_DEBUG, "Symbol table before scope collapse:");
    SymbolTable_print(theTable, stderr, 1);*/

    log(LOG_INFO, "Collapsing scopes");

    symbol_table_collapse_scopes(theTable, parseDict);

    generate_ssa(theTable);

    // TODO: option to enable/disable symtab dump
    log(LOG_DEBUG, "Symbol table after linearization/scope collapse:");
    symbol_table_print(theTable, stderr, 1);

    FILE *outFile = stdout;

    if (strcmp(outFileName, "stdout") != 0)
    {
        outFile = fopen(outFileName, "wb");
        if (outFile == NULL)
        {
            InternalError("Unable to open output file %s", outFileName);
        }
    }

    log(LOG_INFO, "Generating code");

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

    setupMachineInfo = riscv_setup_machine_info;
    struct MachineInfo *info = setupMachineInfo();

    allocate_registers_for_program(theTable, info);

    generate_code_for_program(theTable, outFile, info, riscv_emit_prologue, riscv_emit_epilogue, riscv_generate_code_for_basic_block);

    machine_info_free(info);

    symbol_table_free(theTable);

    fclose(outFile);
    ast_free(program);

    // TempListFree(temps);
    dictionary_free(parseDict);

    return 0;
}
