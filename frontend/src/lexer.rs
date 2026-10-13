use crate::loc::{Point, PointSpan};

mod char_source;
pub(crate) mod errors;
#[cfg(test)]
mod integration_tests;
#[cfg(test)]
mod tests;
pub(crate) mod token;

pub(crate) use errors::LexError;
pub(crate) use token::Token;

use char_source::CharSource;

mod loc_source {
    use super::char_source::CharSource;
    use crate::loc::Point;

    #[derive(Debug)]
    pub(super) struct CharPointSource<'a> {
        char_source: CharSource<'a>,
        // line, col at which peek() returns a char from
        peek_char: Option<(char, Point)>,
    }

    impl<'a> CharPointSource<'a> {
        pub(super) fn new(mut char_source: CharSource<'a>) -> Self {
            let first_char = char_source.next().map(|c| (c, Point::new(1, 1)));

            Self {
                char_source,
                peek_char: first_char,
            }
        }

        fn calc_new_point(old: Point, c: char) -> Point {
            let Point { mut line, mut col } = old;
            if c == '\n' {
                line += 1;
                col = 1;
            } else {
                col += 1;
            }
            Point::new(line, col)
        }

        pub(super) fn peek(&self) -> Option<(char, Point)> {
            self.peek_char
        }

        pub(super) fn advance(&mut self) -> Option<(char, Point)> {
            let (c, point) = self.peek_char?;

            self.peek_char = self
                .char_source
                .next()
                .map(|next_c| (next_c, Self::calc_new_point(point, c)));

            Some((c, point))
        }
    }
}

use loc_source::CharPointSource;

#[derive(Debug)]
pub(crate) struct Lexer<'a> {
    char_source: CharPointSource<'a>,
    last_consumed_point: Option<Point>,
    current_token: Option<(Token, PointSpan)>,
}

// public methods:
impl<'a> Lexer<'a> {
    pub(crate) fn from_char_source(char_source: CharSource<'a>) -> Self {
        Self {
            char_source: CharPointSource::new(char_source),
            last_consumed_point: None,
            current_token: None,
        }
    }

    pub(crate) fn from_file(f: std::fs::File) -> Self {
        Self::from_char_source(CharSource::from_file(f))
    }

    #[allow(dead_code)]
    pub(crate) fn from_string(s: &'a str) -> Self {
        Self::from_char_source(CharSource::from_str(s))
    }

    pub(crate) fn peek(&mut self) -> Result<(Token, PointSpan), LexError> {
        if self.current_token.is_none() {
            self.current_token = Some(self.lex()?);
        }

        let peeked = if let Some(t) = self.current_token.clone() {
            t
        } else {
            (Token::Eof, self.last_consumed_point().into())
        };

        #[cfg(feature = "loud_lexing")]
        println!("Lexer::peek() -> {peeked:?}");

        Ok(peeked)
    }

    // returns the position to which the input has been read
    pub(crate) fn last_consumed_point(&self) -> Point {
        self.last_consumed_point.unwrap_or(Point::new(0, 0))
    }

    pub(crate) fn next(&mut self) -> Result<(Token, PointSpan), LexError> {
        let _ = trace::span_auto!(trace::Level::TRACE, "");
        let next_token = self.lex()?;
        Ok(self
            .current_token
            .replace(next_token)
            .unwrap_or((Token::Eof, self.last_consumed_point().into())))
    }

    #[allow(dead_code)]
    pub(crate) fn lex_all(&mut self) -> Result<Vec<(Token, PointSpan)>, LexError> {
        println!("Lexer::lex_all()");
        let mut tokens: Vec<(Token, PointSpan)> = Vec::new();
        if self.current_token.is_none() {
            tokens.push(self.lex()?);
        }

        loop {
            let next_token = self.next()?;
            println!("next_token: {next_token:?}");
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

        println!("Lexer::lex_all(): {tokens:?}");

        Ok(tokens)
    }
}

// private methods
impl Lexer<'_> {
    fn peek_char(&self) -> Option<char> {
        let c = self.char_source.peek();
        // #[cfg(feature = "loud_lexing")]
        // println!("Lexer::peek_char: {:?}", c);

        c.map(|(c, _)| c)
    }

    fn advance_char(&mut self) -> Point {
        let (_, point) = self.char_source.advance().unwrap();
        self.last_consumed_point.replace(point);
        // #[cfg(feature = "loud_lexing")]
        // println!("Lexer::advance_char: {:?}", c);
        point
    }

    fn match_kw_or_ident(&mut self) -> Option<(Token, Point)> {
        #[cfg(feature = "loud_lexing")]
        println!("Lexer::match_kw_or_ident");

        let mut identifier = String::new();

        let matched = if let Some(first_char) = self.peek_char()
            && (first_char.is_alphabetic() || first_char == '_')
        {
            identifier.push(first_char);
            let mut end_point = self.advance_char();
            while let Some(c) = self.peek_char() {
                if c.is_alphanumeric() || c == '_' {
                    identifier.push(c);
                    end_point = self.advance_char();
                } else {
                    break;
                }
            }

            let maybe_tok = match identifier.as_str() {
                "u8" => Some(Token::U8),
                "u16" => Some(Token::U16),
                "u32" => Some(Token::U32),
                "u64" => Some(Token::U64),
                "i8" => Some(Token::I8),
                "i16" => Some(Token::I16),
                "i32" => Some(Token::I32),
                "i64" => Some(Token::I64),
                "mod" => Some(Token::Mod),
                "fun" => Some(Token::Fn_),
                "if" => Some(Token::If),
                "else" => Some(Token::Else),
                "match" => Some(Token::Match),
                "while" => Some(Token::While),
                "pub" => Some(Token::Pub),
                "struct" => Some(Token::Struct),
                "enum" => Some(Token::Enum),
                "impl" => Some(Token::Impl),
                "mut" => Some(Token::Mut),
                "self" => Some(Token::SelfLower),
                "Self" => Some(Token::SelfUpper),
                "let" => Some(Token::Let),
                "super" => Some(Token::Super),
                _ => Some(Token::Identifier(identifier)),
            };

            maybe_tok.map(|t| (t, end_point))
        } else {
            None
        };

        #[cfg(feature = "loud_lexing")]
        println!("Lexer::match_kw_or_ident: matched {matched:?}");

        matched
    }

