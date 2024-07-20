#include "parser_config.h"
#include <stddef.h>

#include "log.h"
#include "parser.h"
#include "util.h"

#include "mbcl/list.h"
#include "mbcl/stack.h"

extern struct Dictionary *parseDict;
extern Stack *parsedAsts;
extern List *includePath;

void print_chars_per_line(List *charsRemaining)
{
    Iterator *charRunner = NULL;
    for (charRunner = list_begin(charsRemaining); iterator_gettable(charRunner); iterator_next(charRunner))
    {
        size_t *charsRem = iterator_get(charRunner);
        log(LOG_WARNING, "%zu chars in this line", *charsRem);
    }
}

void track_character(struct ParseProgress *auxil, int trackedChar)
{
    List *charsPerLine = auxil->charsRemainingPerLine;
    CHARS_LAST_LINE(auxil) += 1;
    if (trackedChar == '\n')
    {
        size_t *newLineChars = malloc(sizeof(size_t));
        *newLineChars = 0;
        list_append(charsPerLine, newLineChars);
    }
}

void manage_location(struct ParseProgress *auxil, char *matchedString, bool isSourceLocation)
{
    while (*matchedString)
    {
        if (auxil->charsRemainingPerLine->size == 0)
        {
            print_chars_per_line(auxil->charsRemainingPerLine);
            InternalError("bad line/col track at %s:%zu:%zu - no charsRemainingPerLine", auxil->curFile, auxil->curLine, auxil->curCol);
            return;
        }

        if (*matchedString == '\n')
        {
            if (CHARS_THIS_LINE(auxil) != 1)
            {
                print_chars_per_line(auxil->charsRemainingPerLine);
                InternalError("Bad line/col track at %s:%zu:%zu - saw \\n but %zu chars, %zu lines remaining", auxil->curFile, auxil->curLine, auxil->curCol, (*(size_t *)auxil->charsRemainingPerLine->head->data), auxil->charsRemainingPerLine->size);
            }
            free(list_pop_front(auxil->charsRemainingPerLine));

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
                print_chars_per_line(auxil->charsRemainingPerLine);
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

void parser_error(struct ParseProgress *auxil)
{
    InternalError("Syntax Error between %s:%zu:%zu and %s:%zu", auxil->curFile, auxil->curLine, auxil->curCol, auxil->curFile, auxil->curLine + auxil->charsRemainingPerLine->size);
}

void set_current_file(struct ParseProgress *auxil, char *preprocessorLine, u32 lineNum, char *fileName)
{

    log(LOG_DEBUG, "set current file to %s:%zu", auxil->curFile, auxil->curLine);

    if ((auxil->charsRemainingPerLine->size > 0) && (CHARS_THIS_LINE(auxil) > 1))
    {
        print_chars_per_line(auxil->charsRemainingPerLine);
        InternalError("Bad line/col track at line %s:%zu:%zu - changing file to %s:%zu but %zu chars, %zu lines remaining - directive line [%s]\n",
                      auxil->curFile,
                      auxil->curLine,
                      auxil->curCol,
                      fileName,
                      lineNum,
                      (*(size_t *)auxil->charsRemainingPerLine->head->data), auxil->charsRemainingPerLine->size,
                      preprocessorLine);
    }

    auxil->curFile = dictionary_lookup_or_insert(parseDict, fileName);
    auxil->curLine = lineNum;
    auxil->curCol = 1;
}
