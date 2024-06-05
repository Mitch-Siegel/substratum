#include "parser_config.h"
#include <stddef.h>

#include "log.h"
#include "parser.h"
#include "util.h"

extern struct Dictionary *parseDict;
extern struct Stack *parsedAsts;
extern struct LinkedList *includePath;

void printCharsPerLine(struct LinkedList *charsRemaining)
{
    for (struct LinkedListNode *runner = charsRemaining->head; runner != NULL; runner = runner->next)
    {
        size_t *charsRem = runner->data;
        Log(LOG_WARNING, "%zu chars in this line", *charsRem);
    }
}

void trackCharacter(struct ParseProgress *auxil, int trackedChar)
{
    struct LinkedList *charsPerLine = auxil->charsRemainingPerLine;
    CHARS_LAST_LINE(auxil) += 1;
    if (trackedChar == '\n')
    {
        size_t *newLineChars = malloc(sizeof(size_t));
        *newLineChars = 0;
        LinkedList_Append(charsPerLine, newLineChars);
    }
}

void manageLocation(struct ParseProgress *auxil, char *matchedString, bool isSourceLocation)
{
    while (*matchedString)
    {
        if (auxil->charsRemainingPerLine->size == 0)
        {
            printCharsPerLine(auxil->charsRemainingPerLine);
            InternalError("bad line/col track at %s:%zu:%zu - no charsRemainingPerLine", auxil->curFile, auxil->curLine, auxil->curCol);
            return;
        }

        if (*matchedString == '\n')
        {
            if (CHARS_THIS_LINE(auxil) != 1)
            {
                printCharsPerLine(auxil->charsRemainingPerLine);
                InternalError("Bad line/col track at %s:%zu:%zu - saw \\n but %zu chars, %zu lines remaining", auxil->curFile, auxil->curLine, auxil->curCol, (*(size_t *)auxil->charsRemainingPerLine->head->data), auxil->charsRemainingPerLine->size);
            }
            free(LinkedList_PopFront(auxil->charsRemainingPerLine));

            if (isSourceLocation)
            {
                auxil->curCol = 1;
                auxil->curLine++;
            }
        }
        else
        {
            if (CHARS_THIS_LINE(auxil) == 0)
            {
                printCharsPerLine(auxil->charsRemainingPerLine);
                InternalError("Bad line/col track at %s:%zu:%zu - saw %c but %zu chars, %zu lines remaining", auxil->curFile, auxil->curLine, auxil->curCol, *matchedString, (*(size_t *)auxil->charsRemainingPerLine->head->data), auxil->charsRemainingPerLine->size);
            }
            CHARS_THIS_LINE(auxil) -= 1;
            if (isSourceLocation)
            {
                auxil->curCol++;
            }
        }

        matchedString++;
    }
}

void parserError(struct ParseProgress *auxil)
{
    InternalError("Syntax Error between %s:%zu:%zu and %s:%zu", auxil->curFile, auxil->curLine, auxil->curCol, auxil->curFile, auxil->curLine + auxil->charsRemainingPerLine->size);
}

void setCurrentFile(struct ParseProgress *auxil, char *preprocessorLine, u32 lineNum, char *fileName)
{

    Log(LOG_DEBUG, "set current file to %s:%zu", auxil->curFile, auxil->curLine);

    if ((auxil->charsRemainingPerLine->size > 0) && (CHARS_THIS_LINE(auxil) > 1))
    {
        printCharsPerLine(auxil->charsRemainingPerLine);
        InternalError("Bad line/col track at line %s:%zu:%zu - changing file to %s:%zu but %zu chars, %zu lines remaining - directive line [%s]\n",
                      auxil->curFile,
                      auxil->curLine,
                      auxil->curCol,
                      fileName,
                      lineNum,
                      (*(size_t *)auxil->charsRemainingPerLine->head->data), auxil->charsRemainingPerLine->size,
                      preprocessorLine);
    }

    auxil->curFile = Dictionary_LookupOrInsert(parseDict, fileName);
    auxil->curLine = lineNum;
    auxil->curCol = 1;
}
