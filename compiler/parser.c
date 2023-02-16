#include "parser.h"
#include "parseRecipes.h"

FILE *inFile;
char buffer[BUF_SIZE];
int buflen;
int curLine, curCol;
char *token_names[] = {
	"p_type_name",
	"p_primary_expression",
	"p_unary_operator",
	"p_unary_expression",
	"p_expression",
	"p_expression_list",
	"p_variable_declaration",
	"p_declaration_list",
	"p_variable_declaration_statement",
	"p_expression_statement",
	"p_statement",
	"p_statement_list",
	"p_scope",
	"p_function_definition",
	"p_translation_unit",
	"p_null",
	// begin tokens
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
	"t_plus",
	"t_minus",
	// comparison operators
	"t_lThan",
	"t_gThan",
	// logical operators
	"t_and",
	"t_or",
	"t_not",
	// bitwise operators
	"t_bit_not",
	"t_xor",
	// ternary
	"t_ternary",
	// arithmetic-assign operators
	// unary operators
	"t_reference",
	"t_star",
	// assignment
	"t_single_equals",
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
	"t_EOF"};

#define RECIPE_INGREDIENT(production, permutation, index) parseRecipes[production][permutation][index][0]
#define RECIPE_INSTRUCTION(production, permutation, index) parseRecipes[production][permutation][index][1]

#define ParserError(production, info)                                                 \
	{                                                                                 \
		printf("Error while parsing %s\n", production);                               \
		printf("Error at line %d, col %d\n", curLine, curCol);                        \
		printf("%s\n", info);                                                         \
		ErrorAndExit(ERROR_CODE, "Parse buffer when error occurred: [%s]\n", buffer); \
	}

