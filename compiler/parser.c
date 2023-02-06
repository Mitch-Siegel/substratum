#include "parser.h"
#include "parseRecipes.h"

FILE *inFile;
char buffer[BUF_SIZE];
int buflen;
int curLine, curCol;
char inChar;
char *token_names[] = {
	"asm",
	"uint8",
	"uint16",
	"uint32",
	"fun",
	"return",
	"if",
	"else",
	"while",
	"do",
	"name",
	"literal",
	"binary add",
	"binary sub",
	"binary less than",
	"binary greater than",
	"binary less than or equal",
	"binary greater than or equal",
	"binary equals",
	"binary not equals",
	"binary logical and",
	"binary logical or",
	"unary logical not",
	"unary add and assign",
	"unary sub and assign"
	"reference operator",
	"dereference operator",
	"assignment",
	"comma",
	"semicolon",
	"l paren",
	"r paren",
	"l curly",
	"r curly",
	"l bracket",
	"r bracket",
	"array",
	"call",
	"scope",
	"EOF"};

#define ParserError(production, info)                                                 \
	{                                                                                 \
		printf("Error while parsing %s\n", production);                               \
		printf("Error at line %d, col %d\n", curLine, curCol);                        \
		printf("%s\n", info);                                                         \
		ErrorAndExit(ERROR_CODE, "Parse buffer when error occurred: [%s]\n", buffer); \
	}

// return the char 'count' characters ahead
// count must be >0, returns null char otherwise
char lookahead_char_dumb(int count)
{
	long offset = ftell(inFile);
	char returnChar = '\0';
	for (int i = 0; i < count; i++)
	{
		returnChar = fgetc(inFile);
	}
	fseek(inFile, offset, SEEK_SET);
	return returnChar;
}

void trimWhitespace(char trackPos)
{
	int trimming = 1;
	while (trimming)
	{
		switch (lookahead_char_dumb(1))
		{
		case '\n':
			if (trackPos)
			{
				curLine++;
				curCol = 1;
			}
			fgetc(inFile);
			break;

		case -1:
		case '\0':
		case ' ':
		case '\t':
			if (trackPos)
				curCol++;

			fgetc(inFile);
			break;

		case '/':
			switch (lookahead_char_dumb(2))
			{
				// single line comment
			case '/':
				while (fgetc(inFile) != '\n')
					;

				if (trackPos)
				{
					curLine++;
					curCol = 1;
				}
				break;

			case '*':
				// skip the comment opener, begin reading the comment
				fgetc(inFile);
				fgetc(inFile);

				char inBlockComment = 1;
				while (inBlockComment)
				{
					switch (fgetc(inFile))
					{
						// look ahead if we see something that could be related to a block comment
					case '*':
						if (lookahead_char_dumb(1) == '/')
							inBlockComment = 0;

						break;

						// disallow nesting of block comments
					case '/':
						if (lookahead_char_dumb(1) == '*')
							ParserError("comment", "Error - nested block comments not allowed");

						break;

						// otherwise just track position in the file if necessary
					case '\n':
						if (trackPos)
						{
							curLine++;
							curCol = 1;
							break;
						}
						break;

					default:
						if (trackPos)
							curCol += 1;

						break;
					}
				}
				// catch the trailing slash of the comment closer
				fgetc(inFile);
				break;
			}

			break;

		default:
			trimming = 0;
			break;
		}
	}
}

char lookahead_char()
{
	trimWhitespace(1);
	char r = lookahead_char_dumb(1);
	return r;
}

#define RESERVED_COUNT 34

char *reserved[RESERVED_COUNT] = {
	"asm",	  // t_asm,
	"uint8",  // 	t_uint8,
	"uint16", // 	t_uint16,
	"uint32", // 	t_uint32,
	"fun",	  // 	t_fun,
	"return", // 	t_return,
	"if",	  // 	t_if,
	"else",	  // 	t_else,
	"while",  // 	t_while,
	",",	  // 	t_comma,
	"(",	  // 	t_lParen,
	")",	  // 	t_rParen,
	"{",	  // 	t_lCurly,
	"}",	  // 	t_rCurly,
	"[",	  // 	t_lBracket,
	"]",	  // 	t_rBracket,
	";",	  // 	t_semicolon,
	"=",	  // 	t_assign,
	"+",	  // 	t_un_add,
	"+=",	  // 	t_add_assign,
	"-",	  // 	t_un_sub,
	"-=",	  // 	t_un_sub_assign,
	"*",	  // 	t_dereference,
	"&",	  // 	t_reference,
	">",	  // 	t_bin_gThan,
	"<",	  // 	t_bin_lThan,
	">=",	  // 	t_bin_gThanE,
	"<=",	  // 	t_bin_lThanE,
	"==",	  // 	t_bin_equals,
	"!=",	  // 	t_bin_notEquals,
	"&&",	  // 	t_bin_log_and,
	"||",	  // 	t_bin_log_or,
	"!",	  // 	t_un_log_not,
	"$$"};	  // 	t_EOF

