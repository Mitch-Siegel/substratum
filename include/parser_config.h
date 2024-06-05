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

#endif
