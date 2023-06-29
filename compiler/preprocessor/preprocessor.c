#include <libgen.h> // dirname/basename
#include <string.h>
#include <unistd.h> // chdir

#include "util.h"

/*
 *
 * Set up a sort of semi-rolling buffer
 * This data structure guarantees buffer contents are always sequentially accessible (no wrapping to access)
 *
 */
#define ROLLING_BUFFER_SIZE 512

struct RollingBuffer
{
    char data[ROLLING_BUFFER_SIZE * 2];
    int startPos;
    int size;
};

void RollingBuffer_Setup(struct RollingBuffer *b)
{
    b->startPos = 0;
    b->size = 0;
}

#define N_PREPROCESSOR_TOKENS 1
int longestToken = 0;
char *preprocessorTokens[N_PREPROCESSOR_TOKENS] = {"#include "};
int preprocessorTokenLengths[N_PREPROCESSOR_TOKENS];

// add a character to the rolling buffer
void RollingBuffer_Add(struct RollingBuffer *b, char c)
{
    if (b->size == ROLLING_BUFFER_SIZE)
    {
        ErrorAndExit(ERROR_INTERNAL, "Attempt to add to full rolling buffer!\n");
    }
    b->data[b->startPos + b->size++] = c;
    if ((b->startPos + b->size) >= (ROLLING_BUFFER_SIZE * 2) || (b->size == ROLLING_BUFFER_SIZE))
    {
        memcpy(b->data, b->data + ROLLING_BUFFER_SIZE, ROLLING_BUFFER_SIZE);
        b->size = ROLLING_BUFFER_SIZE;
        b->startPos = 0;
    }
}

char RollingBuffer_Consume(struct RollingBuffer *b)
{
    if (b->size < 1)
    {
        ErrorAndExit(ERROR_INTERNAL, "Attempt to consume from empty rolling buffer!\n");
    }
    char consumed = b->data[b->startPos++];
    b->size--;
    return consumed;
}

char *RollingBuffer_RawData(struct RollingBuffer *b)
{
    int dataStartP = b->startPos - ROLLING_BUFFER_SIZE;
    if (dataStartP < 0)
    {
        dataStartP = 0;
    }
    return &(b->data[b->startPos + dataStartP]);
}

int RollingBuffer_Size(struct RollingBuffer *b)
{
    return b->size;
}

// return index of preprocessor token if one found, else -1
// if a token is found, consume it from the buffer
int detectPreprocessorToken(struct RollingBuffer *b)
{
    // ensure we only scan tokens as long as the buffer's data
    // (really only relevant when we have no more data to put into the buffer)
    int maxTokenLength = RollingBuffer_Size(b);
    if (maxTokenLength >= longestToken)
    {
        maxTokenLength = longestToken;
    }

    for (int i = 0; i < N_PREPROCESSOR_TOKENS; i++)
    {
        if (preprocessorTokenLengths[i] > maxTokenLength)
        {
            continue;
        }
        if (!strncmp(preprocessorTokens[i], RollingBuffer_RawData(b), preprocessorTokenLengths[i]))
        {
            for (int j = 0; j < preprocessorTokenLengths[i]; j++)
            {
                RollingBuffer_Consume(b);
            }
            return i;
        }
    }
    return -1;
}

void populateBuffer(struct RollingBuffer *b, FILE *inFile)
{
    while ((!feof(inFile)) && (RollingBuffer_Size(b) < longestToken))
    {
        RollingBuffer_Add(b, fgetc(inFile));
    }
}

void preprocessFile(FILE *inFile, FILE *outFile)
{
    // set up the main buffer for text input from the infile
    struct RollingBuffer mainBuffer;
    RollingBuffer_Setup(&mainBuffer);

    while (!feof(inFile) || (RollingBuffer_Size(&mainBuffer) > 0))
    {
        populateBuffer(&mainBuffer, inFile);

        int whichToken = detectPreprocessorToken(&mainBuffer);

        if (whichToken == -1)
        {
            putc(RollingBuffer_Consume(&mainBuffer), stdout);
            continue;
        }

        switch (whichToken)
        {
        case 0:
        {
            struct RollingBuffer includeStrBuf;
            RollingBuffer_Setup(&includeStrBuf);

            populateBuffer(&mainBuffer, inFile);

            char firstCharOfPath = RollingBuffer_Consume(&mainBuffer);
            if (firstCharOfPath != '"')
            {
                ErrorAndExit(ERROR_CODE, "Unexpected first character of #include directive: %c\n", firstCharOfPath);
            }

            char secondQuoteFound = 0;
            while (!secondQuoteFound)
            {
                populateBuffer(&mainBuffer, inFile);
                char nextCharOfPath = RollingBuffer_Consume(&mainBuffer);
                if (nextCharOfPath == '"')
                {
                    secondQuoteFound = 1;
                    break;
                }
                RollingBuffer_Add(&includeStrBuf, nextCharOfPath);
            }

            printf("%s\n", RollingBuffer_RawData(&includeStrBuf));

            char *rawFileName = RollingBuffer_RawData(&includeStrBuf);
            int rawFileNameLen = RollingBuffer_Size(&includeStrBuf);
            char *includedFileName = malloc(rawFileNameLen + 1);
            memcpy(includedFileName, rawFileName, rawFileNameLen);
            includedFileName[rawFileNameLen] = '\0';
            FILE *includedFile = fopen(includedFileName, "rb");
            if (includedFile == NULL)
            {
                ErrorAndExit(ERROR_INVOCATION, "Unable to open included file %s\n", includedFileName);
            }
            free(includedFileName);
            preprocessFile(includedFile, outFile);
        }
        break;

        default:
            ErrorAndExit(ERROR_INTERNAL, "Invalid preprocessor token index %d\n", whichToken);
        }
    }
}

int main(int argc, char **argv)
{
    for (int i = 0; i < N_PREPROCESSOR_TOKENS; i++)
    {
        preprocessorTokenLengths[i] = strlen(preprocessorTokens[i]);
        if (preprocessorTokenLengths[i] > longestToken)
        {
            longestToken = preprocessorTokenLengths[i];
        }
    }

    if (argc != 3)
    {
        ErrorAndExit(ERROR_INTERNAL, "Usage: mld 'infile' 'outfile'\n");
    }

    if (!strcmp(argv[1], argv[2]))
    {
        ErrorAndExit(ERROR_INVOCATION, "Input and output files must be different!\n");
    }

    char *inFileDir = dirname(strdup(argv[1]));
    if(chdir(inFileDir))
    {
        ErrorAndExit(ERROR_INTERNAL, "Unable to switch to directory %s\n", inFileDir);
    }

    char *justFileName = argv[1] + strlen(inFileDir);
    if(justFileName[0] == '/')
    {
        justFileName++;
    }

    // open base input and output files
    FILE *inFile = fopen(justFileName, "rb");
    if (inFile == NULL)
    {
        ErrorAndExit(ERROR_INVOCATION, "Unable to open input file %s\n", justFileName);
    }
    free(inFileDir);

    FILE *outFile = fopen(argv[2], "wb");
    if (outFile == NULL)
    {
        ErrorAndExit(ERROR_INVOCATION, "Unable to open output file %s\n", argv[2]);
    }

        preprocessFile(inFile, outFile);

    putc('\n', stdout);
}