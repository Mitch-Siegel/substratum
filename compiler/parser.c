#include "parser.h"
#include "parseRecipes.h"

FILE *inFile;
char buffer[BUF_SIZE];
int buflen;
char *curFile;
char justStartedNewFile = 0;
int curLine, curCol;
char *token_names[] = {
	"p_type_name",
	"p_primary_expression",
	"p_wip_array_access",
	"p_unary_operator",
	"p_unary_expression",
	"p_expression_operator",
	"p_wip_expression",
	"p_expression",
	"p_expression_tail",
	"p_function_opener",
	"p_function_call",
	"p_expression_list",
	"p_wip_array_declaration",
	"p_variable_declaration",
	"p_declaration_list",
	"p_variable_declaration_statement",
	"p_assignment_statement",
	"p_return_statement",
	"p_if_awating_else",
	"p_if_else",
	"p_if",
	"p_statement",
	"p_statement_list",
	"p_while",
	"p_scope",
	"p_function_declaration",
	"p_function_definition",
	"p_translation_unit",
	"p_null",
	// begin tokens
	"t_identifier",
	"t_constant",
	"t_char_literal",
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
	"t_lThanE",
	"t_gThanE",
	"t_equals",
	"t_nEquals",
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
	"t_file",
	"t_line",
	"t_EOF"};

#define RECIPE_INGREDIENT(production, permutation, index) parseRecipes[production][permutation][index][0]
#define RECIPE_INSTRUCTION(production, permutation, index) parseRecipes[production][permutation][index][1]

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
						{
							ErrorAndExit(ERROR_INVOCATION, "Error parsing comment - nested block comments are not allowed!\n");
						}
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
				curCol++;
				break;
			}

			break;

		default:
			trimming = 0;
			break;
		}
	}
}

#define RESERVED_COUNT 41

struct ReservedToken
{
	const char *string;
	enum token token;
};

struct ReservedToken reserved[RESERVED_COUNT] = {
	{"asm", t_asm},

	{"void", t_void},
	{"uint8", t_uint8},
	{"uint16", t_uint16},
	{"uint32", t_uint32},

	{"fun", t_fun},
	{"return", t_return},

	{"if", t_if},
	{"else", t_else},
	{"while", t_while},
	{"for", t_for},

	{"+", t_plus},
	{"-", t_minus},

	{"<", t_lThan},
	{">", t_gThan},
	{"<=", t_lThanE},
	{">=", t_gThanE},
	{"==", t_equals},
	{"!=", t_nEquals},

	{"&", t_and},
	{"|", t_or},
	{"!", t_not},

	{"~", t_bit_not},
	{"^", t_xor},

	{"?", t_ternary},

	{"&", t_reference},
	{"*", t_star},

	{"=", t_single_equals},

	{",", t_comma},
	{".", t_dot},
	{"->", t_pointer_op},
	{";", t_semicolon},
	{":", t_colon},
	{"(", t_lParen},
	{")", t_rParen},
	{"{", t_lCurly},
	{"}", t_rCurly},
	{"[", t_lBracket},
	{"]", t_rBracket},

	// parser directives to inform where things came from at preprocess time
	{"#file", t_file},
	{"#line", t_line}

};

int fgetc_track(char trackPos)
{
	int gotten = fgetc(inFile);
	if (trackPos)
	{
		if (gotten == '\n')
		{
			curLine++;
			curCol = 0;
		}
		else
		{
			curCol++;
		}
	}
	return gotten;
}

// scan and return the raw token we see
enum token _scan(char trackPos)
{
	buflen = 0;
	// check if we're looking at whitespace - are we?
	trimWhitespace(trackPos);
	if (feof(inFile))
		return t_EOF;

	enum token currentToken = -1;

	if (lookahead_char_dumb(1) == '"')
	{
		fgetc_track(trackPos);
		int inChar;
		int gotEndQuote = 0;
		while ((inChar = fgetc_track(trackPos)) != EOF)
		{
			if (inChar == '"')
			{
				gotEndQuote = 1;
				break;
			}
			buffer[buflen++] = inChar;
			buffer[buflen] = '\0';
		}

		if (!gotEndQuote)
		{
			struct AST ex;
			ex.sourceFile = curFile;
			ex.sourceCol = curCol;
			ex.sourceLine = curLine;
			ErrorWithAST(ERROR_CODE, (&ex), "String literal doesn't end!\n");
		}
		return t_string_literal;
	}
	else if(lookahead_char_dumb(1) == '\'')
	{
		fgetc_track(trackPos);
		buffer[buflen++] = fgetc_track(trackPos);
		buffer[buflen] = '\0';
		if(fgetc_track(trackPos) != '\'')
		{
			struct AST ex;
			ex.sourceFile = curFile;
			ex.sourceCol = curCol;
			ex.sourceLine = curLine;
			ErrorWithAST(ERROR_CODE, (&ex), "Character literal with more than one character inside!\n");
		}
		return t_char_literal;
	}

