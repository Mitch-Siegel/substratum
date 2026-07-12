use std::fmt::Display;

#[derive(Clone, Debug)]
pub(crate) enum Token {
    U8,
    U16,
    U32,
    U64,
    I8,
    I16,
    I32,
    I64,
    SelfLower,
    SelfUpper,
    Reference,
    Mut,
    Plus,
    Minus,
    Star,
    FSlash,
    LThan,
    GThan,
    LThanE,
    GThanE,
    Equals,
    NotEquals,
    Assign,
    Mod,
    Fn_,
    Let,
    If,
    Else,
    Match,
    Pub,
    While,
    Struct,
    Enum,
    Impl,
    LParen,
    RParen,
    Arrow,
    FatArrow,
    LCurly,
    RCurly,
    LBracket,
    RBracket,
    Comma,
    Dot,
    Semicolon,
    Colon,
    PathSep,
    Super,
    Identifier(String),
    UnsignedDecimalConstant(usize),
    Eof,
}

impl PartialEq for Token {
    fn eq(&self, other: &Self) -> bool {
        matches!(
            (self, other),
            (Token::U8, Token::U8)
                | (Token::U16, Token::U16)
                | (Token::U32, Token::U32)
                | (Token::U64, Token::U64)
                | (Token::I8, Token::I8)
                | (Token::I16, Token::I16)
                | (Token::I32, Token::I32)
                | (Token::I64, Token::I64)
                | (Token::SelfLower, Token::SelfLower)
                | (Token::SelfUpper, Token::SelfUpper)
                | (Token::Reference, Token::Reference)
                | (Token::Mut, Token::Mut)
                | (Token::Plus, Token::Plus)
                | (Token::Minus, Token::Minus)
                | (Token::Star, Token::Star)
                | (Token::FSlash, Token::FSlash)
                | (Token::LThan, Token::LThan)
                | (Token::GThan, Token::GThan)
                | (Token::LThanE, Token::LThanE)
                | (Token::GThanE, Token::GThanE)
                | (Token::Equals, Token::Equals)
                | (Token::NotEquals, Token::NotEquals)
                | (Token::Assign, Token::Assign)
                | (Token::Mod, Token::Mod)
                | (Token::Fn_, Token::Fn_)
                | (Token::Let, Token::Let)
                | (Token::If, Token::If)
                | (Token::Else, Token::Else)
                | (Token::Match, Token::Match)
                | (Token::While, Token::While)
                | (Token::Pub, Token::Pub)
                | (Token::Struct, Token::Struct)
                | (Token::Enum, Token::Enum)
                | (Token::Impl, Token::Impl)
                | (Token::LParen, Token::LParen)
                | (Token::RParen, Token::RParen)
                | (Token::Arrow, Token::Arrow)
                | (Token::FatArrow, Token::FatArrow)
                | (Token::LCurly, Token::LCurly)
                | (Token::RCurly, Token::RCurly)
                | (Token::LBracket, Token::LBracket)
                | (Token::RBracket, Token::RBracket)
                | (Token::Comma, Token::Comma)
                | (Token::Dot, Token::Dot)
                | (Token::Semicolon, Token::Semicolon)
                | (Token::Colon, Token::Colon)
                | (Token::PathSep, Token::PathSep)
                | (Token::Super, Token::Super)
                | (Token::Identifier(_), Token::Identifier(_))
                | (
                    Token::UnsignedDecimalConstant(_),
                    Token::UnsignedDecimalConstant(_)
                )
                | (Token::Eof, Token::Eof)
        )
    }
}

impl Eq for Token {}

