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
#define POS_LINE 0
#define POS_COL 1
int curPos[2];

#define ROLLING_BUFFER_SIZE 4096

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
    if ((b->startPos + b->size) >= (ROLLING_BUFFER_SIZE * 2) || (b->size >= ROLLING_BUFFER_SIZE))
    {
        memcpy(b->data, b->data + b->startPos, b->size);
        // b->size = ROLLING_BUFFER_SIZE;
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

int getCharTrack(FILE *inFile)
{
    int gotten = fgetc(inFile);
    if (gotten == '\n')
    {
        curPos[POS_COL] = 0;
        curPos[POS_LINE]++;
    }
    else
    {
        curPos[POS_COL]++;
    }
    return gotten;
}

// attempt to populate the rolling buffer, return the number of characters in the buffer
int populateBuffer(struct RollingBuffer *b, FILE *inFile, FILE *outFile)
{
    while ((!feof(inFile)) && (RollingBuffer_Size(b) < longestToken))
    {
        int gotten;
        if ((gotten = getCharTrack(inFile)) != EOF)
        {
            if (gotten == '/')
            {
                gotten = getCharTrack(inFile);
                switch (gotten)
                {
                case '/':
                    do
                    {
                        gotten = getCharTrack(inFile);
                    } while ((gotten != EOF) && (gotten != '\n'));
                    break;

                case '*':
                {
                    char inBlockComment = 1;
                    do
                    {
                        gotten = getCharTrack(inFile);
                        if (gotten == '*')
                        {
                            int second_gotten = getCharTrack(inFile);
                            if (second_gotten == '/')
                            {
                                inBlockComment = 0;
                            }
                        }
                    } while ((gotten != EOF) && inBlockComment);

                    if (inBlockComment)
                    {
                        ErrorAndExit(ERROR_CODE, "Block comment does not end!\n");
                    }
                    else
                    {
                        char lineNum[64];
                        int len = snprintf(lineNum, 63, "#line %d\n", curPos[POS_LINE]);
                        for(int i = 0; i < len; i++)
                        {
                            RollingBuffer_Add(b, lineNum[i]);
                        }
                        if (gotten == EOF)
                        {
                            return EOF;
                        }
                    }
                }
                break;

                default:
                    RollingBuffer_Add(b, '/');
                    if (gotten != EOF)
                    {
                        RollingBuffer_Add(b, gotten);
                    }
                    break;
                }
            }
            else
            {
                RollingBuffer_Add(b, gotten);
            }
        }
        else
        {
            return EOF;
        }
    }
    return b->size;
}

// arguments: input file name and output file handle
// store the current working directory
// then change directory to the location of the input file
// open the input file and preprocess it, generating output to the outFile handle
// restore the old working directory
void preprocessFile(char *inFileName, char *oldInFileName, FILE *outFile)
{
    int savedPos[2];
    memcpy(savedPos, curPos, 2 * sizeof(int));
    memset(curPos, 0, 2 * sizeof(int));

    fprintf(stderr, "Preprocessing file %s\n", inFileName);
    // handle directory traversal and old CWD storage
    char *oldCWD = getcwd(NULL, 0);
    char *duped = strdup(inFileName);
    char *inFileDir = dirname(duped);
    if (chdir(inFileDir))
    {
        ErrorAndExit(ERROR_INTERNAL, "Unable to switch to directory %s\n", inFileDir);
    }

    char *justFileName = inFileName;

    // if the directory for the infile is something other than .
    if (inFileDir && strcmp(inFileDir, "."))
    {
        justFileName += strlen(inFileDir);
    }
    free(duped);

    if (justFileName[0] == '/')
    {
        justFileName++;
    }

    // open input file
    FILE *inFile = fopen(justFileName, "rb");
    if (inFile == NULL)
    {
        ErrorAndExit(ERROR_INVOCATION, "Unable to open input file %s\n", justFileName);
    }

    fprintf(outFile, "#file %s\n#line 1\n", justFileName);

    // set up the main buffer for text input from the infile
    struct RollingBuffer mainBuffer;
    RollingBuffer_Setup(&mainBuffer);

    int nCharsPopulated = 0;

    // try to grab more input for the buffer, if we can't and nothing left in buffer, we are done
    while (((nCharsPopulated = populateBuffer(&mainBuffer, inFile, outFile)) > 0) || (RollingBuffer_Size(&mainBuffer) > 0))
    {
        int whichToken = detectPreprocessorToken(&mainBuffer);

        if (whichToken == -1 && mainBuffer.size)
        {
            putc(RollingBuffer_Consume(&mainBuffer), outFile);
            continue;
        }

        switch (whichToken)
        {
        case -1:
            break;

        case 0:
        {
            struct RollingBuffer includeStrBuf;
            RollingBuffer_Setup(&includeStrBuf);

            if (populateBuffer(&mainBuffer, inFile, outFile) == EOF)
            {
                ErrorAndExit(ERROR_CODE, "Got EOF while parsing #include directive!\n");
            }

            char firstCharOfPath = RollingBuffer_Consume(&mainBuffer);
            if (firstCharOfPath != '"')
            {
                ErrorAndExit(ERROR_CODE, "Unexpected first character of #include directive: %c\n", firstCharOfPath);
            }

            char secondQuoteFound = 0;
            while (!secondQuoteFound)
            {
                if (populateBuffer(&mainBuffer, inFile, outFile) == EOF)
                {
                    ErrorAndExit(ERROR_CODE, "Got EOF while parsing #include directive!\n");
                }
                char nextCharOfPath = RollingBuffer_Consume(&mainBuffer);
                if (nextCharOfPath == '"')
                {
                    secondQuoteFound = 1;
                    break;
                }
                RollingBuffer_Add(&includeStrBuf, nextCharOfPath);
            }

            char *rawFileName = RollingBuffer_RawData(&includeStrBuf);
            int rawFileNameLen = RollingBuffer_Size(&includeStrBuf);

            char *includedFileName = malloc(rawFileNameLen + 1);
            memcpy(includedFileName, rawFileName, rawFileNameLen);
            includedFileName[rawFileNameLen] = '\0';

            preprocessFile(includedFileName, justFileName, outFile);

            free(includedFileName);
        }
        break;

        default:
            ErrorAndExit(ERROR_INTERNAL, "Invalid preprocessor token index %d\n", whichToken);
        }
    }

    if (chdir(oldCWD))
    {
        ErrorAndExit(ERROR_INTERNAL, "Unable to set working directory back to %s after processing %s\n", oldCWD, inFileName);
    }
    free(oldCWD);

    putc('\n', outFile);

    memcpy(curPos, savedPos, 2 * sizeof(int));
    if (oldInFileName)
    {
        fprintf(outFile, "#file %s\n#line %d", oldInFileName, curPos[0]);
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

    if (argc != 2 && argc != 3)
    {
        ErrorAndExit(ERROR_INTERNAL, "Usage:mpp 'infile' [outfile]\n");
    }

    FILE *outFile = stdout;

    if (argc >= 3)
    {
        if (!strcmp(argv[1], argv[2]))
        {
            ErrorAndExit(ERROR_INTERNAL, "Input and output files must be different!\n");
        }
        outFile = fopen(argv[2], "wb");
        if (outFile == NULL)
        {
            ErrorAndExit(ERROR_INVOCATION, "Unable to open output file %s\n", argv[2]);
        }
    }

    memset(curPos, 0, 2 * sizeof(int));
    preprocessFile(argv[1], NULL, outFile);
    printf("preprocessor done\n");
}