#include "parser.h"
#include "parseRecipes.h"

FILE *inFile;
char buffer[BUF_SIZE];
int buflen;
int curLine, curCol;
char inChar;
char *token_names[] = {
	"p_primary_expression",
	"p_binary_expression",
	"p_expression",
	"p_null",
	"t_identifier",
	"t_constant",
	"t_string_literal",
	// t_sizeof,
	"t_asm",
	// types
	"t_void",
	"t_uint8",
	"t_uint16",
	"t_uint32",
	// function
	"t_fun",
	"t_return",
	// control flow
	"t_if",
	"t_else",
	"t_while",
	"t_for",
	"t_do",

	// arithmetic operators
	// basic arithmetic
	"t_bin_add",
	"t_bin_sub",
	"t_lShift",
	"t_rShift",
	// comparison operators
	"t_bin_lThan",
	"t_bin_gThan",
	"t_bin_lThanE",
	"t_bin_gThanE",
	"t_bin_equals",
	"t_bin_notEquals",
	// logical operators
	"t_bin_log_and",
	"t_bin_log_or",
	"t_un_log_not",
	// bitwise operators
	"t_un_bit_not",
	"t_un_bit_xor",
	"t_un_bit_or",
	// ternary
	"t_ternary",
	// arithmetic-assign operators
	"t_mul_assign",
	"t_add_assign",
	"t_sub_assign",
	"t_lshift_assign",
	"t_rshift_assign",
	"t_bitand_assign",
	"t_bitxor_assign",
	"t_bitor_assign",
	// unary operators
	"t_un_inc",
	"t_un_dec",
	"t_reference",
	"t_star",
	// assignment
	"t_assign",
	//
	"t_comma",
	"t_dot",
	"t_pointer_op",
	"t_semicolon",
	"t_colon",
	"t_lParen",
	"t_rParen",
	"t_lCurly",
	"t_rCurly",
	"t_lBracket",
	"t_rBracket",
	"t_array",
	"t_call",
	"t_scope",
	"t_EOF"};

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
	"asm",	  //	t_asm
	"uint8",  // 	t_uint8
	"uint16", // 	t_uint16
	"uint32", // 	t_uint32
	"fun",	  // 	t_fun
	"return", // 	t_return
	"if",	  // 	t_if
	"else",	  // 	t_else
	"while",  // 	t_while
	",",	  // 	t_comma
	"(",	  // 	t_lParen
	")",	  // 	t_rParen
	"{",	  // 	t_lCurly
	"}",	  // 	t_rCurly
	"[",	  // 	t_lBracket
	"]",	  // 	t_rBracket
	";",	  // 	t_semicolon
	"=",	  // 	t_assign
	"+",	  // 	t_bin_add
	"+=",	  // 	t_add_assign
	"-",	  // 	t_bin_sub
	"-=",	  // 	t_sub_assign
	"*",	  // 	t_star
	"&",	  // 	t_reference
	">",	  // 	t_bin_gThan
	"<",	  // 	t_bin_lThan
	">=",	  // 	t_bin_gThanE
	"<=",	  // 	t_bin_lThanE
	"==",	  // 	t_bin_equals
	"!=",	  // 	t_bin_notEqual
	"&&",	  // 	t_bin_log_and
	"||",	  // 	t_bin_log_or
	"!",	  // 	t_un_log_not
	"$$"};	  // 	t_EOF

