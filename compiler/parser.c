#include "parser.h"

FILE *inFile;
char buffer[BUF_SIZE];
int buflen;
int curLine, curCol;
char inChar;
char *token_names[] = {
	"asm",
	"var",
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

#define VERBOSE_PARSE
#ifdef VERBOSE_PARSE
int parseDepth = 0;

#define PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE(string) \
	for (int i = 0; i < parseDepth; i++)              \
	{                                                 \
		printf("\t");                                 \
	}                                                 \
	parseDepth++;                                     \
	printf(string);

#define PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE() parseDepth--

#define PRINT_MATCH_IF_VERBOSE(token, value) \
	for (int i = 0; i < parseDepth; i++)     \
	{                                        \
		printf("\t");                        \
	}                                        \
	printf("Matched %s: [%s]\n", token_names[token], value)
#else
#define PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE(string)
#define PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE()
PRINT_MATCH_IF_VERBOSE()
#endif

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

#define RESERVED_COUNT 30

char *reserved[RESERVED_COUNT] = {
	"asm",
	"var",
	"fun",
	"return",
	"if",
	"else",
	"while",
	",",
	"(",
	")",
	"{",
	"}",
	"[",
	"]",
	";",
	"=",
	"+",
	"-",
	"*",
	"&",
	">",
	"<",
	">=",
	"<=",
	"==",
	"!=",
	"&&",
	"||",
	"!",
	"$$"};

enum token reserved_t[RESERVED_COUNT] = {
	t_asm,
	t_var,
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
	t_bin_add,
	t_bin_sub,
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
				if (buffer[0] == '<' || buffer[0] == '>' || buffer[0] == '=' || buffer[0] == '!')
				{
					if (lookahead_char() != '=')
						return reserved_t[i];
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
					return reserved_t[i]; // return its token
				}
			}
		}

		// if on the first char of the token
		if (buflen == 1)
		{
			// determine literal or name based on what we started with
			if (isdigit(inChar))
				currentToken = t_literal;
			else if (isalpha(inChar))
				currentToken = t_name;
		}
		else
		{
			// simple error checking for letters in literals
			if (currentToken == t_literal && isalpha(inChar))
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

struct AST *parseTLDList(struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseTLDList\n");

	struct AST *TLDList = parseTLD(dict);
	while (1)
	{
		if (lookahead() == t_EOF)
			break;

		AST_InsertSibling(TLDList, parseTLD(dict));
	}

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return TLDList;
}

struct AST *parseTLD(struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseTLD\n");

	struct AST *TLD;
	switch (lookahead())
	{
	case t_asm:
		TLD = parseASM(dict);
		break;

	// fun [function name]({[argument list]})
	case t_fun:
		TLD = match(t_fun, dict);
		struct AST *functionname = match(t_name, dict);
		consume(t_lParen);
		struct AST *argList = parseArgDefinitions(dict);
		AST_InsertChild(functionname, argList);
		AST_InsertChild(TLD, functionname);
		consume(t_rParen);

		AST_InsertChild(TLD, parseScope(dict));
		break;

	// var [variable name];
	// var [variable name] = [expression];
	case t_var:
	{
		TLD = match(t_var, dict);
		struct AST *name = parseDeclaration(dict);
		if (lookahead() == t_assign)
		{
			struct AST *assignment = match(t_assign, dict);
			AST_InsertChild(assignment, name);
			AST_InsertChild(assignment, parseExpression(dict));
			AST_InsertChild(TLD, assignment);
		}
		else
		{
			AST_InsertChild(TLD, name);
		}

		consume(t_semicolon);
	}
	break;
	default:
		ParserError("Top level declaration", "Expected function or variable");
		exit(1);
	}

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return TLD;
}

struct AST *parseAssignment(struct AST *name, struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseAssignment\n");

	// pre-parsed name node taken as argument
	// [name] = [expression]
	struct AST *assign = match(t_assign, dict);
	AST_InsertChild(assign, name);
	AST_InsertChild(assign, parseExpression(dict));

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return assign;
}

struct AST *parseScope(struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseScope\n");

	struct AST *scope = AST_New(t_scope, "scope");
	consume(t_lCurly);
	// parse statements until we see the end of this scope
	while (lookahead() != t_rCurly)
	{
		if (lookahead() == t_lCurly)
		{
			AST_InsertChild(scope, parseScope(dict));
		}
		else
		{
			AST_InsertChild(scope, parseStatement(dict));
		}
	}
	consume(t_rCurly);

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return scope;
}

struct AST *parseName(struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseName\n");

	struct AST *name = match(t_name, dict);
	if (lookahead() == t_lBracket)
	{
		struct AST *bracketOp = AST_New(t_bin_add, "+");
		AST_InsertChild(bracketOp, name);
		consume(t_lBracket);
		switch (lookahead())
		{
		case t_name:
		case t_literal:
			AST_InsertChild(bracketOp, parseExpression(dict));
			break;

		default:
			ParserError("statement", "Expected name or literal in square brackets after identifier");
		}

		consume(t_rBracket);

		name = AST_New(t_dereference, "*");
		AST_InsertChild(name, bracketOp);
		// name = newName;
	}

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return name;
}

struct AST *parseDeclaration(struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseDeclaration\n");

	struct AST *declared = NULL;
	struct AST *declaredRunner = NULL;
	enum token upcomingToken = lookahead();
	while (upcomingToken == t_dereference)
	{
		if (declaredRunner == NULL)
		{
			declared = match(t_dereference, dict);
			declaredRunner = declared;
		}
		else
		{
			AST_InsertChild(declaredRunner, match(t_dereference, dict));
			declaredRunner = declaredRunner->child;
		}
		upcomingToken = lookahead();
	}

	struct AST *identifier = match(t_name, dict);

	if (lookahead() == t_lBracket)
	{
		consume(t_lBracket);
		struct AST *arrayDecl = AST_New(t_array, "[]");
		AST_InsertChild(arrayDecl, identifier);
		AST_InsertChild(arrayDecl, match(t_literal, dict));
		if (declaredRunner != NULL)
		{
			AST_InsertChild(declaredRunner, arrayDecl);
		}
		else
		{
			declared = arrayDecl;
		}
		consume(t_rBracket);
	}
	else
	{
		if (declaredRunner != NULL)
		{
			AST_InsertChild(declaredRunner, identifier);
		}
		else
		{
			declared = identifier;
		}
	}

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return declared;
}

struct AST *parseStatement(struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseStatement\n");

	struct AST *statement = NULL;
	enum token upcomingToken = lookahead();
	switch (upcomingToken)
	{

	case t_asm:
		statement = parseASM(dict);
		break;

	// v [variable name];
	// v [variable name] = [expression];
	case t_var:
	{
		struct AST *type = match(upcomingToken, dict);
		statement = type;
		// 'var' will obviously be a declaration

		// declared type (including any '*'s)
		struct AST *declaredType = parseDeclaration(dict);

		AST_InsertChild(type, declaredType);

		if (lookahead() == t_assign)
		{
			struct AST *assignedName = malloc(sizeof(struct AST));
			struct AST *declaredName = declaredType;
			while (declaredName->type == t_dereference)
			{
				declaredName = declaredName->child;
			}
			memcpy(assignedName, declaredName, sizeof(struct AST));
			AST_InsertSibling(statement, parseAssignment(assignedName, dict));
		}

		// ASTNode_insertChild(statement, type);

		consume(t_semicolon);
	}
	break;

	// [variable name] = [expression];
	case t_name:
	{
		struct AST *name = parseName(dict);

		switch (lookahead())
		{
		case t_assign:
			statement = parseAssignment(name, dict);
			break;

		case t_lParen:
			statement = parseFunctionCall(name, dict);
			break;

		default:
			ParserError("statement", "expected '(' or '=' after name");
		}
		consume(t_semicolon);
	}
	break;

	case t_dereference:
	{
		struct AST *deref = match(t_dereference, dict);
		switch (lookahead())
		{
		case t_dereference:
		case t_lParen:
			AST_InsertChild(deref, parseExpression(dict));
			break;

		default:
			AST_InsertChild(deref, parseName(dict));
			break;
		}
		statement = parseAssignment(deref, dict);
		consume(t_semicolon);
	}
	break;

	case t_return:
	{
		statement = match(t_return, dict);
		AST_InsertChild(statement, parseExpression(dict));
		consume(t_semicolon);
	}
	break;

	case t_if:
		statement = parseIfStatement(dict);
		break;

	case t_while:
		statement = parseWhileLoop(dict);
		break;

	default:
		ParserError("statement", "expected 'var', 'if', 'while', '}', or name");
	}

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return statement;
}

struct AST *parseExpression(struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseExpression\n");

	// printf("parsing expression\n");
	struct AST *expression = NULL;

	// figure out what the left side of the expression is
	struct AST *lSide = NULL;

	enum token nextToken;

	switch ((nextToken = lookahead()))
	{
	case t_name:
	{
		struct AST *name = parseName(dict);
		if (lookahead() == t_lParen)
		{
			lSide = parseFunctionCall(name, dict);
		}
		else
		{
			lSide = name;
		}
	}
	break;

	case t_literal:
		lSide = match(nextToken, dict);
		break;

	case t_lParen:
		consume(t_lParen);
		lSide = parseExpression(dict);
		consume(t_rParen);
		break;

	case t_dereference:
	case t_reference:
		lSide = match(nextToken, dict);
		break;

	default:
		ParserError("expression", "expected literal or name");
		break;
	}

	// now, figure out whether there is a right side
	switch ((nextToken = lookahead()))
	{
	// [left side][operator][right side]
	case t_bin_add:
	case t_bin_sub:
		expression = match(nextToken, dict);
		AST_InsertChild(expression, lSide);
		AST_InsertChild(expression, parseExpression(dict));
		break;

	case t_assign:
		expression = lSide;
		break;

	// end of line or end of expression, or condition check - there isn't anything more than the left side
	case t_bin_lThan:
	case t_bin_lThanE:
	case t_bin_gThan:
	case t_bin_gThanE:
	case t_bin_equals:
	case t_bin_notEquals:
	case t_bin_log_and:
	case t_bin_log_or:
	case t_semicolon:
	case t_comma:
	case t_rParen:
	case t_rBracket:
		expression = lSide;
		break;

	default:
		printf("\n[%c]\n", lookahead());
		ParserError("expression", "expected binary operator or terminator");
	}

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return expression;
}

struct AST *parseArgDefinitions(struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseArgDefinitions\n");

	struct AST *argList = NULL;
	int parsing = 1;
	while (parsing)
	{
		enum token next = lookahead();
		switch (next)
		{
		case t_var:
		{
			struct AST *argument = match(next, dict);
			struct AST *declaration = argument;
			while (lookahead() == t_dereference)
			{
				AST_InsertChild(argument, match(t_dereference, dict));
				declaration = declaration->child;
			}
			AST_InsertChild(declaration, match(t_name, dict));

			if (argList == NULL)
				argList = argument;
			else
				AST_InsertSibling(argList, argument);
		}
		break;

		case t_comma:
			consume(next);
			break;

		default:
			parsing = 0;
			break;
		}
	}

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return argList;
}

struct AST *parseArgList(struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseArgList\n");

	struct AST *argList = NULL;
	int parsing = 1;
	while (parsing)
	{
		switch (lookahead())
		{
		case t_rParen:
			parsing = 0;
			break;

		case t_comma:
			consume(t_comma);
			break;

		case t_literal:
			/*if (argList == NULL)
				argList = match(t_literal, dict);
			else
				ASTNode_insertSibling(argList, match(t_literal, dict));
			break;*/

		default:
			if (argList == NULL)
				argList = parseExpression(dict);
			else
				AST_InsertSibling(argList, parseExpression(dict));
			break;
		}
	}

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return argList;
}

struct AST *parseFunctionCall(struct AST *name, struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseFunctionCall\n");

	consume(t_lParen);
	struct AST *callNode = AST_New(t_call, "call");
	AST_InsertChild(callNode, name);
	AST_InsertChild(name, parseArgList(dict));
	consume(t_rParen);

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return callNode;
}

struct AST *parseConditionCheck(struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseConditionCheck\n");

	struct AST *conditionCheck = NULL;

	struct AST *LHS = NULL;

	switch (lookahead())
	{
	case t_name:
	case t_literal:
	case t_dereference:
	case t_reference:
		LHS = parseExpression(dict);
		break;

	case t_lParen:
		consume(t_lParen);
		LHS = parseConditionCheck(dict);
		consume(t_rParen);
		break;

	default:
		ParserError("condition check", "expected expression or unary operator!");
	}

	enum token nextToken;
	switch (nextToken = lookahead())
	{
	case t_bin_lThan:
	case t_bin_lThanE:
	case t_bin_gThan:
	case t_bin_gThanE:
	case t_bin_equals:
	case t_bin_notEquals:
	case t_bin_log_and:
	case t_bin_log_or:
		conditionCheck = match(nextToken, dict);
		AST_InsertChild(conditionCheck, LHS);
		AST_InsertChild(conditionCheck, parseConditionCheck(dict));
		break;

	case t_rParen:
		conditionCheck = LHS;
		break;

	default:
		ParserError("condition check", "expected comparison operator!");
	}

	/*
	switch (lookahead())
	{
	case t_name:
	case t_literal:
	case t_dereference:
	case t_reference:
		AST_InsertChild(conditionCheck, parseExpression(dict));
		break;

	case t_lParen:
		consume(t_lParen);
		AST_InsertChild(conditionCheck, parseConditionCheck(dict));
		consume(t_rParen);
		break;

	default:
		ParserError("condition check (RHS)", "expected expression or unary operator!");
	}
	*/

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();
	return conditionCheck;
}

struct AST *parseIfStatement(struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseIfStatement\n");

	struct AST *ifStatement = match(t_if, dict);
	consume(t_lParen);
	AST_InsertChild(ifStatement, parseConditionCheck(dict));
	consume(t_rParen);
	struct AST *doBlock = AST_New(t_do, "do");
	AST_InsertChild(ifStatement, doBlock);

	if (lookahead() == t_lCurly)
	{
		AST_InsertChild(doBlock, parseScope(dict));
	}
	else
	{
		struct AST *ifScope = AST_New(t_scope, "scope");
		AST_InsertChild(ifScope, parseStatement(dict));
		AST_InsertChild(doBlock, ifScope);
	}

	if (lookahead() == t_else)
	{
		AST_InsertChild(ifStatement, parseElseStatement(dict));
	}

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return ifStatement;
}

struct AST *parseElseStatement(struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseElseStatement\n");

	struct AST *elseStatement = match(t_else, dict);
	struct AST *doBlock = AST_New(t_do, "do");
	AST_InsertChild(elseStatement, doBlock);
	if (lookahead() == t_lCurly)
	{
		AST_InsertChild(doBlock, parseScope(dict));
	}
	else
	{
		struct AST *elseScope = AST_New(t_scope, "scope");
		AST_InsertChild(elseScope, parseStatement(dict));
		AST_InsertChild(doBlock, elseScope);
	}

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return elseStatement;
}

struct AST *parseWhileLoop(struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseWhileLoop\n");

	struct AST *whileLoop = match(t_while, dict);
	consume(t_lParen);
	AST_InsertChild(whileLoop, parseConditionCheck(dict));
	consume(t_rParen);
	struct AST *doBlock = AST_New(t_do, "do");
	AST_InsertChild(whileLoop, doBlock);
	if (lookahead() == t_lCurly)
	{
		AST_InsertChild(doBlock, parseScope(dict));
	}
	else
	{
		struct AST *whileScope = AST_New(t_scope, "scope");
		AST_InsertChild(whileScope, parseStatement(dict));
		AST_InsertChild(doBlock, whileScope);
	}

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return whileLoop;
}

struct AST *parseASM(struct Dictionary *dict)
{
	PRINT_PARSE_FUNCTION_ENTER_IF_VERBOSE("ParseASM\n");

	struct AST *asmNode = match(t_asm, dict);
	consume(t_lCurly);
	trimWhitespace(1);
	char inASMblock = 1;
	int lineLen = 0;
	char asmLine[32];
	while (inASMblock)
	{
		switch (lookahead_char_dumb(1))
		{
		case '}':
			if (lineLen > 0)
			{
				asmLine[lineLen] = '\0';
				AST_InsertChild(asmNode, AST_New(t_asm, Dictionary_LookupOrInsert(dict, asmLine)));
			}
			curCol++;
			inASMblock = 0;
			break;

		case '\n':
			asmLine[lineLen] = '\0';
			curCol = 0;
			AST_InsertChild(asmNode, AST_New(t_asm, Dictionary_LookupOrInsert(dict, asmLine)));
			trimWhitespace(1);
			lineLen = 0;
			break;

		default:
			asmLine[lineLen++] = fgetc(inFile);
			curCol++;
		}
	}
	consume(t_rCurly);
	consume(t_semicolon);

	PRINT_PARSE_FUNCTION_DONE_IF_VERBOSE();

	return asmNode;
}