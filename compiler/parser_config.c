#include "parser_config.h"
#include <stddef.h>

#include "parser.h"
#include "util.h"

extern struct Dictionary *parseDict;
extern struct Stack *parsedAsts;
extern struct LinkedList *includePath;
void parseFile(char *inFileName)
{
    struct AST *translationUnit = NULL;

    struct ParseProgress p;
    memset(&p, 0, sizeof(struct ParseProgress));
    p.curLine = 1;
    p.curCol = 1;
    p.curLineRaw = 1;
    p.curColRaw = 1;
    p.curFile = inFileName;
    p.charsRemainingPerLine = LinkedList_New();
    int *lineZeroChars = malloc(sizeof(int));
    *lineZeroChars = 0;
    LinkedList_Append(p.charsRemainingPerLine, lineZeroChars);

    char *actualFileName = inFileName;
    p.f = fopen(inFileName, "rb");
    if (p.f == NULL)
    {
        for (struct LinkedListNode *includePathRunner = includePath->head; includePathRunner != NULL; includePathRunner = includePathRunner->next)
        {
            int includedPathLen = strlen(includePathRunner->data);
            char *includedPath = malloc(includedPathLen + strlen(inFileName) + 2);
            strcpy(includedPath, includePathRunner->data);
            includedPath[includedPathLen] = '/';
            strcpy(includedPath + includedPathLen + 1, inFileName);
            p.f = fopen(includedPath, "rb");
            if (p.f != NULL)
            {
                actualFileName = Dictionary_LookupOrInsert(parseDict, includedPath);
                free(includedPath);
                break;
            }
            else
            {
                free(includedPath);
            }
        }

        if (p.f == NULL)
        {
            ErrorAndExit(ERROR_CODE, "Unable to find included file %s!\n", inFileName);
        }
    }

    printf("Actual file name is %s: %p\n", actualFileName, p.f);

    p.dict = parseDict;
    p.curFile = actualFileName;

    pcc_context_t *parseContext = pcc_create(&p);

    while (pcc_parse(parseContext, &translationUnit))
    {
    }

    pcc_destroy(parseContext);

    fclose(p.f);
    LinkedList_Free(p.charsRemainingPerLine, free);
    Stack_Push(parsedAsts, translationUnit);
}