enum token reserved_tokens[RESERVED_COUNT] = {
	t_asm,			 // t_asm,
	t_uint8,		 // 	t_uint8,
	t_uint16,		 // 	t_uint16,
	t_uint32,		 // 	t_uint32,
	t_fun,			 // 	t_fun,
	t_return,		 // 	t_return,
	t_if,			 // 	t_if,
	t_else,			 // 	t_else,
	t_while,		 // 	t_while,
	t_comma,		 // 	t_comma,
	t_lParen,		 // 	t_lParen,
	t_rParen,		 // 	t_rParen,
	t_lCurly,		 // 	t_lCurly,
	t_rCurly,		 // 	t_rCurly,
	t_lBracket,		 // 	t_lBracket,
	t_rBracket,		 // 	t_rBracket,
	t_semicolon,	 // 	t_semicolon,
	t_assign,		 // 	t_assign,
	t_bin_add,		 // 	t_bin_add,
	t_add_assign,	 // 	t_add_assign,
	t_bin_sub,		 // 	t_bin_sub,
	t_sub_assign,	 // 	t_un_sub_assign,
	t_star,			 // 	t_star,
	t_reference,	 // 	t_reference,
	t_bin_gThan,	 // 	t_bin_gThan,
	t_bin_lThan,	 // 	t_bin_lThan,
	t_bin_gThanE,	 // 	t_bin_gThanE,
	t_bin_lThanE,	 // 	t_bin_lThanE,
	t_bin_equals,	 // 	t_bin_equals,
	t_bin_notEquals, // 	t_bin_notEquals,
	t_bin_log_and,	 // 	t_bin_log_and,
	t_bin_log_or,	 // 	t_bin_log_or,
	t_un_log_not,	 // 	t_un_log_not,
	t_EOF};			 // 	t_EOF

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

struct InProgressProduction
{
	enum token production;
	struct AST *tree;
};

struct InProgressProduction *InProgressProduction_New(enum token production, struct AST *tree)
{
	struct InProgressProduction *wip = malloc(sizeof(struct InProgressProduction));
	wip->production = production;
	wip->tree = tree;
	return wip;
}

/*
struct LinkedList *visitedProductionsForLookahead = NULL;

char compareTokens(enum token *a, enum token *b)
{
	return *a != *b;
}

void GeneratePossibleLookaheads(struct Stack *possibleProductions, enum token currentProduction, int recipeIndex, int depth)
{
	if (depth > 80)
	{
		return;
	}

	for (int i = 0; parseRecipes[currentProduction][i][0] != p_null; i++)
	{
		enum token indirectIngredient = parseRecipes[currentProduction][i][0];
		if (indirectIngredient > p_null)
		{
			char foundAlready = 0;
			for (int j = 0; j < possibleProductions->size; j++)
			{
				struct ProductionPossibility *existingP = possibleProductions->data[j];
				if (existingP->t == indirectIngredient)
				{
					foundAlready = 1;
				}
			}
			if (!foundAlready)
			{
				struct ProductionPossibility *p = malloc(sizeof(struct ProductionPossibility));
				p->recipeIndex = recipeIndex;
				p->t = indirectIngredient;
				Stack_Push(possibleProductions, (void *)p);
			}
		}
		else
		{
			if (LinkedList_Find(visitedProductionsForLookahead, compareTokens, &indirectIngredient) == NULL)
			{
				enum token *currentProductionCopy = malloc(sizeof(enum token));
				*currentProductionCopy = currentProduction;
				LinkedList_Append(visitedProductionsForLookahead, currentProductionCopy);
				GeneratePossibleLookaheads(possibleProductions, indirectIngredient, recipeIndex, depth + 1);
			}
		}
	}
}
*/

void printParseStack(struct Stack *parseStack)
{
	/*
	printf("Parse Stack:\t");
	for (int i = 0; i < parseStack->size; i++)
	{
		struct InProgressProduction *thisProduction = (struct InProgressProduction *)parseStack->data[i];
		printf("%s ", getTokenName(thisProduction->production));
	}
	printf("\n\t");
	*/
	printf("Parse Stack:\t");
	for (int i = 0; i < parseStack->size; i++)
	{
		struct InProgressProduction *thisProduction = (struct InProgressProduction *)parseStack->data[i];
		if (thisProduction->production < p_null)
		{
			printf("%s ", getTokenName(thisProduction->production));
		}
		else if (thisProduction->production > p_null)
		{
			printf("%s ", thisProduction->tree->value);
		}
		else
		{
			ErrorAndExit(ERROR_INTERNAL, "null production in parse stack!\n");
		}
	}
	printf("\n");
}

