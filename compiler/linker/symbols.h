enum LinkedSymbol
{
    s_variable,
    s_function_declaration,
    s_function_definition,
    s_section,
    s_null,
};

enum LinkDirection
{
    export,
    require,
};

struct VariableSymbol
{
    int type;
    int indirectionLevel;
};

struct Type
{
    char isPrimitive;
    union
    {
        int primitive;
        struct
        {
            char isStruct;
            char *name;
        } structOrUnion;
    } data;
    int indirectionLevel;
};

struct FunctionDeclarationSymbol
{
    char *name;
    struct Type *returnType;
    int nArgs;
    struct Type *args;
};

struct Symbol
{
    char *name;                   // string name of the symbol
    enum LinkDirection direction; // whether this symbol is exported or required from this file
    enum LinkedSymbol symbolType; // what type of symbol this is
    union
    {
        struct VariableSymbol asVariable;
        struct FunctionDeclarationSymbol asFunction;
    } data;                         // union exact details about this symbol
    struct LinkedList *lines;       // raw data of any text lines containing asm
    struct LinkedList *linkerLines; // info that is used only by the linker
};

struct Symbol *Symbol_New(char *name, enum LinkDirection direction, enum LinkedSymbol symbolType);

void Symbol_Free(struct Symbol *s);

enum LinkedSymbol symbolNameToEnum(char *name);

char *symbolEnumToName(enum LinkedSymbol s);

int compareSymbols(struct Symbol *a, struct Symbol *b);

struct Type *parseType(char *declString);