	while (1)
	{
		int inChar = fgetc_track(trackPos);
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
				char forceNextChar = 0;
				switch (reserved[i].token)
				{
				case t_single_equals:
				case t_not:
				case t_gThan:
				case t_lThan:
					if (lookahead_char_dumb(1) == '=')
					{
						forceNextChar = 1;
					}
					else
					{
						return reserved[i].token;
					}
					break;

				case t_minus:
					if (lookahead_char_dumb(1) == '>')
					{
						forceNextChar = 1;
					}
					else
					{
						return reserved[i].token;
					}
					break;

				default:
					return reserved[i].token;
				}

				if (forceNextChar)
				{
					break;
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
			{
				ErrorAndExit(ERROR_INTERNAL, "Error parsing literal - alphabetical character detected!\n");
			}
		}

		// if the next input char is whitespace or a single-character token, we're done with this token
		switch (lookahead_char_dumb(1))
		{
		case ' ':
		case '\n':
		case '\t':
		case ',':
		case '(':
		case ')':
		case '{':
		case '}':
		case '[':
		case ']':
		case ';':
		case '+':
		case '-':
		case '*':
		case '/':
			return currentToken;
			break;
		}
	}
}

// wrapper around _scan
// handles #file and #line directives in order to correctly track position in preprocessed files
enum token scan(char trackPos, struct Dictionary *dict)
{
	enum token result = _scan(trackPos);
	// chew through all instances of #file and #line
	while (result == t_file || result == t_line)
	{
		switch (result)
		{
		case t_file:
			// make sure we see what we expect after the #file
			if (_scan(trackPos) != t_string_literal)
			{
				struct AST ex;
				ex.sourceCol = curCol;
				ex.sourceLine = curLine;
				ErrorWithAST(ERROR_INTERNAL, (&ex), "Expected filename (as t_identifier) after #file\n\tSaw malformed preprocessor output and got [%s] instead!\n", buffer);
			}

			printf("FILE: %s\n", buffer);

			// if we are bothering to track position, actually update the current file
			if (trackPos)
			{
				curFile = Dictionary_LookupOrInsert(dict, buffer);
				justStartedNewFile = 1;
			}
			result = _scan(trackPos);
			break;

		case t_line:
			// make sure we see what we expect after the #line
			if (_scan(trackPos) != t_constant)
			{
				struct AST ex;
				ex.sourceCol = curCol;
				ex.sourceLine = curLine;
				ErrorWithAST(ERROR_INTERNAL, (&ex), "Expected line (as t_constant) after #line\n\tSaw malformed preprocessor output and got [%s] instead!\n", buffer);
			}

			// if we are bothering to track position, actually update the current line
			if (trackPos)
			{
				curLine = atoi(buffer);
			}
			result = _scan(trackPos);
			break;

		default:
			break;
		}
	}

	return result;
}

// return the next token that would be scanned without consuming
enum token lookahead()
{
	long offset = ftell(inFile);
	enum token retToken = scan(0, NULL);
	fseek(inFile, offset, SEEK_SET);
	return retToken;
}

// error-checked method to consume and return AST node of expected token
struct AST *match(enum token t, struct Dictionary *dict)
{
	trimWhitespace(1);
	int line = curLine;
	int col = curCol;
	enum token result = scan(1, dict);

	if (result != t)
	{
		printf("Expected token %s, got %s\n", token_names[t], token_names[result]);
		ErrorAndExit(ERROR_INTERNAL, "Error matching token!\t");
	}

	struct AST *matched = AST_New(result, Dictionary_LookupOrInsert(dict, buffer));
	matched->sourceLine = line;
	matched->sourceCol = col;
	return matched;
}

