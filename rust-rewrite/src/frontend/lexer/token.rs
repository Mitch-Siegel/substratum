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
            (Self::U8, Self::U8)
                | (Self::U16, Self::U16)
                | (Self::U32, Self::U32)
                | (Self::U64, Self::U64)
                | (Self::I8, Self::I8)
                | (Self::I16, Self::I16)
                | (Self::I32, Self::I32)
                | (Self::I64, Self::I64)
                | (Self::SelfLower, Self::SelfLower)
                | (Self::SelfUpper, Self::SelfUpper)
                | (Self::Reference, Self::Reference)
                | (Self::Mut, Self::Mut)
                | (Self::Plus, Self::Plus)
                | (Self::Minus, Self::Minus)
                | (Self::Star, Self::Star)
                | (Self::FSlash, Self::FSlash)
                | (Self::LThan, Self::LThan)
                | (Self::GThan, Self::GThan)
                | (Self::LThanE, Self::LThanE)
                | (Self::GThanE, Self::GThanE)
                | (Self::Equals, Self::Equals)
                | (Self::NotEquals, Self::NotEquals)
                | (Self::Assign, Self::Assign)
                | (Self::Mod, Self::Mod)
                | (Self::Fn_, Self::Fn_)
                | (Self::Let, Self::Let)
                | (Self::If, Self::If)
                | (Self::Else, Self::Else)
                | (Self::Match, Self::Match)
                | (Self::While, Self::While)
                | (Self::Pub, Self::Pub)
                | (Self::Struct, Self::Struct)
                | (Self::Enum, Self::Enum)
                | (Self::Impl, Self::Impl)
                | (Self::LParen, Self::LParen)
                | (Self::RParen, Self::RParen)
                | (Self::Arrow, Self::Arrow)
                | (Self::FatArrow, Self::FatArrow)
                | (Self::LCurly, Self::LCurly)
                | (Self::RCurly, Self::RCurly)
                | (Self::LBracket, Self::LBracket)
                | (Self::RBracket, Self::RBracket)
                | (Self::Comma, Self::Comma)
                | (Self::Dot, Self::Dot)
                | (Self::Semicolon, Self::Semicolon)
                | (Self::Colon, Self::Colon)
                | (Self::PathSep, Self::PathSep)
                | (Self::Super, Self::Super)
                | (Self::Identifier(_), Self::Identifier(_))
                | (
                    Self::UnsignedDecimalConstant(_),
                    Self::UnsignedDecimalConstant(_)
                )
                | (Self::Eof, Self::Eof)
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
            Self::Identifier(string) => write!(f, "Identifier({string})"),
            Self::UnsignedDecimalConstant(constant) => {
                write!(f, "UnsignedDecimalConstant({constant})")
            }
            Self::Eof => write!(f, "EOF"),
        }
    }
}
