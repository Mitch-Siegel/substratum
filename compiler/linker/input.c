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

enum basicTypes parseBasicType(char *basicTypeString, int *len)
{
    if (strstr(basicTypeString, "u8"))
    {
        *len = 5;
        return vt_u8;
    }
    else if (strstr(basicTypeString, "u16"))
    {
        *len = 6;
        return vt_u16;
    }
    else if (strstr(basicTypeString, "u32"))
    {
        *len = 6;
        return vt_u32;
    }
    else if (strstr(basicTypeString, "NOTYPE"))
    {
        *len = 6;
        return vt_null;
    }
    else
    {
        ErrorAndExit(ERROR_INTERNAL, "Unexpected type string seen in parseBasicType: [%s]\n", basicTypeString);
    }
}

struct Type *parseType(FILE *inFile, char *declString, struct Symbol *wipSymbol, char canInitialize)
{
    struct Type *parsed = malloc(sizeof(struct Type));
    memset(parsed, 0, sizeof(struct Type));

    int typeNameLen = 0;
    parsed->basicType = parseBasicType(declString, &typeNameLen);

    if (parsed->basicType == vt_null)
    {
        return parsed;
    }

    char *remainingTypeInfo = declString + typeNameLen;

    if (parsed->basicType == vt_class)
    {
        ErrorAndExit(ERROR_INTERNAL, "Linker doesn't support parsing class types!\n");
    }

    while (isspace(*remainingTypeInfo))
    {
        remainingTypeInfo++;
    }

    int remainingLen = strlen(remainingTypeInfo);
    int numStars = 0;

    while (numStars < remainingLen)
    {
        if (remainingTypeInfo[numStars] == '*')
        {
            numStars++;
        }
        else
        {
            break;
        }
    }

    remainingTypeInfo += numStars;
    while (isspace(*remainingTypeInfo))
    {
        remainingTypeInfo++;
    }
    remainingLen = strlen(remainingTypeInfo);

    if (remainingLen)
    {
        if (remainingTypeInfo[0] == '[')
        {
            char *tokNumOnly = strdup(remainingTypeInfo + 1);
            for (int i = 0; i < strlen(tokNumOnly); i++)
            {
                if (tokNumOnly[i] == ']')
                {
                    tokNumOnly[i] = '\0';
                    break;
                }
            }
            parsed->arraySize = atoi(tokNumOnly);
            free(tokNumOnly);
        }
    }

    if (canInitialize)
    {
        getline_force_metadata(&inBuf, &bufSize, inFile, wipSymbol);
        if (!strcmp(inBuf, "noinitialize"))
        {
            parsed->initializeTo = NULL;
        }
        else
        {
            if (strcmp(inBuf, "initialize"))
            {
                ErrorAndExit(ERROR_INTERNAL, "Expected either 'initialize' or 'noinitialize' after type, got %s instead!\n", inBuf);
            }
            parsed->initializeTo = (char *)1;
        }
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

    wipSymbol->data.asFunction.returnType = parseType(inFile, inBuf + strlen(token) + 1, wipSymbol, 0);

    getline_force_metadata(&inBuf, &bufSize, inFile, wipSymbol);
    token = strtok(inBuf, " ");

    int nArgs = atoi(token);
    wipSymbol->data.asFunction.nArgs = nArgs;

    if (nArgs > 0)
    {
        wipSymbol->data.asFunction.args = malloc(nArgs * sizeof(struct Type));
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
    struct Type *varType = parseType(inFile, inBuf, wipSymbol, 1);
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
