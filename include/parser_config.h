#ifndef PARSER_BASE_H
#define PARSER_BASE_H

#include "substratum_defs.h"
struct LinkedList;
struct ParseProgress;

void trackCharacter(struct ParseProgress *auxil, int trackedCharacter);

void manageLocation(struct ParseProgress *auxil, char *matchedString, bool isSourceLocation);

#define manageSourceLocation(auxil, matchedString) manageLocation(auxil, matchedString, true)
#define manageNonSourceLocation(auxil, matchedString) manageLocation(auxil, matchedString, false)

void parserError(struct ParseProgress *auxil);

void setCurrentFile(struct ParseProgress *auxil, char *preprocessorLine, u32 lineNum, char *fileName);

#define CHARS_THIS_LINE(auxil) (*(size_t *)((auxil)->charsRemainingPerLine->head->data))
#define CHARS_LAST_LINE(auxil) (*(size_t *)((auxil)->charsRemainingPerLine->tail->data))

#define PCC_GETCHAR(auxil) ({            \
    int inChar = fgetc((auxil)->f);      \
    printf("%c", inChar);                \
    if ((inChar) == EOF)                 \
    {                                    \
        (auxil)->eofReceived = 1;        \
    }                                    \
    else                                 \
    {                                    \
        trackCharacter(auxil, (inChar)); \
    }                                    \
    (inChar);                            \
})

/*#define PCC_DEBUG(auxil, event, rule, level, pos, buffer, length)     \
    {                                                                 \
        if (event == PCC_DBG_MATCH)                                   \
        {                                                             \
            printf("\nmatch %s : [%.*s]\n", rule, (int)length, buffer); \
        }                                                             \
    }*/

/*#define PCC_DEBUG(auxil, event, rule, level, pos, buffer, length)                                                              \
    {                                                                                                                          \
        for (size_t i = 0; i < level; i++)                                                                                     \
        {                                                                                                                      \
            printf("-   ");                                                                                                    \
        }                                                                                                                      \
        printf("PCC @ %s:%zu:%2zu - %s:%s %zu", auxil->curFile, auxil->curLine, auxil->curCol, dbgEventNames[event], rule, pos); \
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

#define PCC_ERROR(auxil)                                                   \
    {                                                                      \
        if ((ctx != NULL) && (ctx->buffer.len > 0))                        \
        {                                                                  \
            int nNlFound = 0;                                              \
            ssize_t i = 0;                                                 \
            for (i = ctx->buffer.len - 1; (i >= 0) && (nNlFound < 3); i--) \
            {                                                              \
                if (ctx->buffer.buf[i] == '\n')                            \
                {                                                          \
                    nNlFound++;                                            \
                }                                                          \
            }                                                              \
            if (nNlFound == 3)                                             \
            {                                                              \
                i += 2;                                                    \
            }                                                              \
            fputs("Syntax error near:\n", stderr);                         \
            while (i < ctx->buffer.len)                                    \
            {                                                              \
                fputc(ctx->buffer.buf[i], stderr);                         \
                i++;                                                       \
            }                                                              \
            fputc('\n', stderr);                                           \
        }                                                                  \
        parserError(auxil);                                                \
    }

#define AST_S(original, newrightmost) AST_ConstructAddSibling(original, newrightmost)
#define AST_C(parent, child) AST_ConstructAddChild(parent, child)
#define AST_N(auxil, token, value, location)                                                                                                          \
    ({                                                                                                                                                \
        struct AST *created = AST_New(token, Dictionary_LookupOrInsert((auxil)->dict, (value)), (auxil)->curFile, (auxil)->curLine, (auxil)->curCol); \
        manageSourceLocation(auxil, value);                                                                                                           \
        created;                                                                                                                                      \
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
