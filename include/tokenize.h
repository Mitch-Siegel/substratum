#ifndef TOKENIZE_H
#define TOKENIZE_H

#include "substratum_defs.h"

struct ParseProgress;

enum RegexType {
    r_char,
    r_dot,
    r_star,
    r_end,
};

struct Regex {
    enum RegexType type;
    char *matched; // all chars matched under this type
};

struct Stack *tokenize(struct ParseProgress *progress);


#endif
