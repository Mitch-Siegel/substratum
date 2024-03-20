#include "util.h"

#ifndef _PARSER_BASE_H_
#define _PARSER_BASE_H_

void trackCharacter(struct LinkedList *charsPerLine, int c);

void manageSourceLocation(struct ParseProgress *auxil, char *matchedString, int charsConsumed, struct LinkedList *charsPerLine, unsigned int *curLineP, unsigned int *curColP);

#define UPCOMING_CHARS_THIS_LINE(auxil) (*(int *)(auxil->charsRemainingPerLine->head->data))
#define UPCOMING_CHARS_LAST_LINE(auxil) (*(int *)(auxil->charsRemainingPerLine->tail->data))

#define PCC_GETCHAR(auxil) ({                                 \
    int inChar = fgetc(auxil->f);                             \
    if (inChar == '\n')                                       \
    {                                                         \
        auxil->curLineRaw++;                                  \
        auxil->curColRaw = 0;                                 \
    }                                                         \
    else                                                      \
    {                                                         \
        auxil->curColRaw++;                                   \
    }                                                         \
    if (inChar == EOF)                                        \
    {                                                         \
        auxil->eofReceived = 1;                               \
    }                                                         \
    else                                                      \
    {                                                         \
        trackCharacter(auxil->charsRemainingPerLine, inChar); \
    }                                                         \
    inChar;                                                   \
})

/*#define PCC_DEBUG(auxil, event, rule, level, pos, buffer, length)                                                              \
    {                                                                                                                          \
        for (size_t i = 0; i < level; i++)                                                                                     \
        {                                                                                                                      \
            printf("-   ");                                                                                                    \
        }                                                                                                                      \
        printf("PCC @ %s:%d:%2d - %s:%s %lu", auxil->curFile, auxil->curLine, auxil->curCol, dbgEventNames[event], rule, pos); \
        printf("[");                                                                                                           \
        for (size_t i = 0; i < length; i++)                                                                                    \
        {                                                                                                                      \
            if (buffer[i] == '\n')                                                                                             \
            {                                                                                                                  \
                printf("\\n");                                                                                                 \
            }                                                                                                                  \
            else                                                                                                               \
            {                                                                                                                  \
                printf("%c", buffer[i]);                                                                                       \
            }                                                                                                                  \
        }                                                                                                                      \
        printf("]\n");                                                                                                         \
    }*/

#define PCC_ERROR(auxil)                                                                                    \
    {                                                                                                       \
        ErrorAndExit(ERROR_INTERNAL, "Syntax Error: %s:%d:%d\n", auxil->curFile, auxil->curLineRaw, auxil->curColRaw); \
    }

#define AST_S(original, newrightmost) AST_ConstructAddSibling(original, newrightmost)
#define AST_C(parent, child) AST_ConstructAddChild(parent, child)
#define AST_N(auxil, token, value, location)                                                                                                        \
    ({                                                                                                                                              \
        struct AST *created = NULL;                                                                                                                 \
        if (strlen(value) > 0)                                                                                                                      \
        {                                                                                                                                           \
            if (auxil->lastMatchLocation == 0)                                                                                                      \
            {                                                                                                                                       \
                manageSourceLocation(auxil, value, (location + 1) - strlen(value), auxil->charsRemainingPerLine, &auxil->curLine, &auxil->curCol);  \
                auxil->lastMatchLocation = location;                                                                                                \
            }                                                                                                                                       \
            created = AST_New(token, Dictionary_LookupOrInsert(auxil->dict, value), auxil->curFile, auxil->curLine, auxil->curCol);                 \
            manageSourceLocation(auxil, value, location - auxil->lastMatchLocation, auxil->charsRemainingPerLine, &auxil->curLine, &auxil->curCol); \
            auxil->lastMatchLocation = location;                                                                                                    \
        }                                                                                                                                           \
        else                                                                                                                                        \
        {                                                                                                                                           \
            created = AST_New(token, Dictionary_LookupOrInsert(auxil->dict, value), auxil->curFile, auxil->curLine, auxil->curCol);                 \
        }                                                                                                                                           \
        created;                                                                                                                                    \
    })

// #ifndef AST_S
// #define AST_S(original, newrightmost) ({struct AST *constructed = AST_ConstructAddSibling(original, newrightmost); printf("AST_S@ %s:%d -  %p, %p: %p\n", __FILE__, __LINE__, original, newrightmost, constructed); constructed; })
// #endif
// #ifndef AST_C
// #define AST_C(parent, child) ({struct AST *constructed = AST_ConstructAddChild(parent, child); printf("AST_C@ %s:%d - %p, %p: %p\n", __FILE__, __LINE__, parent, child, constructed); constructed; })
// #endif
// #ifndef AST_N
// #define AST_N(token, value) ({struct AST *created = AST_New(token, Dictionary_LookupOrInsert(auxil->dict, value), auxil->curFile, auxil->curLine, auxil->curCol); printf("AST_N@ %s:%d -  %s, %s: %p\n", __FILE__, __LINE__, getTokenName(token), value, created); created; })
// #endif

#endif
