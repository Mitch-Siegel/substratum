#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ast.h"
#include "util.h"

#define BUF_SIZE 128


int lookahead_char_dumb();

void trimWhitespace(char trackPos);

// scan and return the raw token we see
enum token _scan(char trackPos);

// wrapper around _scan
// handles #file and #line directives in order to correctly track position in preprocessed files
enum token scan(char trackPos, struct Dictionary *dict);

enum token lookahead();

struct AST *match(enum token t, struct Dictionary *dict);

void consume(enum token t, struct Dictionary *dict);

char *getTokenName(enum token t);

struct AST *TableParse(struct Dictionary *dict);

struct AST *ParseProgram(char *inFileName, struct Dictionary *dict);