enum token foundReduction[2] = {p_null, 0};
void findReduction(struct Stack *parseStack)
{
	foundReduction[0] = p_null;
	foundReduction[1] = -1;
	// iterate all recipe sets
	for (int pi = 0; pi < p_null; pi++)
	{
		// iterate each recipe within the set
		for (int qi = 0; parseRecipes[pi][qi][0] != p_null; qi++)
		{

			// iterate each production/token in the buffer
			int ti;
			int productionLength = 0;
			for (; parseRecipes[pi][qi][productionLength] != p_null; productionLength++)
				;
			// if we don't have enough tokens for this production, skip it
			if(parseStack->size < productionLength)
			{
				continue;
			}

			for (ti = 0; (ti < productionLength) && (parseRecipes[pi][qi][ti] != p_null); ti++)
			{
				struct InProgressProduction *examinedExistingProduction = (struct InProgressProduction *)parseStack->data[parseStack->size - (productionLength - ti)];
				if (parseRecipes[pi][qi][ti] != examinedExistingProduction->production)
				{
					// printf("ingredient %d not what expected - moving on to next recipe\n", ti);
					break;
				}
			}
			if ((ti == productionLength) && (parseRecipes[pi][qi][ti] == p_null))
			{
				// printf("Found production %s, recipe %d\n", getTokenName(pi), qi);
				foundReduction[0] = pi;
				foundReduction[1] = qi;
				return;
			}
		}
	}
}

void reduce(struct Stack *parseStack)
{
	int productionSize = 0;
	while (parseRecipes[foundReduction[0]][foundReduction[1]][productionSize] != p_null)
	{
		productionSize++;
	}
	struct InProgressProduction **ingredients = malloc(productionSize * sizeof(struct InProgressProduction *));

	// grab (in order) the ingredients we will use, copy them to an array here
	for (int i = 0; i < productionSize; i++)
	{
		ingredients[i] = parseStack->data[parseStack->size - (productionSize - i)];
	}

	// take the ingredients off the stack
	for (int i = 0; i < productionSize; i++)
	{
		Stack_Pop(parseStack);
	}

	// construct the tree for this production
	struct InProgressProduction *produced = InProgressProduction_New(foundReduction[0], ingredients[0]->tree);
	enum RecipeInstructions *thisRecipe = parseRecipeInstructions[foundReduction[0]][foundReduction[1]];
	for (int i = 1; i < productionSize; i++)
	{
		switch (thisRecipe[i])
		{
		case above:
		{
			struct AST *oldTree = produced->tree;
			produced->tree = ingredients[i]->tree;
			AST_InsertChild(produced->tree, oldTree);
		}
		break;

		case below:
			AST_InsertChild(produced->tree, ingredients[i]->tree);
			break;

		case cnsme:
			free(ingredients[i]->tree);
			break;
		}
		free(ingredients[i]);
	}

	free(ingredients);
	Stack_Push(parseStack, produced);
}

struct AST *TableParse(struct Dictionary *dict)
{
	struct Stack *parseStack = Stack_New();
	// Stack_Push(parseStack, (void *)p_translation_unit);
	char parsing = 1;
	while (parsing)
	{
		if (parseStack->size > 0)
		{
			findReduction(parseStack);
		}
		if (foundReduction[0] != p_null)
		{
			reduce(parseStack);
			printf("Reduce: %s:%d\n\t", getTokenName(foundReduction[0]), foundReduction[1]);
		}
		else
		{
			struct AST *nextToken = match(lookahead(), dict);
			Stack_Push(parseStack, InProgressProduction_New(nextToken->type, nextToken));
			// printf("shift [%s]\n", nextToken->value);
			printf("Shift: [%s]\n\t", nextToken->value);
		}
		printParseStack(parseStack);
		printf("\n");

		for (int i = 0; i < 0xffffffff; i++)
		{
		}
	}
	return NULL;
}

struct AST *ParseProgram(char *inFileName, struct Dictionary *dict)
{
	curLine = 1;
	curCol = 1;
	inFile = fopen(inFileName, "rb");
	struct AST *program = TableParse(dict);
	fclose(inFile);

	return program;
}
