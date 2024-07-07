#ifndef PARSER_BASE_H
#define PARSER_BASE_H

#include "substratum_defs.h"
struct LinkedList;
struct ParseProgress;

void track_character(struct ParseProgress *auxil, int trackedCharacter);

void manage_location(struct ParseProgress *auxil, char *matchedString, bool isSourceLocation);

#define manage_source_location(auxil, matchedString) manage_location(auxil, matchedString, true)
#define manage_non_source_location(auxil, matchedString) manage_location(auxil, matchedString, false)

void parser_error(struct ParseProgress *auxil);

void set_current_file(struct ParseProgress *auxil, char *preprocessorLine, u32 lineNum, char *fileName);

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
        track_character(auxil, (inChar)); \
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
        parser_error(auxil);                                                \
    }

#define AST_S(original, newrightmost) ast_construct_add_sibling(original, newrightmost)
#define AST_C(parent, child) ast_construct_add_child(parent, child)
#define AST_N(auxil, token, value, location)                                                                                                            \
    ({                                                                                                                                                  \
        struct Ast *created = ast_new(token, dictionary_lookup_or_insert((auxil)->dict, (value)), (auxil)->curFile, (auxil)->curLine, (auxil)->curCol); \
        manage_source_location(auxil, value);                                                                                                           \
        created;                                                                                                                                        \
    })

#endif
