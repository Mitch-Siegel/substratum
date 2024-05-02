#include "parser_config.h"
#include <stddef.h>

#include "log.h"
#include "parser.h"
#include "util.h"

extern struct Dictionary *parseDict;
extern struct Stack *parsedAsts;
extern struct LinkedList *includePath;

void trackCharacter(struct LinkedList *charsPerLine, int trackedChar)
{
    if (trackedChar != '\n')
    {
        if (charsPerLine->size == 0)
        {
            size_t *zeroCharLine = malloc(sizeof(size_t));
            *zeroCharLine = 0;
            LinkedList_Append(charsPerLine, zeroCharLine);
        }

        (*(size_t *)charsPerLine->tail->data)++;
    }
    else
    {
        size_t *newLineChars = malloc(sizeof(size_t));
        *newLineChars = 0;
        LinkedList_Append(charsPerLine, newLineChars);
    };
}

void manageSourceLocation(struct ParseProgress *auxil, char *matchedString, size_t charsConsumed, struct LinkedList *charsPerLine, size_t *curLineP, size_t *curColP)
{
    size_t length = charsConsumed;
    while (length > 0)
    {
        // if we read EOF and there are no more lines to track source location with, early return
        if (auxil->eofReceived || (charsPerLine->size == 0))
        {
            return;
        }

        if ((*(size_t *)charsPerLine->head->data) >= length)
        {
            (*(size_t *)charsPerLine->head->data) -= length;
            *curColP += length;
            length = 0;
        }
        else
        {
            size_t *remaningCharsThisLine = LinkedList_PopFront(charsPerLine);
            length -= *remaningCharsThisLine;
            free(remaningCharsThisLine);
            *curColP = 1;
            (*curLineP)++;
        }
    }
}

void parserError(struct ParseProgress *auxil)
{
    InternalError("Syntax Error between %s:%zu:%zu and %zu", auxil->curFile, auxil->curLine, auxil->curCol, auxil->curLine + auxil->charsRemainingPerLine->size);
}

void setCurrentFile(char **curFileP, char *fileName)
{
    *curFileP = Dictionary_LookupOrInsert(parseDict, fileName);
}
