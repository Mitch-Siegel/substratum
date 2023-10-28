#include <stdlib.h>
#include <stdio.h>

#include "symbols.h"

#ifndef _INPUT_H_
#define _INPUT_H_

extern char *inBuf;
extern size_t bufSize;

char parseLinkDirection(char *directionString);

int getline_force_raw(char **linep, size_t *linecapp, FILE *stream);

size_t getline_force_metadata(char **linep, size_t *linecapp, FILE *stream, struct Symbol *wipSymbol);

void parseFunctionDeclaration(struct Symbol *wipSymbol, FILE *inFile);

void parseVariable(struct Symbol *wipSymbol, FILE *inFile);

void parseObject(struct Symbol *wipSymbol, FILE *inFile);

#endif
