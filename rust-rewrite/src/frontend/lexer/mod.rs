use token::Token;

use super::sourceloc::SourceLoc;

pub use char_source::CharSource;

mod char_source;
#[cfg(test)]
mod integration_tests;
#[cfg(test)]
mod tests;
pub mod token;

#[derive(Debug)]
pub struct Lexer<'a> {
    cur_line: usize,
    cur_col: usize,
    current_char: Option<char>,
    current_token: Option<Token>,
    char_source: CharSource<'a>,
}

impl<'a> Lexer<'a> {
    fn peek_char(&self) -> Option<char> {
        #[cfg(feature = "loud_lexing")]
        println!("Lexer::peek_char: {:?}", self.current_char);

        self.current_char
    }

    fn advance_char(&mut self) {
        #[cfg(feature = "loud_lexing")]
        println!("Lexer::advance_char: {:?}", self.current_char);
        if let Some(consumed) = self.current_char {
            if consumed == '\n' {
                self.cur_line += 1;
                self.cur_col = 1;
            } else {
                self.cur_col += 1;
            }
        }
        self.current_char = self.char_source.next();
    }

    fn invalid_char(&self, char: char) {
        panic!("Invalid character '{}' at {}", char, self.current_loc());
    }

    fn from_char_source(mut char_source: CharSource<'a>) -> Self {
        let first_char = char_source.next();

        let created = Self {
            cur_line: if first_char == Some('\n') { 2 } else { 1 },
            cur_col: if first_char.is_some() && first_char != Some('\n') {
                2
            } else {
                1
            },
            current_char: first_char,
            current_token: None,
            char_source,
        };

        created
    }

    pub fn from_file(f: std::fs::File) -> Self {
        Self::from_char_source(CharSource::from_file(f))
    }

    pub fn from_string(s: &'a str) -> Self {
        Self::from_char_source(CharSource::from_str(s))
    }

    fn match_kw_or_ident(&mut self) -> Token {
        #[cfg(feature = "loud_lexing")]
        println!("Lexer::match_kw_or_ident");

        let mut identifier = String::new();

        while let Some(c) = self.peek_char() {
            if c.is_alphanumeric() || c == '_' {
                identifier.push(c);
                self.advance_char();
            } else {
                break;
            }
        }

        let matched = match identifier.as_str() {
            "u8" => Token::U8,
            "u16" => Token::U16,
            "u32" => Token::U32,
            "u64" => Token::U64,
            "i8" => Token::I8,
            "i16" => Token::I16,
            "i32" => Token::I32,
            "i64" => Token::I64,
            "fun" => Token::Fun,
            "if" => Token::If,
            "else" => Token::Else,
            "while" => Token::While,
            "pub" => Token::Pub,
            "struct" => Token::Struct,
            _ => Token::Identifier(identifier),
        };

        #[cfg(feature = "loud_lexing")]
        println!("Lexer::match_kw_or_ident: matched {:?}", matched);

        matched
    }

    pub fn peek(&mut self) -> Token {
        if self.current_token.is_none() {
            self.current_token = Some(self.lex());
        }

        let peeked = self.current_token.clone().unwrap_or(Token::Eof);

        #[cfg(feature = "loud_lexing")]
        println!("Lexer::peek() -> {:?}", peeked);

        peeked
    }

    pub fn current_loc(&self) -> SourceLoc {
        SourceLoc::new(self.cur_line, self.cur_col)
    }

    fn match_next_char_for_token_or(
        &mut self,
        expected: char,
        tok_true: Token,
        tok_false: Token,
    ) -> Token {
        #[cfg(feature = "loud_lexing")]
        println!(
            "Lexer::match_next_char_for_token_or: expected: {}, true: {}, false: {}",
            expected, tok_true, tok_false
        );

        match self.peek_char() {
            None => tok_false,
            Some(peeked_char) => {
                if peeked_char == expected {
                    self.advance_char();
                    tok_true
                } else {
                    tok_false
                }
            }
        }
    }

    fn lex(&mut self) -> Token {
        #[cfg(feature = "loud_lexing")]
        println!("Lexer::lex()");

        while self.peek_char().is_some() && self.peek_char().unwrap().is_whitespace() {
            self.advance_char();
        }

        let token = if let Some(peeked_char) = self.peek_char() {
            match peeked_char {
                '{' => {
                    self.advance_char();
                    Token::LCurly
                }
                '}' => {
                    self.advance_char();
                    Token::RCurly
                }
                '(' => {
                    self.advance_char();
                    Token::LParen
                }
                ')' => {
                    self.advance_char();
                    Token::RParen
                }
                '+' => {
                    self.advance_char();
                    Token::Plus
                }
                '-' => {
                    self.advance_char();
                    self.match_next_char_for_token_or('>', Token::Arrow, Token::Minus)
                }
                '*' => {
                    self.advance_char();
                    Token::Star
                }
                '/' => {
                    self.advance_char();
                    Token::FSlash
                }
                '>' => {
                    self.advance_char();
                    self.match_next_char_for_token_or('=', Token::GThanE, Token::GThan)
                }
                '<' => {
                    self.advance_char();
                    self.match_next_char_for_token_or('=', Token::LThanE, Token::LThan)
                }
                '!' => {
                    self.advance_char();
                    match self.peek_char() {
                        None => {
                            self.invalid_char('!');
                            Token::Eof
                        }
                        Some(char) => {
                            if char == '=' {
                                self.advance_char();
                                Token::NotEquals
                            } else {
                                self.invalid_char('!');
                                Token::Eof
                            }
                        }
                    }
                }
                '=' => {
                    self.advance_char();
                    self.match_next_char_for_token_or('=', Token::Equals, Token::Assign)
                }
                ',' => {
                    self.advance_char();
                    Token::Comma
                }
                ';' => {
                    self.advance_char();
                    Token::Semicolon
                }
                ':' => {
                    self.advance_char();
                    Token::Colon
                }
                '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' => {
                    let mut constant_string = String::new();
                    while self.peek_char().is_some() {
                        let char: char = self.peek_char().unwrap();
                        if char.is_numeric() {
                            self.advance_char();
                            constant_string.push(char);
                        } else {
                            break;
                        }
                    }
                    Token::UnsignedDecimalConstant(
                        usize::from_str_radix(&constant_string, 10)
                            .expect("Couldn't convert unsigned decimal constant"),
                    )
                }
                _ => self.match_kw_or_ident(),
            }
        } else {
            Token::Eof
        };

        #[cfg(feature = "loud_lexing")]
        println!("Lexer::lex(): lexed {}", token);

        token
    }

    pub fn next(&mut self) -> Token {
        let next_token = self.lex();
        self.current_token.replace(next_token).unwrap_or(Token::Eof)
    }

    pub fn lex_all(&mut self) -> Vec<Token> {
        println!("Lexer::lex_all()");
        let mut tokens: Vec<Token> = Vec::new();
        if self.current_token.is_none() {
            tokens.push(self.lex());
        }

        loop {
            let next_token = self.next();
            println!("next_token: {:?}", next_token);
            match next_token {
                Token::Eof => {
                    tokens.push(next_token);
                    break;
                }
                _ => {
                    tokens.push(next_token);
                }
            }
        }

        println!("Lexer::lex_all(): {:?}", tokens);

        tokens
    }
}
