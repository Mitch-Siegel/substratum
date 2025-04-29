use crate::frontend::{
    lexer::{token::Token, LexError},
    sourceloc::SourceLoc,
};

#[derive(Clone, PartialEq, Eq)]
pub enum ParseError {
    LexError(LexError),
    UnexpectedToken(UnexpectedTokenError),
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
                    "Unexpected token '{}' at {}, expected one of [{}]",
                    unexpected_token.got, unexpected_token.loc, expected_tokens
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
}

impl ParseError {
    pub fn lex_error(e: LexError) -> Self {
        Self::LexError(e)
    }

    pub fn unexpected_token(loc: SourceLoc, got: Token, expected: &[Token]) -> Self {
        Self::UnexpectedToken(UnexpectedTokenError {
            loc,
            got,
            expected: expected.to_vec(),
        })
    }
}

impl From<LexError> for ParseError {
    fn from(value: LexError) -> Self {
        Self::LexError(value)
    }
}