    fn match_next_char_for_token_or(
        &mut self,
        expected: char,
        tok_true: Token,
        false_output: (Token, Point),
    ) -> (Token, Point) {
        #[cfg(feature = "loud_lexing")]
        println!(
            "Lexer::match_next_char_for_token_or: expected: {expected}, true: {tok_true}, false: {}@{}",
            false_output.0, false_output.1
        );

        match self.peek_char() {
            None => false_output,
            Some(peeked_char) => {
                if peeked_char == expected {
                    (tok_true, self.advance_char())
                } else {
                    false_output
                }
            }
        }
    }

    fn trim_whitespace(&mut self) -> Point {
        let mut ret = self.last_consumed_point();
        while let Some(c) = self.peek_char() {
            if c.is_whitespace() {
                ret = self.advance_char();
            } else {
                return ret;
            }
        }
        ret
    }

    #[allow(clippy::too_many_lines)]
    fn lex(&mut self) -> Result<(Token, PointSpan), LexError> {
        #[cfg(feature = "loud_lexing")]
        println!("Lexer::lex()");

        self.trim_whitespace();
        let match_start = self.char_source.peek().map(|(_, p)| p);

        let (tok, match_end) = if let Some(peeked_char) = self.peek_char() {
            match peeked_char {
                '{' => Ok((Token::LCurly, self.advance_char())),
                '}' => Ok((Token::RCurly, self.advance_char())),
                '[' => Ok((Token::LBracket, self.advance_char())),
                ']' => Ok((Token::RBracket, self.advance_char())),
                '(' => Ok((Token::LParen, self.advance_char())),
                ')' => Ok((Token::RParen, self.advance_char())),
                '&' => Ok((Token::Reference, self.advance_char())),
                '+' => Ok((Token::Plus, self.advance_char())),
                '-' => {
                    let end = self.advance_char();
                    Ok(self.match_next_char_for_token_or('>', Token::Arrow, (Token::Minus, end)))
                }
                '*' => Ok((Token::Star, self.advance_char())),
                '/' => Ok((Token::FSlash, self.advance_char())),
                '>' => {
                    let end = self.advance_char();
                    Ok(self.match_next_char_for_token_or('=', Token::GThanE, (Token::GThan, end)))
                }
                '<' => {
                    let end = self.advance_char();
                    Ok(self.match_next_char_for_token_or('=', Token::LThanE, (Token::LThan, end)))
                }
                '!' => {
                    let end = self.advance_char();
                    match self.peek_char() {
                        None => Err(LexError::unexpected_eof(match_start.unwrap())),
                        Some(character) => {
                            if character == '=' {
                                Ok((Token::NotEquals, self.advance_char()))
                            } else {
                                Err(LexError::invalid_char(character, end))
                            }
                        }
                    }
                }
                '=' => {
                    let end = self.advance_char();
                    Ok(match self.peek_char() {
                        Some('=') => (Token::Equals, self.advance_char()),
                        Some('>') => (Token::FatArrow, self.advance_char()),
                        _ => (Token::Assign, end),
                    })
                }
                ',' => Ok((Token::Comma, self.advance_char())),
                '.' => Ok((Token::Dot, self.advance_char())),
                ';' => Ok((Token::Semicolon, self.advance_char())),
                ':' => {
                    let end = self.advance_char();
                    Ok(self.match_next_char_for_token_or(':', Token::PathSep, (Token::Colon, end)))
                }
                '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' => {
                    let mut constant_string = String::new();
                    let mut end: Point = match_start.unwrap();
                    while let Some(c) = self.peek_char()
                        && c.is_numeric()
                    {
                        constant_string.push(c);
                        end = self.advance_char();
                    }
                    let tok = Token::UnsignedDecimalConstant(
                        constant_string
                            .parse::<usize>()
                            .expect("Couldn't convert unsigned decimal constant"),
                    );

                    Ok((tok, end))
                }
                _ => match self.match_kw_or_ident() {
                    Some(token) => Ok(token),
                    None => Err(LexError::invalid_char(
                        self.peek_char().unwrap(),
                        self.last_consumed_point(),
                    )),
                },
            }
        } else {
            Ok((Token::Eof, self.last_consumed_point()))
        }?;

        self.trim_whitespace();

        #[cfg(feature = "loud_lexing")]
        println!(
            "Lexer::lex(): lexed '{tok}'@{}",
            match_start.unwrap_or(match_end)
        );
        Ok((
            tok,
            PointSpan::new(match_start.unwrap_or(match_end), match_end),
        ))
    }
}
