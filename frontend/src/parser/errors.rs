use crate::{
    lexer::{LexError, token::Token},
    loc::Loc,
};

#[derive(Clone)]
pub(crate) enum ParseError {
    LexError(Box<LexError>),
    UnexpectedToken(Box<UnexpectedTokenError>),
}

impl PartialEq for ParseError {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::LexError(a), Self::LexError(b)) => a == b,
            (Self::UnexpectedToken(a), Self::UnexpectedToken(b)) => a == b,
            (_, _) => false,
        }
    }
}

impl std::fmt::Display for ParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::LexError(lex_error) => write!(f, "{lex_error}"),
            Self::UnexpectedToken(unexpected_token) => {
                write!(
                    f,
                    "unexpected token '{}' at {}, expected one of [{}] (wile parsing {} starting at {}) (error generated at {})",
                    unexpected_token.got,
                    unexpected_token.loc,
                    unexpected_token
                        .expected
                        .iter()
                        .map(std::string::ToString::to_string)
                        .collect::<Vec<String>>()
                        .join(", "),
                    unexpected_token.while_parsing,
                    unexpected_token.while_parsing_start,
                    unexpected_token.parser_source_location
                )
            }
        }
    }
}

impl std::fmt::Debug for ParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{self}")
    }
}

#[derive(Debug, Clone, Eq)]
pub(crate) struct UnexpectedTokenError {
    pub loc: Loc,
    pub got: Token,
    pub expected: Vec<Token>,
    pub while_parsing: String,
    pub while_parsing_start: Loc,
    pub parser_source_location: Loc,
}

impl PartialEq for UnexpectedTokenError {
    fn eq(&self, other: &Self) -> bool {
        self.loc == other.loc
            && self.got == other.got
            && self.expected == other.expected
            && self.while_parsing == other.while_parsing
            && self.while_parsing_start == other.while_parsing_start
    }
}

impl ParseError {
    pub(crate) fn unexpected_token(
        loc: Loc,
        got: Token,
        expected: &[Token],
        while_parsing: String,
        while_parsing_start: Loc,
        parser_source_location: Loc,
    ) -> Self {
        Self::UnexpectedToken(Box::new(UnexpectedTokenError {
            loc,
            got,
            expected: expected.to_vec(),
            while_parsing,
            while_parsing_start,
            parser_source_location,
        }))
    }
}

impl From<LexError> for ParseError {
    fn from(value: LexError) -> Self {
        Self::LexError(Box::new(value))
    }
}

#[cfg(test)]
mod tests {
    use crate::frontend::{
        lexer::{LexError, Token},
        loc::{SourcePoint, oc},
        parser::ParseError,
    };

    #[test]
    fn parse_error_fmt() {
        let lex_error =
            LexError::unexpected_eof(Loc::new(String::new(), SourcePoint { line: 1, col: 1 }));
        assert_eq!(
            format!("{}", lex_error),
            format!("{}", ParseError::from(lex_error))
        );

        let unexpected_token = ParseError::unexpected_token(
            Loc::new(String::new(), SourcePoint { line: 2, col: 3 }),
            Token::U8,
            &[Token::U16, Token::U32],
            "something".into(),
            Loc::new(String::new(), SourcePoint { line: 1, col: 1 }),
            Loc::new(String::new(), SourcePoint { line: 1, col: 1 }),
        );

        assert_eq!(
            format!("{}", unexpected_token),
            format!(
                "Unexpected token '{}' at {}, expected one of ['{}', '{}'] (while parsing {} starting at {}) (error generated at :1:1)",
                Token::U8,
                Loc::new(String::new(), SourcePoint { line: 2, col: 3 }),
                Token::U16,
                Token::U32,
                "something",
                Loc::new(String::new(), SourcePoint { line: 1, col: 1 })
            )
        );
    }
}
