use token::Token;

use super::sourceloc::SourceLoc;

pub use char_source::CharSource;

mod char_source;
pub mod errors;
#[cfg(test)]
mod integration_tests;
#[cfg(test)]
mod tests;
pub mod token;

pub use errors::LexError;

#[derive(Debug)]
pub struct Lexer<'a> {
    cur_line: usize,
    cur_col: usize,
    current_char: Option<char>,
    current_token: Option<(Token, SourceLoc)>,
    char_source: CharSource<'a>,
}

// public methods:
impl<'a> Lexer<'a> {
    pub fn from_char_source(mut char_source: CharSource<'a>) -> Self {
        let first_char = char_source.next();

        let start_line = if first_char == Some('\n') { 2 } else { 1 };
        let start_col = 1;

        let created = Self {
            cur_line: start_line,
            cur_col: start_col,
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

    pub fn peek(&mut self) -> Result<(Token, SourceLoc), LexError> {
        if self.current_token.is_none() {
            self.current_token = Some(self.lex()?);
        }

        let peeked = self
            .current_token
            .clone()
            .unwrap_or((Token::Eof, SourceLoc::new(self.cur_line, self.cur_col)));

        #[cfg(feature = "loud_lexing")]
        println!("Lexer::peek() -> {:?}", peeked);

        Ok(peeked)
    }

    // returns the position to which the input has been read
    pub fn current_loc(&self) -> SourceLoc {
        SourceLoc::new(self.cur_line, self.cur_col)
    }

    pub fn next(&mut self) -> Result<(Token, SourceLoc), LexError> {
        let next_token = self.lex()?;
        Ok(self
            .current_token
            .replace(next_token)
            .unwrap_or((Token::Eof, SourceLoc::new(self.cur_line, self.cur_col))))
    }

    pub fn lex_all(&mut self) -> Result<Vec<(Token, SourceLoc)>, LexError> {
        println!("Lexer::lex_all()");
        let mut tokens: Vec<(Token, SourceLoc)> = Vec::new();
        if self.current_token.is_none() {
            tokens.push(self.lex()?);
        }

        loop {
            let next_token = self.next()?;
            println!("next_token: {:?}", next_token);
            match next_token.0 {
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

        Ok(tokens)
    }
}

// private methods
impl<'a> Lexer<'a> {
    fn peek_char(&self) -> Option<char> {
        // #[cfg(feature = "loud_lexing")]
        // println!("Lexer::peek_char: {:?}", self.current_char);

        self.current_char
    }

    fn advance_char(&mut self) {
        // #[cfg(feature = "loud_lexing")]
        // println!("Lexer::advance_char: {:?}", self.current_char);
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

    fn match_kw_or_ident(&mut self) -> Option<Token> {
        #[cfg(feature = "loud_lexing")]
        println!("Lexer::match_kw_or_ident");

        let mut identifier = String::new();

        if let Some(first_char) = self.peek_char() {
            if first_char.is_alphabetic() || first_char == '_' {
                identifier.push(first_char);
                self.advance_char();
                while let Some(c) = self.peek_char() {
                    if c.is_alphanumeric() || c == '_' {
                        identifier.push(c);
                        self.advance_char();
                    } else {
                        break;
                    }
                }
            }
        }

        let matched = match identifier.as_str() {
            "u8" => Some(Token::U8),
            "u16" => Some(Token::U16),
            "u32" => Some(Token::U32),
            "u64" => Some(Token::U64),
            "i8" => Some(Token::I8),
            "i16" => Some(Token::I16),
            "i32" => Some(Token::I32),
            "i64" => Some(Token::I64),
            "fun" => Some(Token::Fun),
            "if" => Some(Token::If),
            "else" => Some(Token::Else),
            "while" => Some(Token::While),
            "pub" => Some(Token::Pub),
            "struct" => Some(Token::Struct),
            "impl" => Some(Token::Impl),
            _ => {
                if identifier.len() > 0 {
                    Some(Token::Identifier(identifier))
                } else {
                    None
                }
            }
        };

        #[cfg(feature = "loud_lexing")]
        println!("Lexer::match_kw_or_ident: matched {:?}", matched);

        matched
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

    fn trim_whitespace(&mut self) {
        while self.peek_char().is_some() && self.peek_char().unwrap().is_whitespace() {
            self.advance_char();
        }
    }

    fn lex(&mut self) -> Result<(Token, SourceLoc), LexError> {
        #[cfg(feature = "loud_lexing")]
        println!("Lexer::lex()");

        self.trim_whitespace();
        let match_start = SourceLoc::new(self.cur_line, self.cur_col);

        let token = if let Some(peeked_char) = self.peek_char() {
            match peeked_char {
                '{' => {
                    self.advance_char();
                    Ok(Token::LCurly)
                }
                '}' => {
                    self.advance_char();
                    Ok(Token::RCurly)
                }
                '(' => {
                    self.advance_char();
                    Ok(Token::LParen)
                }
                ')' => {
                    self.advance_char();
                    Ok(Token::RParen)
                }
                '+' => {
                    self.advance_char();
                    Ok(Token::Plus)
                }
                '-' => {
                    self.advance_char();
                    Ok(self.match_next_char_for_token_or('>', Token::Arrow, Token::Minus))
                }
                '*' => {
                    self.advance_char();
                    Ok(Token::Star)
                }
                '/' => {
                    self.advance_char();
                    Ok(Token::FSlash)
                }
                '>' => {
                    self.advance_char();
                    Ok(self.match_next_char_for_token_or('=', Token::GThanE, Token::GThan))
                }
                '<' => {
                    self.advance_char();
                    Ok(self.match_next_char_for_token_or('=', Token::LThanE, Token::LThan))
                }
                '!' => {
                    self.advance_char();
                    match self.peek_char() {
                        None => Err(LexError::unexpected_eof(self.current_loc())),
                        Some(character) => {
                            if character == '=' {
                                self.advance_char();
                                Ok(Token::NotEquals)
                            } else {
                                Err(LexError::invalid_char(character, self.current_loc()))
                            }
                        }
                    }
                }
                '=' => {
                    self.advance_char();
                    Ok(self.match_next_char_for_token_or('=', Token::Equals, Token::Assign))
                }
                ',' => {
                    self.advance_char();
                    Ok(Token::Comma)
                }
                '.' => {
                    self.advance_char();
                    Ok(Token::Dot)
                }
                ';' => {
                    self.advance_char();
                    Ok(Token::Semicolon)
                }
                ':' => {
                    self.advance_char();
                    Ok(Token::Colon)
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
                    Ok(Token::UnsignedDecimalConstant(
                        usize::from_str_radix(&constant_string, 10)
                            .expect("Couldn't convert unsigned decimal constant"),
                    ))
                }
                _ => match self.match_kw_or_ident() {
                    Some(token) => Ok(token),
                    None => Err(LexError::invalid_char(
                        self.peek_char().unwrap(),
                        self.current_loc(),
                    )),
                },
            }
        } else {
            Ok(Token::Eof)
        };

        self.trim_whitespace();

        match token {
            Ok(tok) => {
                #[cfg(feature = "loud_lexing")]
                println!("Lexer::lex(): lexed '{}'@{}", tok.name(), match_start);
                Ok((tok, match_start))
            }
            Err(e) => {
                #[cfg(feature = "loud_lexing")]
                println!("Lexer::lex(): lexing error {}", e);
                Err(e)
            }
        }
    }
}
