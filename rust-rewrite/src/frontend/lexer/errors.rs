use crate::frontend::sourceloc::SourceLoc;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum LexError {
    InvalidChar(InvalidCharError),
    UnexpectedEof(UnexpectedEofError),
}

impl std::fmt::Display for LexError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::InvalidChar(invalid_char) => {
                write!(f, "Invalid char {} at {}", invalid_char.c, invalid_char.pos)
            }
            Self::UnexpectedEof(unexpected_eof) => {
                write!(f, "Unexpected EOF at {}", unexpected_eof.pos)
            }
        }
    }
}

impl LexError {
    pub fn invalid_char(c: char, pos: SourceLoc) -> Self {
        Self::InvalidChar(InvalidCharError { c, pos })
    }

    pub fn unexpected_eof(pos: SourceLoc) -> Self {
        Self::UnexpectedEof(UnexpectedEofError { pos })
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct InvalidCharError {
    c: char,
    pos: SourceLoc,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct UnexpectedEofError {
    pos: SourceLoc,
}
