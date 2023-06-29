#include "util.h"

/*
 *
 * Set up a sort of semi-rolling buffer
 * This data structure guarantees buffer contents are always sequentially accessible (no wrapping to access)
 *
 */
#define ROLLING_BUFFER_SIZE 512
char rollingData[ROLLING_BUFFER_SIZE * 2];
int rollingDataStartPos = 0;
int rollingBufferSize = 0;

#define N_PREPROCESSOR_TOKENS 1
int longestToken = 0;
char *preprocessorTokens[N_PREPROCESSOR_TOKENS] = {"#include "};
int preprocessorTokenLengths[N_PREPROCESSOR_TOKENS];

// add a character to the rolling buffer
void RollingBuffer_Add(char c)
{
    if (rollingBufferSize == ROLLING_BUFFER_SIZE)
    {
        ErrorAndExit(ERROR_INTERNAL, "Attempt to add to full rolling buffer!\n");
    }
    rollingData[rollingDataStartPos + rollingBufferSize++] = c;
    if ((rollingDataStartPos + rollingBufferSize) >= (ROLLING_BUFFER_SIZE * 2) || (rollingBufferSize == ROLLING_BUFFER_SIZE))
    {
        memcpy(rollingData, rollingData + ROLLING_BUFFER_SIZE, ROLLING_BUFFER_SIZE);
        rollingBufferSize = ROLLING_BUFFER_SIZE;
        rollingDataStartPos = 0;
    }
}

char RollingBuffer_Consume()
{
    if (rollingBufferSize < 1)
    {
        ErrorAndExit(ERROR_INTERNAL, "Attempt to consume from empty rolling buffer!\n");
    }
    char consumed = rollingData[rollingDataStartPos++];
    rollingBufferSize--;
    return consumed;
}

char *RollingBuffer_RawData()
{
    int dataStartP = rollingDataStartPos - ROLLING_BUFFER_SIZE;
    if (dataStartP < 0)
    {
        dataStartP = 0;
    }
    return &(rollingData[rollingDataStartPos + dataStartP]);
}

int RollingBuffer_Size()
{
    return rollingBufferSize;
}

// return index of preprocessor token if one found, else -1
int detectPreprocessorToken()
{
    // ensure we only scan tokens as long as the buffer's data
    // (really only relevant when we have no more data to put into the buffer)
    int maxTokenLength = RollingBuffer_Size();
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
        if (!strncmp(preprocessorTokens[i], RollingBuffer_RawData(), preprocessorTokenLengths[i]))
        {
            return i;
        }
    }
    return -1;
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

    FILE *inFile = fopen(argv[1], "rb");
    if (inFile == NULL)
    {
        ErrorAndExit(ERROR_INVOCATION, "Unable to open input file %s\n", argv[1]);
    }

    FILE *outFile = fopen(argv[2], "wb");
    if (outFile == NULL)
    {
        ErrorAndExit(ERROR_INVOCATION, "Unable to open output file %s\n", argv[2]);
    }

    int isEof;
    while ((!(isEof = feof(inFile))) || (RollingBuffer_Size() > 0))
    {
        if (!isEof)
        {
            RollingBuffer_Add(fgetc(inFile));
        }

        if (isEof || RollingBuffer_Size() >= longestToken)
        {
            int whichToken = detectPreprocessorToken();
            if (whichToken != -1)
            {
                printf("\nyeah yeah\n");
                for (int i = 0; i < preprocessorTokenLengths[whichToken]; i++)
                {
                    RollingBuffer_Consume();
                }
            }
            else
            {
                putc(RollingBuffer_Consume(), stdout);
            }
        }
    }

    putc('\n', stdout);
}