impl Token {
    pub(crate) fn name(&self) -> &str {
        match self {
            Self::U8 => "u8",
            Self::U16 => "u16",
            Self::U32 => "u32",
            Self::U64 => "u64",
            Self::I8 => "i8",
            Self::I16 => "i16",
            Self::I32 => "i32",
            Self::I64 => "i64",
            Self::SelfLower => "self",
            Self::SelfUpper => "Self",
            Self::Reference => "&",
            Self::Mut => "mut",
            Self::Plus => "+",
            Self::Minus => "-",
            Self::Star => "*",
            Self::FSlash => "/",
            Self::GThan => ">",
            Self::GThanE => ">=",
            Self::LThan => "<",
            Self::LThanE => "<=",
            Self::Equals => "==",
            Self::NotEquals => "!=",
            Self::Assign => "=",
            Self::Mod => "mod",
            Self::Fn_ => "fun",
            Self::Let => "let",
            Self::If => "if",
            Self::Else => "else",
            Self::Match => "match",
            Self::While => "while",
            Self::Pub => "pub",
            Self::Struct => "struct",
            Self::Enum => "enum",
            Self::Impl => "impl",
            Self::LParen => "(",
            Self::RParen => ")",
            Self::Arrow => "->",
            Self::FatArrow => "=>",
            Self::LCurly => "{",
            Self::RCurly => "}",
            Self::LBracket => "[",
            Self::RBracket => "]",
            Self::Comma => ",",
            Self::Dot => ".",
            Self::Semicolon => ";",
            Self::Colon => ":",
            Self::PathSep => "::",
            Self::Super => "super",
            Self::Identifier(_) => "identifier",
            Self::UnsignedDecimalConstant(_) => "unsigned decimal constant",
            Self::Eof => "EOF",
        }
    }
}

impl Display for Token {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::U8 => write!(f, "u8"),
            Self::U16 => write!(f, "u16"),
            Self::U32 => write!(f, "u32"),
            Self::U64 => write!(f, "u64"),
            Self::I8 => write!(f, "i8"),
            Self::I16 => write!(f, "i16"),
            Self::I32 => write!(f, "i32"),
            Self::I64 => write!(f, "i64"),
            Self::SelfLower => write!(f, "self"),
            Self::SelfUpper => write!(f, "Self"),
            Self::Reference => write!(f, "&"),
            Self::Mut => write!(f, "mut"),
            Self::Plus => write!(f, "+"),
            Self::Minus => write!(f, "-"),
            Self::Star => write!(f, "*"),
            Self::FSlash => write!(f, "/"),
            Self::GThan => write!(f, ">"),
            Self::GThanE => write!(f, ">="),
            Self::LThan => write!(f, "<"),
            Self::LThanE => write!(f, "<="),
            Self::Equals => write!(f, "=="),
            Self::NotEquals => write!(f, "!="),
            Self::Assign => write!(f, "="),
            Self::Mod => write!(f, "mod"),
            Self::Fn_ => write!(f, "fun"),
            Self::Let => write!(f, "let"),
            Self::If => write!(f, "if"),
            Self::Else => write!(f, "else"),
            Self::Match => write!(f, "match"),
            Self::While => write!(f, "while"),
            Self::Pub => write!(f, "pub"),
            Self::Struct => write!(f, "struct"),
            Self::Enum => write!(f, "enum"),
            Self::Impl => write!(f, "impl"),
            Self::LParen => write!(f, "("),
            Self::RParen => write!(f, ")"),
            Self::Arrow => write!(f, "->"),
            Self::FatArrow => write!(f, "=>"),
            Self::LCurly => write!(f, "{{"),
            Self::RCurly => write!(f, "}}"),
            Self::LBracket => write!(f, "["),
            Self::RBracket => write!(f, "]"),
            Self::Comma => write!(f, ","),
            Self::Dot => write!(f, "."),
            Self::Semicolon => write!(f, ";"),
            Self::Colon => write!(f, ":"),
            Self::PathSep => write!(f, "::"),
            Self::Super => write!(f, "super"),
            Self::Identifier(string) => write!(f, "Identifier({})", string),
            Self::UnsignedDecimalConstant(constant) => {
                write!(f, "UnsignedDecimalConstant({})", constant)
            }
            Self::Eof => write!(f, "EOF"),
        }
    }
}
