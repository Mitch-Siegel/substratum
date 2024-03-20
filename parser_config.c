#include "parser_config.h"
#include <stddef.h>

#include "parser.h"
#include "util.h"

extern struct Dictionary *parseDict;
extern struct Stack *parsedAsts;
extern struct LinkedList *includePath;

void trackCharacter(struct LinkedList *charsPerLine, int c)
{
    if (c != '\n')
    {
        (*(int *)charsPerLine->tail->data)++;
    }
    else
    {
        int *newLineChars = malloc(sizeof(int));
        *newLineChars = 0;
        LinkedList_Append(charsPerLine, newLineChars);
    };
}

void manageSourceLocation(struct ParseProgress *auxil, char *matchedString, int charsConsumed, struct LinkedList *charsPerLine, unsigned int *curLineP, unsigned int *curColP)
{
    int length = charsConsumed;
    while (length > 0)
    {
        // if we read EOF and there are no more lines to track source location with, early return
        if (auxil->eofReceived && (charsPerLine->size == 0))
        {
            return;
        }

        if ((*(int *)charsPerLine->head->data) >= length)
        {
            (*(int *)charsPerLine->head->data) -= length;
            *curColP += length;
            length = 0;
        }
        else
        {
            int *remaningCharsThisLine = LinkedList_PopFront(charsPerLine);
            length -= *remaningCharsThisLine;
            free(remaningCharsThisLine);
            *curColP = 1;
            (*curLineP)++;
        }
    }
}
