#include "input.h"
#include "util.h"
#include "tac.h" // for variableTypes

// returns 0 if export, 1 if require
char parseLinkDirection(char *directionString)
{
    if (!strcmp(directionString, "require"))
    {
        return 1;
    }
    else if (!strcmp(directionString, "export"))
    {
        return 0;
    }

    ErrorAndExit(ERROR_INTERNAL, "Linker Error - unexpected direction %s\n", directionString);
}

int getline_force_raw(char **linep, size_t *linecapp, FILE *stream)
{
    int len = getline(linep, linecapp, stream);
    if (len == 0)
    {
        ErrorAndExit(ERROR_INTERNAL, "getline_force_raw expects non-zero line length, got 0!\n");
    }
    if (len == -1)
    {
        return len;
    }

    if ((*linep)[len - 1] == '\n')
    {
        (*linep)[len - 1] = '\0';
        len--;
    }
    return len;
}

size_t getline_force(char **linep, size_t *linecapp, FILE *stream, struct Symbol *wipSymbol)
{
    size_t len = getline_force_raw(linep, linecapp, stream);
    LinkedList_Append(wipSymbol->lines, strTrim(*linep, len));
    return len;
}

size_t getline_force_metadata(char **linep, size_t *linecapp, FILE *stream, struct Symbol *wipSymbol)
{
    size_t len = getline_force_raw(linep, linecapp, stream);
    LinkedList_Append(wipSymbol->linkerLines, strTrim(*linep, len));
    return len;
}

unsigned char readHex(char *str)
{
    unsigned char byte = 0;
    for (int i = 0; i < 2; i++)
    {
        switch (str[i])
        {
        case '0':
            byte |= 0;
            break;

        case '1':
            byte |= 1;
            break;

        case '2':
            byte |= 2;
            break;

        case '3':
            byte |= 3;
            break;

        case '4':
            byte |= 4;
            break;

        case '5':
            byte |= 5;
            break;

        case '6':
            byte |= 6;
            break;

        case '7':
            byte |= 7;
            break;

        case '8':
            byte |= 8;
            break;

        case '9':
            byte |= 9;
            break;

        case 'a':
            byte |= 10;
            break;

        case 'b':
            byte |= 11;
            break;

        case 'c':
            byte |= 12;
            break;

        case 'd':
            byte |= 13;
            break;

        case 'e':
            byte |= 14;
            break;

        case 'f':
            byte |= 15;
            break;

        default:
            ErrorAndExit(ERROR_INTERNAL, "Malformed hex input for object initialization data!\nGot char '%c', expected valid hex char\n", str[i]);
        }
        // only shift on the first char
        byte <<= 4 * (1 - i);
    }
    return byte;
}

char *inBuf = NULL;
size_t bufSize = 0;

struct LinkerType *parseType(char *declString)
{
    char *lasts = NULL;

    struct LinkerType *parsed = malloc(sizeof(struct LinkerType));
    char *token = strtok_r(declString, " ", &lasts);

    if (!strcmp(token, "uint8"))
    {
        parsed->size = 1;
    }
    else if (!strcmp(token, "uint16"))
    {
        parsed->size = 2;
    }
    else if (!strcmp(token, "uint32"))
    {
        parsed->size = 4;
    }
    else if(!strcmp(token, "NOTYPE"))
    {
        parsed->size = 0;
    }
    else
    {
        ErrorAndExit(ERROR_INTERNAL, "Unexpected type string seen in parseType: [%s]\n", token);
    }

    parsed->indirectionLevel = 0;

    int tokLen = strlen(token);
    int starStartIndex = 0;

    for(int i = 0; i < tokLen; i++)
    {
        if(token[i] == '*')
        {
            starStartIndex = i;
            break;
        }
    }

    if(starStartIndex)
    {
        for(int i = starStartIndex; i < tokLen; i++)
        {
            if(token[i] == '*')
            {
                parsed->indirectionLevel++;
            }
        }
    }
    

    if (parsed->indirectionLevel)
    {
        parsed->size = 4;
    }

    return parsed;
}

void parseFunctionDeclaration(struct Symbol *wipSymbol, FILE *inFile)
{
    getline_force_metadata(&inBuf, &bufSize, inFile, wipSymbol);
    char *token = strtok(inBuf, " ");
    if (strcmp(token, "returns"))
    {
        ErrorAndExit(ERROR_INTERNAL, "Expected returns [type] but got %s instead!\n", token);
    }

    wipSymbol->data.asFunction.returnType = parseType(inBuf + strlen(token) + 1);

    getline_force_metadata(&inBuf, &bufSize, inFile, wipSymbol);
    token = strtok(inBuf, " ");

    int nArgs = atoi(token);
    wipSymbol->data.asFunction.nArgs = nArgs;

    if (nArgs > 0)
    {
        wipSymbol->data.asFunction.args = malloc(nArgs * sizeof(struct LinkerType));
    }

    token = strtok(NULL, " ");
    if (strcmp(token, "arguments"))
    {
        ErrorAndExit(ERROR_INTERNAL, "Expected '[n] arguments' but got %s %s instead!\n", inBuf, token);
    }

    while (nArgs-- > 0)
    {
        getline_force_metadata(&inBuf, &bufSize, inFile, wipSymbol);
    }
}

void parseVariable(struct Symbol *wipSymbol, FILE *inFile)
{
    getline_force_metadata(&inBuf, &bufSize, inFile, wipSymbol);
    struct LinkerType *varType = parseType(inBuf);
    wipSymbol->data.asVariable = *varType;
    free(varType);
}

void parseObject(struct Symbol *wipSymbol, FILE *inFile)
{
    getline_force_metadata(&inBuf, &bufSize, inFile, wipSymbol);

    char *token = strtok(inBuf, " ");
    if (strcmp(token, "size"))
    {
        ErrorAndExit(ERROR_INTERNAL, "Expected size [size] but got %s instead!\n", token);
    }
    token = strtok(NULL, " ");

    wipSymbol->data.asObject.size = atoi(token);

    token = strtok(NULL, " ");
    if (strcmp(token, "initialized"))
    {
        ErrorAndExit(ERROR_INTERNAL, "Expected initialized [1/0] but got %s instead!\n", token);
    }
    token = strtok(NULL, " ");
    wipSymbol->data.asObject.isInitialized = (atoi(token) == 1);

    if (wipSymbol->data.asObject.isInitialized)
    {
        size_t nRead = getline_force_metadata(&inBuf, &bufSize, inFile, wipSymbol);

        if (wipSymbol->data.asObject.size * 3 != nRead)
        {
            ErrorAndExit(ERROR_INTERNAL, "Bad formatting of byte stream for object initialization!\n");
        }
        wipSymbol->data.asObject.initializeTo = malloc(wipSymbol->data.asObject.size);
        size_t charIndex = 0;
        size_t byteIndex = 0;
        while (byteIndex < wipSymbol->data.asObject.size)
        {
            unsigned char byte = readHex(inBuf + charIndex);
            charIndex += 2;

            if (inBuf[charIndex] != ' ')
            {
                ErrorAndExit(ERROR_INTERNAL, "Malformed hex input for object initialization data!\nSaw '%c', expected ' '\n", inBuf[charIndex]);
            }
            charIndex++;
            wipSymbol->data.asObject.initializeTo[byteIndex++] = byte;
        }
    }
}
