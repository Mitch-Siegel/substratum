use crate::frontend::{
    lexer::{token::Token, LexError},
    sourceloc::SourceLoc,
};

#[derive(Clone)]
pub enum ParseError {
    LexError(LexError),
    UnexpectedToken(UnexpectedTokenError),
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
            Self::LexError(lex_error) => write!(f, "{}", lex_error),
            Self::UnexpectedToken(unexpected_token) => {
                let mut expected_tokens = String::new();
                for tok in &unexpected_token.expected {
                    if expected_tokens.len() > 0 {
                        expected_tokens += ", ";
                    }

                    expected_tokens += &format!("'{}'", tok.name());
                }
                write!(
                    f,
                    "Unexpected token '{}' at {}, expected one of [{}] (while parsing {} starting at {})",
                    unexpected_token.got, unexpected_token.loc, expected_tokens, unexpected_token.while_parsing, unexpected_token.while_parsing_start
                )
            }
        }
    }
}

impl std::fmt::Debug for ParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct UnexpectedTokenError {
    pub loc: SourceLoc,
    pub got: Token,
    pub expected: Vec<Token>,
    pub while_parsing: String,
    pub while_parsing_start: SourceLoc,
}

impl ParseError {
    pub fn unexpected_token(
        loc: SourceLoc,
        got: Token,
        expected: &[Token],
        while_parsing: String,
        while_parsing_start: SourceLoc,
    ) -> Self {
        Self::UnexpectedToken(UnexpectedTokenError {
            loc,
            got,
            expected: expected.to_vec(),
            while_parsing,
            while_parsing_start,
        })
    }
}

impl From<LexError> for ParseError {
    fn from(value: LexError) -> Self {
        Self::LexError(value)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_error_fmt() {
        let lex_error = LexError::unexpected_eof(SourceLoc::new(1, 1));
        assert_eq!(
            format!("{}", lex_error),
            format!("{}", ParseError::from(lex_error))
        );

        let unexpected_token = ParseError::unexpected_token(
            SourceLoc::new(2, 3),
            Token::U8,
            &[Token::U16, Token::U32],
            "something".into(),
            SourceLoc::new(1, 1),
        );

        assert_eq!(
            format!("{}", unexpected_token),
            format!(
                "Unexpected token '{}' at {}, expected one of ['{}', '{}'] (while parsing {} starting at {})",
                Token::U8,
                SourceLoc::new(2, 3),
                Token::U16,
                Token::U32,
                "something",
                SourceLoc::new(1, 1)
            )
        );
    }
}
