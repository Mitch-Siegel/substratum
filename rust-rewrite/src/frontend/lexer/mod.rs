use token::Token;

use super::sourceloc::SourceLoc;

pub mod token;

pub struct Lexer<I>
where
    I: Iterator<Item = char>,
{
    cur_line: usize,
    cur_col: usize,
    current_char: Option<char>,
    current_token: Option<Token>,
    input: I,
}

impl<I> Lexer<I>
where
    I: Iterator<Item = char>,
{
    fn peek_char(&self) -> Option<char> {
        self.current_char
    }

    fn advance_char(&mut self) {
        if let Some(consumed) = self.current_char {
            if consumed == '\n' {
                self.cur_line += 1;
                self.cur_col = 1;
            } else {
                self.cur_col += 1;
            }
        }
        self.current_char = self.input.next();
    }

    fn invalid_char(&self, char: char) {
        panic!("Invalid character '{}' at {}", char, self.current_loc());
    }

    pub fn new(input: I) -> Self {
        let mut created = Lexer {
            cur_line: 1,
            cur_col: 1,
            current_char: None,
            current_token: None,
            input: input,
        };
        created.advance_char();
        created.next();
        return created;
    }

    fn match_kw_or_ident(&mut self) -> Token {
        let mut identifier = String::new();

        while let Some(c) = self.peek_char() {
            if c.is_alphanumeric() || c == '_' {
                identifier.push(c);
                self.advance_char();
            } else {
                break;
            }
        }
        match identifier.as_str() {
            "u8" => Token::U8,
            "u16" => Token::U16,
            "u32" => Token::U32,
            "u64" => Token::U64,
            "fun" => Token::Fun,
            "if" => Token::If,
            "else" => Token::Else,
            "while" => Token::While,
            _ => Token::Identifier(identifier),
        }
    }

    pub fn peek(&self) -> Token {
        self.current_token.clone().unwrap_or(Token::Eof)
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
        match self.peek_char() {
            None => tok_false,
            Some(char) => {
                if char == expected {
                    self.advance_char();
                    tok_true
                } else {
                    tok_false
                }
            }
        }
    }

    fn lex(&mut self) -> Token {
        while self.peek_char().is_some() && self.peek_char().unwrap().is_whitespace() {
            self.advance_char();
        }

        let token = if let Some(char) = self.peek_char() {
            match char {
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
                '!' => match self.peek_char() {
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
                },
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
        token
    }

    pub fn next(&mut self) -> Token {
        let next_token = self.lex();
        self.current_token.replace(next_token).unwrap_or(Token::Eof)
    }

    pub fn lex_all(&mut self) -> Vec<Token> {
        let mut tokens: Vec<Token> = Vec::new();
        loop {
            let next_token = self.next();
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

        tokens
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn assert_single_tokenization(input_str: &str, expected_token: Token) {
        println!(
            "Assert single tokenization against {} == {}",
            input_str, expected_token
        );
        let result = Lexer::new(String::from(input_str).chars()).lex_all();
        assert_eq!(result, vec! {expected_token, Token::Eof});
    }

    #[test]
    fn tokenize_l_curly() {
        assert_single_tokenization("{", Token::LCurly);
    }

    #[test]
    fn tokenize_r_curly() {
        assert_single_tokenization("}", Token::RCurly);
    }

    #[test]
    fn tokenize_identifier() {
        assert_single_tokenization("abc", Token::Identifier(String::from("abc")));
        assert_single_tokenization("abc123", Token::Identifier(String::from("abc123")));
        assert_single_tokenization("abc123def", Token::Identifier(String::from("abc123def")));
        assert_single_tokenization(
            "unsignedOrSomething_123",
            Token::Identifier(String::from("unsignedOrSomething_123")),
        );
        assert_single_tokenization(
            "u16_named_fred",
            Token::Identifier(String::from("u16_named_fred")),
        );
    }

    #[test]
    fn tokenize_unsigned_decimal_constant() {
        assert_single_tokenization("123", Token::UnsignedDecimalConstant(123));
        assert_single_tokenization(
            &usize::MAX.to_string(),
            Token::UnsignedDecimalConstant(usize::MAX),
        );
        assert_single_tokenization(
            &usize::MIN.to_string(),
            Token::UnsignedDecimalConstant(usize::MIN),
        );
    }
}
