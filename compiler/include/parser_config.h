#define UPCOMING_CHARS_THIS_LINE(auxil) (*(int *)(auxil->charsRemainingPerLine->head->data))

#define PCC_GETCHAR(auxil) ({     \
    int inChar = fgetc(auxil->f); \
    inChar;                       \
})

/*if (inChar == '\n')                                                \
{                                                                  \
    UPCOMING_CHARS_THIS_LINE(auxil)                                \
    ++;                                                            \
}                                                                  \
else                                                               \
{                                                                  \
    int *newLineChars = malloc(sizeof(int));                       \
    *newLineChars = 0;                                             \
    LinkedList_Append(auxil->charsRemainingPerLine, newLineChars); \
};                                                                 \*/

/*#define PCC_DEBUG(auxil, event, rule, level, pos, buffer, length)                                           \
    {                                                                                                       \
        for (size_t i = 0; i < level; i++)                                                                  \
        {                                                                                                   \
            printf("-   ");                                                                                 \
        }                                                                                                   \
        printf("PCC @ %d:%2d - %s:%s %lu", auxil->curLine, auxil->curCol, dbgEventNames[event], rule, pos); \
        printf("[");                                                                                        \
        for (size_t i = 0; i < length; i++)                                                                 \
        {                                                                                                   \
            if (buffer[i] == '\n')                                                                          \
            {                                                                                               \
                printf("\\n");                                                                              \
            }                                                                                               \
            else                                                                                            \
            {                                                                                               \
                printf("%c", buffer[i]);                                                                    \
            }                                                                                               \
        }                                                                                                   \
        printf("]\n");                                                                                      \
    }*/

#define PCC_ERROR(auxil)                                                                              \
    {                                                                                                 \
        ErrorAndExit(ERROR_INTERNAL, "SYNTAX_ERROR_UNKNOWN: %d:%d\n", auxil->curLine, auxil->curCol); \
    }

#define AST_S(original, newrightmost) AST_ConstructAddSibling(original, newrightmost)
#define AST_C(parent, child) AST_ConstructAddChild(parent, child)
#define AST_N(auxil, token, value)                                                                                                          \
    ({                                                                                                                                      \
        struct AST *created = AST_New(token, Dictionary_LookupOrInsert(auxil->dict, value), auxil->curFile, auxil->curLine, auxil->curCol); \
        created;                                                                                                                            \
    })

/*
// printf("Match %s at %d:%d\n", value, auxil->curLine, auxil->curCol);
int length = strlen(value);                                                                                                         \
while (length > 0)                                                                                                                  \
{                                                                                                                                   \
    if (UPCOMING_CHARS_THIS_LINE(auxil) >= length)                                                                                  \
    {                                                                                                                               \
        UPCOMING_CHARS_THIS_LINE(auxil) -= length;                                                                                  \
        auxil->curCol += length;                                                                                                    \
        length = 0;                                                                                                                 \
    }                                                                                                                               \
    else                                                                                                                            \
    {                                                                                                                               \
        int *remaningCharsThisLine = LinkedList_PopFront(auxil->charsRemainingPerLine);                                             \
        length -= *remaningCharsThisLine;                                                                                           \
        free(remaningCharsThisLine);                                                                                                \
        auxil->curCol = 1;                                                                                                          \
        auxil->curLine++;                                                                                                           \
    }                                                                                                                               \
}
        */

// #ifndef AST_S
// #define AST_S(original, newrightmost) ({struct AST *constructed = AST_ConstructAddSibling(original, newrightmost); printf("AST_S@ %s:%d -  %p, %p: %p\n", __FILE__, __LINE__, original, newrightmost, constructed); constructed; })
// #endif
// #ifndef AST_C
// #define AST_C(parent, child) ({struct AST *constructed = AST_ConstructAddChild(parent, child); printf("AST_C@ %s:%d - %p, %p: %p\n", __FILE__, __LINE__, parent, child, constructed); constructed; })
// #endif
// #ifndef AST_N
// #define AST_N(token, value) ({struct AST *created = AST_New(token, Dictionary_LookupOrInsert(auxil->dict, value), auxil->curFile, auxil->curLine, auxil->curCol); printf("AST_N@ %s:%d -  %s, %s: %p\n", __FILE__, __LINE__, getTokenName(token), value, created); created; })
// #endif