// error-checked method to consume expected token with no return
void consume(enum token t, struct Dictionary *dict)
{
	enum token result = scan(1, dict);
	if (result != t)
	{
		printf("Expected token %s, got %s\n", token_names[t], token_names[result]);
		ErrorAndExit(ERROR_INTERNAL, "Error consuming token!\t");
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

void printUpper(char *str)
{
	int i = 0;
	char toPrint;
	while ((toPrint = str[i++]) != '\0')
	{
		putchar(toupper(toPrint));
	}
}

void enumeratePossibleProductions()
{
	// iterate all possible productions
	for (int pi = 0; pi < p_null; pi++)
	{
		printUpper(getTokenName(pi) + 2);
		puts(":");
		// iterate all permutations of each production
		for (int qi = 0; RECIPE_INGREDIENT(pi, qi, 0) != p_null; qi++)
		{
			printf("\t");
			enum token currentIngredient;
			for (int ti = 0; (currentIngredient = RECIPE_INGREDIENT(pi, qi, ti)) != p_null; ti++)
			{
				char *tokenName = getTokenName(currentIngredient);
				if (currentIngredient < p_null)
				{
					printUpper(tokenName + 2);
					putchar(' ');
				}
				else
				{
					char found = 0;
					for (int i = 0; i < RESERVED_COUNT; i++)
					{
						if (reserved[i].token == currentIngredient)
						{
							printf("'%s' ", reserved[i].string);
							found = 1;
							break;
						}
					}
					if (!found)
					{
						printf("%s ", tokenName);
					}
				}
			}
			printf("\n");
		}
		printf("\n");
	}
}

void printParseStack(struct Stack *parseStack)
{
	printf("Parse Stack:\n");
	for (int i = 0; i < parseStack->size; i++)
	{
		printf("%2d: ", i);
		struct InProgressProduction *thisProduction = (struct InProgressProduction *)parseStack->data[i];
		if (thisProduction->tree == NULL)
		{
			printf("No line/col info available for: ");
		}
		else
		{
			printf("%s:%3d:%-3d: %10s\t", thisProduction->tree->sourceFile, thisProduction->tree->sourceLine, thisProduction->tree->sourceCol, thisProduction->tree->value);
		}

		printf("%s ", getTokenName(thisProduction->production));

		if (thisProduction->production == p_null)
		{
			ErrorAndExit(ERROR_INTERNAL, "null production in parse stack!\n");
		}
		printf("\n");
	}
	printf("\n");
}

enum token foundReduction[2] = {p_null, 0};
// arguments: a parse stack and whether or not there is more input
// returns: pointer to InProgressProduction for lookahead token from the top of the stack, not included in the found production
// 			or NULL if no token popped
struct InProgressProduction *findReduction(struct Stack *parseStack, char isRemainingInput)
{
	struct InProgressProduction *poppedLookahead = NULL;

	// skip over any productions of length 1 if we have a token on top of the stack (lookahead)
	char disallowTrivialProductions = 0;
	if (parseStack->size > 0 && isRemainingInput)
	{
		struct InProgressProduction *topOfStack = (struct InProgressProduction *)Stack_Peek(parseStack);
		if (topOfStack->production > p_null)
		{
			disallowTrivialProductions = 1;
		}
	}

	foundReduction[0] = p_null;
	foundReduction[1] = -1;

	// loop at most 2 times
	for (int i = 0; i < 2; i++)
	{
		// search greedily (in order of recipe enumeration) for longest possible production
		for (int startIndex = 0; startIndex < parseStack->size; startIndex++)
		{
			// iterate all possible productions
			for (int pi = 0; pi < p_null; pi++)
			{
				// iterate all permutations of each production
				for (int qi = 0; (RECIPE_INGREDIENT(pi, qi, 0) != p_null); qi++)
				{
					// calculate the size of this production
					int thisProductionSize = 0;
					for (thisProductionSize = 0; RECIPE_INGREDIENT(pi, qi, thisProductionSize) != p_null; thisProductionSize++)
						;
					// thisProductionSize++;

					if ((startIndex + thisProductionSize) != parseStack->size)
					{
						// printf("\tSkip %s:%d\n", getTokenName(pi), qi);
						continue;
					}

					if (disallowTrivialProductions && thisProductionSize == 1)
					{
						continue;
					}

					// printf("\tTrying %s:%d (size of %d, start from index %d)\n", getTokenName(pi), qi, thisProductionSize, startIndex);
					int ti;
					enum token examinedIngredient;
					for (ti = 0; ((examinedIngredient = RECIPE_INGREDIENT(pi, qi, ti)) != p_null) && (startIndex + ti < parseStack->size); ti++)
					{
						struct InProgressProduction *examinedExistingProduction = (struct InProgressProduction *)parseStack->data[startIndex + ti];
						// printf("\t\t%s == %s?:\t", getTokenName(examinedExistingProduction->production), getTokenName(examinedIngredient));
						if (examinedIngredient != examinedExistingProduction->production)
						{
							// printf("NO - moving on to next recipe\n\n");
							break;
						}
						else
						{
							// printf("YES\n");
						}
					}

					// ensure we got to the end of the production
					if (ti == thisProductionSize && (startIndex + ti == parseStack->size))
					{
						// printf("Found production %s, recipe %d\n", getTokenName(pi), qi);
						foundReduction[0] = pi;
						foundReduction[1] = qi;
						return poppedLookahead;
					}
				}
			}
		}

		if (poppedLookahead == NULL)
		{
			if (parseStack->size > 0)
			{
				struct InProgressProduction *topOfStack = (struct InProgressProduction *)Stack_Peek(parseStack);
				if (topOfStack->production > p_null)
				{
					poppedLookahead = Stack_Pop(parseStack);
					disallowTrivialProductions = 0;
				}
				else
				{
					return NULL;
				}
			}
			else
			{
				return NULL;
			}
		}
		else
		{
			Stack_Push(parseStack, poppedLookahead);
			return NULL;
		}
	}
	ErrorAndExit(ERROR_INTERNAL, "Bad condition led to exit of lookahead loop in findReduction()");
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

	ErrorAndExit(ERROR_INTERNAL, "Fell through switch on instruction type in performRecipeInstruction\n");
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

char *ExpandSourceFromAST(struct AST *tree)
{
	struct Stack *allNodes = Stack_New();
	struct Stack *toVisit = Stack_New();
	Stack_Push(toVisit, tree);
	while (toVisit->size > 0)
	{
		struct AST *visited = Stack_Pop(toVisit);
		Stack_Push(allNodes, visited);

		if (visited->child != NULL)
		{
			Stack_Push(toVisit, visited->child);
		}

		if (visited->sibling != NULL)
		{
			Stack_Push(toVisit, visited->sibling);
		}
	}

	for (int i = 0; i < allNodes->size; i++)
	{
		for (int j = 0; j < (allNodes->size - i) - 1; j++)
		{
			struct AST *a = allNodes->data[j];
			struct AST *b = allNodes->data[j + 1];

			if (a->sourceLine > b->sourceLine)
			{
				allNodes->data[j + 1] = a;
				allNodes->data[j] = b;
			}
		}
	}

	for (int i = 0; i < allNodes->size; i++)
	{
		for (int j = 0; j < (allNodes->size - i) - 1; j++)
		{
			struct AST *a = allNodes->data[j];
			struct AST *b = allNodes->data[j + 1];

			if ((a->sourceLine == b->sourceLine) && (a->sourceCol > b->sourceCol))
			{
				allNodes->data[j + 1] = a;
				allNodes->data[j] = b;
				continue;
			}
		}
	}

	char *printedStr = malloc(1);
	printedStr[0] = '\0';
	struct AST *lastPrinted = allNodes->data[0];
	for (int i = 0; i < allNodes->size; i++)
	{
		struct AST *nodeToPrint = allNodes->data[i];
		if (nodeToPrint->sourceLine > lastPrinted->sourceLine)
		{
			char lineNo[10];
			snprintf(lineNo, 9, "\n%4d:", nodeToPrint->sourceLine);
			printedStr = strAppend(printedStr, strdup(lineNo));

			for (int i = 0; i < nodeToPrint->sourceCol; i++)
			{
				printedStr = strAppend(printedStr, strdup(" "));
			}
		}
		// else
		// {
		int sizeDiff = (nodeToPrint->sourceCol - strlen(lastPrinted->value)) - lastPrinted->sourceCol;
		for (int i = 0; i < sizeDiff; i++)
		{
			printedStr = strAppend(printedStr, strdup(" "));
		}
		// }
		printedStr = strAppend(printedStr, strdup(nodeToPrint->value));

		lastPrinted = nodeToPrint;
	}
	return printedStr;
}

void TableParseError(struct Stack *parseStack)
{
	printParseStack(parseStack);

	struct InProgressProduction *firstIPP = (struct InProgressProduction *)Stack_Peek(parseStack);
	printf("Error at or near line %d, col %d:\nSource code looks approximately like:\n\t", firstIPP->tree->sourceLine, firstIPP->tree->sourceCol);

	char *printedSource = malloc(1);
	printedSource[0] = '\0';

	struct InProgressProduction *examinedIPP = Stack_Peek(parseStack);
	printedSource = ExpandSourceFromAST(examinedIPP->tree);

	printf("%s\n\n", printedSource);
	free(printedSource);
	ErrorAndExit(ERROR_INVOCATION, "Fix your program!\t");
}

// compare each subsection of the stack to all possible recipes to see if any production exists
// return silently if good, call TableParseError if error detected
void ValidateParseStack(struct Stack *parseStack)
{
	int nLoops = 0;
	// printf("Validating parse stack:\n");

	char valid = 1;
	// look through stack starting from every possible index
	// every starting index in the stack must also be the start of some contiguous part of a possible production
	for (int startIndex = parseStack->size - 1; (startIndex > 0) && valid; startIndex--)
	{
		// only examine starting indices which are tokens, not productions
		// since productions have already been generated from tokens we can assume they're valid
		// (and will only ever be used in other valid productions)
		struct InProgressProduction *examinedIndex = (struct InProgressProduction *)parseStack->data[startIndex];
		if (examinedIndex->production < p_null)
		{
			continue;
		}

		char validThisIndex = 0;
		// for all possible productions
		for (int pi = 0; pi < p_null; pi++)
		{
			// for all permutations per production
			for (int qi = 0; RECIPE_INGREDIENT(pi, qi, 0) != p_null; qi++)
			{
				// for every possible offset within the production
				for (int ingredientOffset = 0; RECIPE_INGREDIENT(pi, qi, ingredientOffset) != p_null; ingredientOffset++)
				{
					// char validThisOffset = 1;
					int ti;
					// drive ti over the range from offset -> either: 1. out of ingredients (complete and valid production) 2. at top of stack (part of a valid production but not all)
					for (ti = ingredientOffset; (ti < (parseStack->size - startIndex)) && (RECIPE_INGREDIENT(pi, qi, ti) != p_null); ti++)
					{
						nLoops++;
						struct InProgressProduction *examinedIPP = (struct InProgressProduction *)parseStack->data[startIndex + ti];
						if (examinedIPP->production != RECIPE_INGREDIENT(pi, qi, ti))
						{
							// validThisOffset = 0;
							break;
						}
					}
					if ((startIndex + ti + 1 == parseStack->size) || (RECIPE_INGREDIENT(pi, qi, ti) == p_null))
					{
						validThisIndex = 1;
						break;
					}
				}
				if (validThisIndex)
				{
					break;
				}
			}
		}
		valid &= validThisIndex;
	}

	// printf("Validation completed %d comparisons - good?:%d\n", nLoops, valid);
	if (!valid)
	{
		TableParseError(parseStack);
	}
}

struct AST *TableParse(struct Dictionary *dict)
{
	// number of shifts from this file
	int nShifts = 0;

	int maxConsecutiveTokens = 0;
	// scan recipes to figure out the greatest number of consecutive tokens we can have on the stack (to catch errors)
	// iterate all recipe sets
	for (int pi = 0; pi < p_null; pi++)
	{
		// iterate each recipe within the set (last recipe is just a singe null production)
		for (int qi = 0; RECIPE_INGREDIENT(pi, qi, 0) != p_null; qi++)
		{
			int nConsecutiveTerminals = 0;
			for (int ti = 0; RECIPE_INGREDIENT(pi, qi, ti) != p_null; ti++)
			{
				if (RECIPE_INGREDIENT(pi, qi, ti) < p_null)
				{
					nConsecutiveTerminals = 0;
				}
				else
				{
					nConsecutiveTerminals++;
					if (nConsecutiveTerminals > maxConsecutiveTokens)
					{
						maxConsecutiveTokens = nConsecutiveTerminals;
					}
				}
			}
		}
	}

	struct Stack *parseStack = Stack_New();
	char parsing = 1;
	char haveMoreInput = 1;
	while (parsing)
	{
		char forceShift = 0;
		// no errors, actually try to reduce or shift in a new token
		if (parseStack->size > 0)
		{
			struct InProgressProduction *topOfStack = (struct InProgressProduction *)Stack_Peek(parseStack);
			// condition: top of stack holds production, we have more input
			if ((topOfStack->production < p_null) && haveMoreInput)
			{
				forceShift = 1;
			}
		}

		switch (forceShift)
		{
			// not being forced to shift in a lookahead token
		case 0:
		{
			// keep track of the lookahead production - findReduction will pop and return it if it's not used in the current reduce operation
			struct InProgressProduction *lookaheadProduction = NULL;

			// try to find a reduction (potentially return one token from the top of the stack if it is obstructing a reduction underneath it)
			lookaheadProduction = findReduction(parseStack, haveMoreInput);

			// if we found a reduction
			if (foundReduction[0] != p_null)
			{
				// printParseStack(parseStack);

				// do the reduction
				reduce(parseStack);

				// as long as we have some unused lookahead token, try to use it
				while (lookaheadProduction != NULL)
				{
					Stack_Push(parseStack, lookaheadProduction);
					lookaheadProduction = findReduction(parseStack, 1);
					if (foundReduction[0] == p_null)
					{
						break;
					}
					reduce(parseStack);
				}

				if (lookaheadProduction != NULL)
				{
					Stack_Push(parseStack, lookaheadProduction);
				}

				// printf("Found reduction %s:%d\n", getTokenName(foundReduction[0]), foundReduction[1]);
				// assumeMoreInput = 0;
				// }
				if (lookaheadProduction != NULL)
				{
					Stack_Push(parseStack, lookaheadProduction);
				}
				break;
			}
			// didn't find a reduction
			else
			{
				// if we already hit EOF, we are done
				if (!haveMoreInput)
				{
					parsing = 0;
					break;
				}
			}
		}
			// no reduction found, fall through to shift

		// do a shift
		case 1:
		{
			// check for errors
			// ValidateParseStack(parseStack);

			enum token lookaheadToken = lookahead();
			struct AST *nextToken = NULL;
			if (lookaheadToken == t_EOF)
			{
				haveMoreInput = 0;
				break;
			}
			else if (lookaheadToken == t_asm)
			{
				nextToken = match(lookaheadToken, dict);
				consume(t_lCurly, dict);
				while ((lookaheadToken = lookahead()) != t_rCurly)
				{
					buflen = 0;
					while ((buffer[buflen++] = fgetc(inFile)) != '\n')
						;
					curLine++;
					curCol = 0;
					buffer[buflen - 1] = '\0';
					AST_InsertChild(nextToken, AST_New(t_asm, Dictionary_LookupOrInsert(dict, buffer)));
				}
			}
			else
			{
				nextToken = match(lookaheadToken, dict);
			}
			// printf("\tShift token [%s] with type %s\n", nextToken->value, getTokenName(nextToken->type));
			Stack_Push(parseStack, InProgressProduction_New(nextToken->type, nextToken));
			nShifts++;
		}
		break;
		}

		if (parseStack->size > 128)
		{
			printParseStack(parseStack);
			ErrorAndExit(ERROR_CODE, "Maximum parse stack size exceeded!\n");
		}

		// ensure we reduce as much as possible when starting a new file
		if (justStartedNewFile)
		{
			if (parseStack->size == 0)
			{
				justStartedNewFile = 0;
				break;
			}
			struct Stack *newFileStack = Stack_New();
			struct InProgressProduction *peeked = Stack_Peek(parseStack);
			while (!strcmp(curFile, peeked->tree->sourceFile) && parseStack->size > 1)
			{
				Stack_Push(newFileStack, Stack_Pop(parseStack));
				peeked = Stack_Peek(parseStack);
			}

			if (newFileStack->size > 1)
			{
				if (strcmp(curFile, peeked->tree->sourceFile))
				{
					if (parseStack->size > 0 && peeked->production != p_translation_unit)
					{
						printf("Error - files must contain complete translation units! (curFile: %s, peeked %d:%d)\n", curFile, peeked->tree->sourceLine, peeked->tree->sourceCol);
						TableParseError(parseStack);
					}
				}
				printf("%d from new file\n", newFileStack->size);
				justStartedNewFile = 0;
			}
			while (newFileStack->size > 0)
			{
				Stack_Push(parseStack, Stack_Pop(newFileStack));
			}
			Stack_Free(newFileStack);
		}
	}

	if (parseStack->size > 1 || parseStack->size == 0)
	{
		printParseStack(parseStack);
		ErrorAndExit(ERROR_INTERNAL, "Something bad happened during parsing - parse stack dump above\t");
	}

	struct InProgressProduction *finalProduction = (struct InProgressProduction *)Stack_Pop(parseStack);
	struct AST *finalTree = finalProduction->tree;
	free(finalProduction);
	Stack_Free(parseStack);

	return finalTree;
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
