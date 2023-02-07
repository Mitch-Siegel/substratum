#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ast.h"
#include "util.h"

#define BUF_SIZE 128

void ParserError();

int lookahead_char_dumb();

void trimWhitespace(char trackPos);

int lookahead_char();

enum token scan(char trackPos);

enum token lookahead();

struct AST *match(enum token t, struct Dictionary *dict);

void consume(enum token t);

char *getTokenName(enum token t);

struct AST *TableParse(struct Dictionary *dict);

struct AST *ParseProgram(char *inFileName, struct Dictionary *dict);
