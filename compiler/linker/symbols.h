#include <stdio.h>

#ifndef _SYMBOLS_H_
#define _SYMBOLS_H_
enum LinkedSymbol
{
    s_variable,
    s_function_declaration,
    s_function_definition,
    s_section,
    s_object,
    s_null,
};

enum LinkDirection
{
    export,
    require,
};

struct LinkerType
{
    char isPrimitive;
    int size;
    struct
    {
        char isStruct;
        char *name;
    } structOrUnion;
    int indirectionLevel;
};

struct FunctionDeclarationSymbol
{
    char *name;
    struct LinkerType *returnType;
    int nArgs;
    struct LinkerType *args;
};

struct Object
{
    int size;
    char *initializeTo;
    char isInitialized;
};

struct Symbol
{
    char *name;                   // string name of the symbol
    enum LinkDirection direction; // whether this symbol is exported or required from this file
    enum LinkedSymbol symbolType; // what type of symbol this is
    union
    {
        struct LinkerType asVariable;
        struct FunctionDeclarationSymbol asFunction;
        struct Object asObject;
    } data;                         // union exact details about this symbol
    struct LinkedList *lines;       // raw data of any text lines containing asm
    struct LinkedList *linkerLines; // info that is used only by the linker
    char *fromFile;                 // name of the file this symbol is from
};

struct Symbol *Symbol_New(char *name, enum LinkDirection direction, enum LinkedSymbol symbolType, char *fromFile);

void Symbol_Free(struct Symbol *s);

void Symbol_Write(struct Symbol *s, FILE *f, char outputExecutable);

enum LinkedSymbol symbolNameToEnum(char *name);

char *symbolEnumToName(enum LinkedSymbol s);

int compareSymbols(struct Symbol *a, struct Symbol *b);

struct LinkerType *parseType(char *declString);

char addRequire(struct LinkedList **exports, struct LinkedList **requires, struct Symbol *toRequire);

char addExport(struct LinkedList **exports, struct LinkedList **requires, struct Symbol *toAdd);
#endif