// return the char 'count' characters ahead
// count must be >0, returns null char otherwise
int lookahead_char_dumb(int count)
{
	long offset = ftell(inFile);
	int returnChar = '\0';
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
		case EOF:
			return;

		case '\n':
			if (trackPos)
			{
				curLine++;
				curCol = 1;
			}
			fgetc(inFile);
			break;

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

int lookahead_char()
{
	trimWhitespace(1);
	char r = lookahead_char_dumb(1);
	return r;
}

#define RESERVED_COUNT 35

struct ReservedToken
{
	const char *string;
	enum token token;
};

struct ReservedToken reserved[RESERVED_COUNT] = {
	// {"", 	t_identifier,}, //	t_identifier,
	// {"", 	t_constant,}, //t_constant,
	// {"", 	t_string_literal,}, //t_string_literal,
	// t_sizeof,}, //// t_sizeof,
	{"asm", t_asm},		  // t_asm,
						  // types
	{"void", t_void},	  // t_void,
	{"uint8", t_uint8},	  // t_uint8,
	{"uint16", t_uint16}, // t_uint16,
	{"uint32", t_uint32}, // t_uint32,
						  // function
	{"fun", t_fun},		  // t_fun,
	{"return", t_return}, // t_return,
	// control flow
	{"if", t_if},			// t_if,
	{"else", t_else},		// t_else,
	{"while", t_while},		// t_while,
	{"for", t_for},			// t_for,
							// {"", 	t_do,}, //t_do,
							// arithmetic operators
							// basic arithmetic
	{"+", t_plus},			// t_plus,
	{"-", t_minus},			// t_minus,
							// comparison operators
	{"<", t_lThan},			// t_lThan,
	{">", t_gThan},			// t_gThan,
							// logical operators
	{"&", t_and},			// t_and,
	{"|", t_or},			// t_or,
	{"!", t_not},			// t_not,
							// bitwise operators}, //// bitwise operators
	{"~", t_bit_not},		// t_bit_not,
	{"^", t_xor},			// t_xor,
							// ternary
	{"?", t_ternary},		// t_ternary,
							// arithmetic-assign operators}, //// arithmetic-assign operators
							// unary operators
	{"&", t_reference},		// t_reference,
	{"*", t_star},			// t_star,
							// assignment
	{"=", t_single_equals}, // t_single_equals,
							// semantics
	{",", t_comma},			// t_comma,
	{".", t_dot},			// t_dot,
	{"->", t_pointer_op},	// t_pointer_op,
	{";", t_semicolon},		// t_semicolon,
	{":", t_colon},			// t_colon,
	{"(", t_lParen},		// t_lParen,
	{")", t_rParen},		// t_rParen,
	{"{", t_lCurly},		// t_lCurly,
	{"}", t_rCurly},		// t_rCurly,
	{"[", t_lBracket},		// t_lBracket,
	{"]", t_rBracket},		// t_rBracket,
							// {"", 	t_EOF}, //t_EOF
};

enum token
scan(char trackPos)
{
	buflen = 0;
	// check if we're looking at whitespace - are we?
	trimWhitespace(trackPos);
	if (feof(inFile))
		return t_EOF;

	enum token currentToken = -1;
	int inChar;

	while (1)
	{
		inChar = fgetc(inFile);
		if (inChar == EOF)
		{
			if (buflen > 0)
			{
				buflen = 0;
				return currentToken;
			}
			else
			{
				return t_EOF;
			}
		}
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
			if (!strcmp(buffer, reserved[i].string))
			{
				return reserved[i].token;
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

void printPossibleProduction(struct Stack *left, struct Stack *right)
{
	printf("\t");
	for (int i = 0; i < left->size; i++)
	{
		char *thisProductionName = getTokenName(((enum token)left->data[i]));
		printf("%s ", thisProductionName);
	}
	for (int i = right->size; i-- > 0;)
	{
		char *thisProductionName = getTokenName((enum token)right->data[i]);
		printf("%s ", thisProductionName);
	}
	printf("\n");
}

int nValidProductions = 0;
void enumeratePossibleProductionsRecursive(struct Stack *leftStack, struct Stack *rightStack, int depth)
{

	int nTerminalsAtEnd;
	for (nTerminalsAtEnd = 0; nTerminalsAtEnd < leftStack->size; nTerminalsAtEnd++)
	{
		if ((enum token)leftStack->data[(leftStack->size - 1) - nTerminalsAtEnd] < p_null)
		{
			break;
		}
	}

	if (nTerminalsAtEnd == leftStack->size)
	{
		nValidProductions++;
		printPossibleProduction(leftStack, rightStack);
	}
	else
	{
		if ((depth > 25) || (nValidProductions > 99))
		{
			return;
		}

		for (int i = 0; i < nTerminalsAtEnd; i++)
		{
			Stack_Push(rightStack, Stack_Pop(leftStack));
		}
		enum token productionToExpand = (enum token)Stack_Pop(leftStack);
		for (int qi = 0; RECIPE_INGREDIENT(productionToExpand, qi, 0) != p_null; qi++)
		{

			int expectedleftStackSize = leftStack->size;
			int expectedrightStackSize = rightStack->size;
			enum token addedIngredient;
			for (int ti = 0; (addedIngredient = RECIPE_INGREDIENT(productionToExpand, qi, ti)) != p_null; ti++)
			{
				Stack_Push(leftStack, (void *)addedIngredient);
			}
			enumeratePossibleProductionsRecursive(leftStack, rightStack, depth + 1);

			while (leftStack->size > expectedleftStackSize)
			{
				Stack_Pop(leftStack);
			}

			while (rightStack->size > expectedrightStackSize)
			{
				Stack_Pop(rightStack);
			}
		}
	}
}

void enumeratePossibleProductions()
{
	struct Stack *leftStack = Stack_New();
	struct Stack *rightStack = Stack_New();
	for (size_t i = 0; i < p_null; i++)
	{
		Stack_Push(leftStack, (void *)i);
		printf("Some possible productions for: %s\n", getTokenName(i));
		nValidProductions = 0;
		enumeratePossibleProductionsRecursive(leftStack, rightStack, 0);
		printf("\n\n");
		while (leftStack->size > 0)
		{
			Stack_Pop(leftStack);
		}
		while (rightStack->size > 0)
		{
			Stack_Pop(rightStack);
		}
	}
	Stack_Free(leftStack);
	Stack_Free(rightStack);
}

void printParseStack(struct Stack *parseStack)
{
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
		// iterate each recipe within the set (last recipe is just a singe null production)
		for (int qi = 0; RECIPE_INGREDIENT(pi, qi, 0) != p_null; qi++)
		{
			// iterate each production/token in the buffer
			int ti;
			int productionLength = 0;
			for (; RECIPE_INGREDIENT(pi, qi, productionLength) != p_null; productionLength++)
				;
			// if we don't have enough tokens for this production, skip it
			if (parseStack->size < productionLength)
			{
				continue;
			}

			for (ti = 0; (ti < productionLength) && (RECIPE_INGREDIENT(pi, qi, ti) != p_null); ti++)
			{
				struct InProgressProduction *examinedExistingProduction = (struct InProgressProduction *)parseStack->data[parseStack->size - (productionLength - ti)];
				if (RECIPE_INGREDIENT(pi, qi, ti) != examinedExistingProduction->production)
				{
					// printf("ingredient %d not what expected - moving on to next recipe\n", ti);
					break;
				}
			}

			// ensure we got to the end of the production and double check for sanity that the end is a null
			if ((ti == productionLength) && (RECIPE_INGREDIENT(pi, qi, ti) == p_null))
			{
				// printf("Found production %s, recipe %d\n", getTokenName(pi), qi);
				foundReduction[0] = pi;
				foundReduction[1] = qi;
				return;
			}
		}
	}
}

struct AST *performRecipeInstruction(struct AST *existingTree, struct AST *ingredientTree, enum RecipeInstructions instruction)
{
	// if no current tree and we don't want to consume the ingredient, the ingredient becomes the tree
	if (existingTree == NULL && instruction != cnsme)
	{
		return ingredientTree;
	}

	switch (instruction)
	{
		// insert as of current node, current node becomes the inserted one
	case above:
		AST_InsertChild(ingredientTree, existingTree);
		return ingredientTree;

		// insert as child of current node, current node stays the same
	case below:
		AST_InsertChild(existingTree, ingredientTree);
		return existingTree;

		// insert as sibling of current node, current node stays the same
	case besid:
		AST_InsertSibling(existingTree, ingredientTree);
		return existingTree;

	case cnsme:
		free(ingredientTree);
		return existingTree;
	}

	ErrorAndExit(ERROR_INTERNAL, "Fell through switch on instruction type in performRecipeInstruction");
}

void reduce(struct Stack *parseStack)
{
	int productionSize = 0;
	while (RECIPE_INGREDIENT(foundReduction[0], foundReduction[1], productionSize) != p_null)
	{
		productionSize++;
	}
	// printf("Reduce %s:%d - %d ingredients\n", getTokenName(foundReduction[0]), foundReduction[1], productionSize);
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
	struct InProgressProduction *produced = InProgressProduction_New(foundReduction[0], NULL);
	for (int i = 0; i < productionSize; i++)
	{
		produced->tree = performRecipeInstruction(produced->tree, ingredients[i]->tree, RECIPE_INSTRUCTION(foundReduction[0], foundReduction[1], i));
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
			// printf("Reduce: %s:%d\n\t", getTokenName(foundReduction[0]), foundReduction[1]);
		}
		else
		{
			enum token lookaheadToken = lookahead();
			struct AST *nextToken = NULL;
			if (lookaheadToken == t_EOF)
			{
				parsing = 0;
				break;
			}
			else if (lookaheadToken == t_asm)
			{
				nextToken = match(lookaheadToken, dict);
				consume(t_lCurly);
				while ((lookaheadToken = lookahead()) != t_rCurly)
				{
					buflen = 0;
					while ((buffer[buflen++] = fgetc(inFile)) != '\n')
						;
					buffer[buflen - 1] = '\0';
					AST_InsertChild(nextToken, AST_New(t_asm, Dictionary_LookupOrInsert(dict, buffer)));
				}
			}
			else
			{
				nextToken = match(lookaheadToken, dict);
			}
			Stack_Push(parseStack, InProgressProduction_New(nextToken->type, nextToken));
			// printf("Shift: [%s]\n\t", nextToken->value);
		}
		// printParseStack(parseStack);
	}

	if (parseStack->size > 1)
	{
		printParseStack(parseStack);
		ErrorAndExit(ERROR_INTERNAL, "Something bad happened during parsing - parse stack dump above");
	}
	return ((struct InProgressProduction *)Stack_Pop(parseStack))->tree;
}

struct AST *ParseProgram(char *inFileName, struct Dictionary *dict)
{
	curLine = 1;
	curCol = 1;
	inFile = fopen(inFileName, "rb");
	// enumeratePossibleProductions();
	struct AST *program = TableParse(dict);
	fclose(inFile);

	return program;
}