enum token reserved_tokens[RESERVED_COUNT] = {
	t_asm,
	t_uint8,
	t_uint16,
	t_uint32,
	t_fun,
	t_return,
	t_if,
	t_else,
	t_while,
	t_comma,
	t_lParen,
	t_rParen,
	t_lCurly,
	t_rCurly,
	t_lBracket,
	t_rBracket,
	t_semicolon,
	t_assign,
	t_un_add,
	t_add_assign,
	t_un_sub,
	t_sub_assign,
	t_dereference,
	t_reference,
	t_bin_gThan,
	t_bin_lThan,
	t_bin_gThanE,
	t_bin_lThanE,
	t_bin_equals,
	t_bin_notEquals,
	t_bin_log_and,
	t_bin_log_or,
	t_un_log_not,
	t_EOF};

enum token scan(char trackPos)
{
	buflen = 0;
	// check if we're looking at whitespace - are we?
	trimWhitespace(trackPos);
	if (feof(inFile))
		return t_EOF;

	enum token currentToken = -1;

	while (1)
	{
		inChar = fgetc(inFile);
		if (trackPos)
			curCol++;

		buffer[buflen++] = inChar;
		buffer[buflen] = '\0';
		if (feof(inFile))
			return currentToken;

		// Iterate all reserved keywords
		for (int i = 0; i < RESERVED_COUNT; i++)
		{
			// if we match a reserved keyword
			if (!strcmp(buffer, reserved[i]))
			{
				// allow catching both '<', '>', '=', and '<=', '>=', '=='
				if (buffer[0] == '<' || buffer[0] == '>' || buffer[0] == '=' || buffer[0] == '!' || buffer[0] == '+' || buffer[0] == '-')
				{
					if (lookahead_char() != '=')
						return reserved_tokens[i];
				}
				else if ((buffer[0] == '&') && (buflen == 1))
				{
					if (lookahead_char() == '&')
					{
						continue;
					}
				}
				else
				{
					return reserved_tokens[i]; // return its token
				}
			}
		}

		// if on the first char of the token
		if (buflen == 1)
		{
			// determine literal or name based on what we started with
			if (isdigit(inChar))
				currentToken = t_constant;
			else if (isalpha(inChar))
				currentToken = t_identifier;
		}
		else
		{
			// simple error checking for letters in literals
			if (currentToken == t_constant && isalpha(inChar))
				ParserError("literal", "Error - alphabetical character in literal!");
		}

		// if the next input char is whitespace or a single-character token, we're done with this token
		switch (lookahead_char_dumb(1))
		{
		case ' ':
		case ',':
		case '(':
		case ')':
		case '{':
		case '}':
		case '[':
		case ']':
		case ';':
		case '\n':
		case '\t':
		case '+':
		case '-':
		case '*':
		case '/':
			return currentToken;
			break;
		}
	}
}

// return the next token that would be scanned without consuming
enum token lookahead()
{
	long offset = ftell(inFile);
	enum token retToken = scan(0);
	fseek(inFile, offset, SEEK_SET);
	return retToken;
}

// error-checked method to consume and return AST node of expected token
struct AST *match(enum token t, struct Dictionary *dict)
{
	int line = curLine;
	int col = curCol;
	enum token result = scan(1);
	if (result != t)
	{
		printf("Expected token %s, got %s\n", token_names[t], token_names[result]);
		ParserError("match", "Error matching a token!");
	}

	struct AST *matched = AST_New(result, Dictionary_LookupOrInsert(dict, buffer));
	matched->sourceLine = line;
	matched->sourceCol = col;
	PRINT_MATCH_IF_VERBOSE(matched->type, matched->value);
	return matched;
}

// error-checked method to consume expected token with no return
void consume(enum token t)
{
	enum token result = scan(1);
	if (result != t)
	{
		printf("Expected token %s, got %s\n", token_names[t], token_names[result]);
		ParserError("consume", "Error consuming a token!");
	}
}

char *getTokenName(enum token t)
{
	return token_names[t];
}

struct AST *ParseProgram(char *inFileName, struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseProgram\n");

	curLine = 1;
	curCol = 1;
	inFile = fopen(inFileName, "rb");
	struct AST *program = parseTLDList(dict);
	fclose(inFile);

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return program;
}
