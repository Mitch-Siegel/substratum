#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>

FILE *infile;

char *insTokens[] = {
    "NOP",
    "ADD", "SUB", "MUL", "DIV", "SHR", "SHL", "INC", "DEC",
    "ADDI", "SUBI", "MULI", "DIVI", "SHRI", "SHLI",
    "",
    ""};

uint8_t insOpcodes[] =
    {
        0x00,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
        0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d};

char *registerNames[] = {"%RA", "%RB", "%RC", "%RD", "%RSI", "%RDI", "%RSP", "%RBP"};

struct instruction
{
    uint8_t *data;
    int length;
    struct instruction *next;
};

struct instruction *newInstruction(int length, uint8_t opCode)
{
    struct instruction *wip = malloc(sizeof(struct instruction));
    wip->length = length;
    wip->data = malloc(length);
    wip->data[0] = opCode;
    wip->next = NULL;
    return wip;
}

struct instructionList
{
    struct instruction *first, *last;
};

void instructionListInsert(struct instructionList *theList, struct instruction *newInstruction)
{
    if (theList->first == NULL)
    {
        theList->first = newInstruction;
        theList->last = theList->first;
    }
    else
    {
        printf("inserting to tail\n");
        theList->last->next = newInstruction;
        theList->last = newInstruction;
    }
}

char lookahead()
{
    long offset = ftell(infile);
    char c = fgetc(infile);
    fseek(infile, offset, SEEK_SET);
    return c;
}

char buffer[8];
int buflen;

void scan()
{
    // check for a comment
    if (lookahead() == '#')
    {
        while (fgetc(infile) != '\n')
        {
        }
    }
    char c = fgetc(infile);
    // consume whitespace
    while (isspace(c))
    {
        c = fgetc(infile);
    }
    buflen = 0;
    // scan until next whitespace or separator
    while (!isspace(c) && c != EOF)
    {
        buffer[buflen++] = c;
        buffer[buflen] = '\0';
        c = fgetc(infile);
    }
}

uint8_t parseOpcode()
{
    scan();
    for (int i = 0; i < buflen; i++)
    {
        buffer[i] = toupper(buffer[i]);
    }
    int i = 0;
    for (i; i < 15; i++)
    {
        if (!strcmp(insTokens[i], buffer))
        {
            break;
        }
    }

    return insOpcodes[i];
}

uint8_t parseRegister()
{
    scan();
    for (int i = 0; i < buflen; i++)
    {
        buffer[i] = toupper(buffer[i]);
    }
    printf("%s\n", buffer);
    int i = -1;
    for (int j = 0; j < 8; j++)
    {
        if (!strcmp(registerNames[j], buffer))
        {
            i = j;
            break;
        }
    }
    if (i == -1)
    {
        printf("Error parsing register\n");
        exit(1);
    }
    return i;
}

uint16_t parseImmediate()
{
    scan();
    uint16_t value = 0;
    int minBuf = buflen - 9 > 0 ? buflen - 9 : 0;
    for (int i = buflen - 1; i >= minBuf; i--)
    {
        int subValue;
        subValue = buffer[i] - 48;
        for(int m = 1; m < buflen - i; m++){
            subValue *= 10;
        }
        value += subValue;
    }
    return value;
}
void parseLine(struct instructionList *workingList)
{
    printf("parsing line\n");
    int scanning = 1;
    uint8_t opCode = parseOpcode();
    uint8_t instructionLength;
    switch (opCode)
    {
    case 0x00:
        instructionLength = 1;
        break;

    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x46:
    case 0x47:
        instructionLength = 2;
        break;

    case 0x48:
    case 0x49:
    case 0x4a:
    case 0x4b:
    case 0x4c:
    case 0x4d:
        instructionLength = 4;
        break;

    default:
        printf("Error decoding instruction length\n");
        exit(1);
    }

    struct instruction *thisInstruction = newInstruction(instructionLength, opCode);
    switch (opCode)
    {
        {
        case 0x00:
            break;

        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x47:
            printf("getting registers\n");
            thisInstruction->data[1] = (parseRegister() << 4) + parseRegister();
            printf("%02x\n", thisInstruction->data[1]);
            break;

        case 0x48:
        case 0x49:
        case 0x4a:
        case 0x4b:
        case 0x4c:
        case 0x4d:
            printf("getting registers\n");
            thisInstruction->data[1] = parseRegister();
            uint16_t immediate = parseImmediate();
            thisInstruction->data[2] = immediate >> 8;
            thisInstruction->data[1] = immediate & (0x00ff);
            break;

        default:
            printf("Error decoding instruction length\n");
            exit(1);
        }
    }
    instructionListInsert(workingList, thisInstruction);
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        printf("Please provide file to assemble!\n");
        exit(1);
    }
    infile = fopen(argv[1], "rb");
    if (infile == NULL)
    {
        printf("Error opening file [%s]\n", argv[1]);
        exit(1);
    }
    fseek(infile, 0, SEEK_END);
    long eof = infile;
    fseek(infile, 0, SEEK_SET);
    //long offset = ftell(infile);
    //char returnChar = fgetc(infile);
    //fseek(infile, offset, SEEK_SET);
    struct instructionList *parsedList = malloc(sizeof(struct instructionList));

    while (!feof(infile))
    {
        parseLine(parsedList);
        for (int i = 0; i < 0xffffff; i++)
        {
        }
    }
    struct instruction* currentInstruction = parsedList->first;
    while(currentInstruction != NULL){
        for(int i = 0; i < currentInstruction->length; i++){
            printf("0x%02x ", currentInstruction->data[i]);
        }
        printf("\n");
        currentInstruction = currentInstruction->next;
    }
}