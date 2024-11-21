use crate::ast::*;
use crate::lexer::*;

pub struct Parser<I>
where
    I: Iterator<Item = char>,
{
    lexer: Lexer<I>,
}

impl<I> Parser<I>
where
    I: Iterator<Item = char>,
{
    pub fn new(lexer: Lexer<I>) -> Self
    where
        I: Iterator<Item = char>,
    {
        Parser { lexer: lexer }
    }

    fn peek_token(&self) -> Token {
        return self.lexer.peek();
    }

    fn next_token(&mut self) -> Token {
        return self.lexer.next();
    }

    fn expect_token(&mut self, t: Token) -> Token {
        if self.peek_token().eq(&t) {
            self.next_token()
        } else {
            panic!(
                "Expected token {} at {}, got token {} instead!",
                t,
                self.lexer.current_loc(),
                self.peek_token()
            );
        }
    }

    fn current_loc(&self) -> SourceLoc {
        self.lexer.current_loc()
    }

    fn unexpected_token<T>(&self) -> T {
        panic!(
            "Unexpected token {} at {}",
            self.peek_token(),
            self.current_loc()
        );
    }

    pub fn parse(&mut self) -> Vec<TranslationUnitTree> {
        let mut translation_units = Vec::new();
        while self.lexer.peek() != Token::Eof {
            translation_units.push(self.parse_translation_unit());
        }
        translation_units
    }

    fn parse_translation_unit(&mut self) -> TranslationUnitTree {
        match self.peek_token() {
            Token::Fun => self.parse_function_declaration_or_definition(),
            _ => self.unexpected_token::<TranslationUnitTree>(),
        }
    }

    fn parse_function_declaration_or_definition(&mut self) -> TranslationUnitTree {
        let function_declaration = self.parse_function_prototype();
        match self.peek_token() {
            Token::LCurly => TranslationUnitTree {
                loc: function_declaration.loc,
                contents: TranslationUnit::FunctionDefinition(FunctionDefinitionTree {
                    prototype: function_declaration,
                    body: self.parse_compoound_statement(),
                }),
            },
            _ => TranslationUnitTree {
                loc: function_declaration.loc,
                contents: TranslationUnit::FunctionDeclaration(function_declaration),
            },
        }
    }

    fn parse_compoound_statement(&mut self) -> CompoundStatementTree {
        let start_loc = self.current_loc();
        self.expect_token(Token::LCurly);
        let mut statements: Vec<StatementTree> = Vec::new();
        loop {
            match self.peek_token() {
                Token::RCurly => break,
                _ => self.unexpected_token(),
            }
        }
        self.expect_token(Token::RCurly);
        CompoundStatementTree {
            loc: start_loc,
            statements: statements,
        }
    }

    fn parse_function_prototype(&mut self) -> FunctionDeclarationTree {
        let start_loc = self.current_loc();
        // start with fun
        self.expect_token(Token::Fun);
        FunctionDeclarationTree {
            // grab start location and name
            loc: start_loc,
            name: self.parse_identifier(),
            arguments: {
                self.expect_token(Token::LParen);
                let mut arguments = Vec::<VariableDeclarationTree>::new();
                loop {
                    match self.peek_token() {
                        // argument declaration
                        Token::U8 | Token::U16 | Token::U32 | Token::U64 => {
                            arguments.push(self.parse_variable_declaration());
                            match self.peek_token() {
                                Token::Comma => self.next_token(), // expect another argument declaration after comma
                                _ => break,                        // loop again for anything else
                            };
                        }
                        Token::RParen => break, // done on rparen
                        _ => self.unexpected_token(),
                    }
                }
                // consume closing paren
                self.expect_token(Token::RParen);
                arguments
            },
            return_type: match self.peek_token() {
                Token::Arrow => {
                    self.next_token();
                    Some(self.parse_typename())
                }
                _ => None,
            },
        }
    }

    fn parse_variable_declaration(&mut self) -> VariableDeclarationTree {
        VariableDeclarationTree {
            loc: self.current_loc(),
            typename: self.parse_typename(),
            name: self.parse_identifier(),
        }
    }

    fn parse_typename(&mut self) -> TypenameTree {
        TypenameTree {
            loc: self.current_loc(),
            name: match self.peek_token() {
                Token::U8 => {
                    self.next_token();
                    String::from("u8")
                }
                Token::U16 => {
                    self.next_token();
                    String::from("u16")
                }
                Token::U32 => {
                    self.next_token();
                    String::from("u32")
                }
                Token::U64 => {
                    self.next_token();
                    String::from("u64")
                }
                _ => self.unexpected_token(),
            },
        }
    }

    fn parse_identifier(&mut self) -> String {
        match self.expect_token(Token::Identifier(String::from(""))) {
            Token::Identifier(value) => value,
            _ => self.unexpected_token::<String>(),
        }
    }
}

#[cfg(test)]
mod tests {}
