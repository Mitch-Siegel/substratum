#include "tokenize.h"

#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "log.h"
#include "util.h"

char *RegexTypeNames[r_end + 1] = {
    [r_char] = "char",
    [r_dot] = "dot",
    [r_star] = "star",
    [r_end] = "END",
};

void Regex_Print(struct Regex *expr)
{
    while (expr->type != r_end)
    {
        Log(LOG_WARNING, "%s %s", expr->matched, RegexTypeNames[expr->type]);

        expr++;
    }
}

void regexMatchAppend(char **matched, char toAdd)
{
    size_t oldLen = 0;
    if (*matched != NULL)
    {
        oldLen = strlen(*matched);
    }
    *matched = realloc(*matched, oldLen + 3);
    (*matched)[oldLen] = toAdd;
    (*matched)[oldLen + 1] = '\0';
}

struct Regex *nextExpr(struct Regex *arr, size_t *alloc, size_t *idx)
{
    if (((*idx) + 1) >= *alloc)
    {
        arr = realloc(arr, (*alloc + 1) * sizeof(struct Regex));
        memset(arr + *alloc, 0, sizeof(struct Regex));
        *alloc += 1;
    }
    (*idx)++;
    return arr;
}

struct Regex *parseRegex(char *expression)
{
    struct Regex *parsed = malloc(sizeof(struct Regex));
    memset(parsed, 0, sizeof(struct Regex));
    size_t exprAlloc = 1;
    size_t exprIdx = 0;

    while (*expression)
    {
        Log(LOG_WARNING, "at expr idx %zu - alloc %zu", exprIdx, exprAlloc);
        switch (*expression)
        {
        case '.':
            parsed[exprIdx].type = r_dot;
            break;

        case '*':
            exprIdx--;
            parsed[exprIdx].type = r_star;
            break;

        case '[':
        {
            expression++;
            while ((*expression) && (*expression != ']'))
            {
                regexMatchAppend(&parsed[exprIdx].matched, *expression);
                expression++;
            }
        }
        break;

        default:
            parsed[exprIdx].type = r_char;
            regexMatchAppend(&parsed[exprIdx].matched, *expression);
            break;
        }

        expression++;

        parsed = nextExpr(parsed, &exprAlloc, &exprIdx);
    }

    if (exprIdx == exprAlloc)
    {
        exprAlloc++;
        parsed = realloc(parsed, exprAlloc * sizeof(struct Regex));
    }
    parsed[exprIdx++].type = r_end;

    return parsed;
}

bool searchCharClass(char *class, char c)
{
    while ((*class))
    {
        if (*class == c)
        {
            return true;
        }
        class ++;
    }
    return false;
}

bool matchRegex(struct Regex *expression, char *string)
{
    bool matched = 1;
    while ((expression->type != r_end) && (*string))
    {
        switch (expression->type)
        {
        case r_char:
            if (!searchCharClass(expression->matched, *string))
            {
                return false;
            }
            string++;
            break;

        case r_dot:
            if (*string == '\0')
            {
                printf("fail dot\n");
                return false;
            }
            string++;
            break;

        case r_star:
        {
            while (searchCharClass(expression->matched, *string))
            {
                string++;
            }
        }
        break;

        case r_end:
            break;
        }

        expression++;
    }

    if(*string)
    {
        matched = false;
    }

    return matched;
}

const size_t MAX_TOKEN_LENGTH = 256;

struct TokenBuffer
{
    char *data;
    size_t size;
    size_t maxSize;
};

void tokBuf_Add(struct TokenBuffer *tokBuf, char toAdd)
{
    tokBuf->data[tokBuf->size] = toAdd;
    tokBuf->size++;

    if (tokBuf->size > MAX_TOKEN_LENGTH)
    {
        InternalError("Internal limit of token buffer exceeded");
    }
}

void tokBuf_Remove(struct TokenBuffer *tokBuf, size_t size)
{
    size_t newSize = tokBuf->size - size;
    memmove(tokBuf->data, tokBuf->data + size, newSize);
    tokBuf->size = newSize;
}

struct AST *tokBuf_Match(struct ParseProgress *progress, struct TokenBuffer *tokBuf)
{
    char cpyTo[4];
    cpyTo[3] = '\0';
    memcpy(cpyTo, tokBuf->data, 3);
    tokBuf_Remove(tokBuf, 3);

    return AST_New(t_identifier, Dictionary_LookupOrInsert(progress->dict, cpyTo), "asdf", 123, 234);
}

struct Stack *tokenize(struct ParseProgress *progress)
{
    struct Stack *tokens = Stack_New();

    struct Regex *re = parseRegex("a.bcd*[efg]*h*");
    Regex_Print(re);

    char *tryMatch = "a!bceeefffeeeffgggeeegggfff";
    printf("%s matches? %d\n", tryMatch, matchRegex(re, tryMatch));

    // struct TokenBuffer tokBuf = {malloc(MAX_TOKEN_LENGTH + 1), 0, MAX_TOKEN_LENGTH};

    // while (!feof(progress->f))
    // {
    //     if (tokBuf.size < MAX_TOKEN_LENGTH)
    //     {
    //         tokBuf_Add(&tokBuf, fgetc(progress->f));
    //     }
    //     else
    //     {
    //         Stack_Push(tokens, tokBuf_Match(progress, &tokBuf));
    //     }
    //     // printf("%c", c);
    // }

    // free(tokBuf.data);

    return tokens;
}
