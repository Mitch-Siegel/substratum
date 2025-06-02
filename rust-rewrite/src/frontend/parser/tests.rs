use crate::frontend::{
    lexer::{token::Token, Lexer},
    parser::{ParseError, Parser},
    sourceloc::SourceLoc,
};

#[test]
fn expect_token() {
    let mut p = Parser::new(Lexer::from_string("u8"));
    let result = p.expect_token(Token::U8);
    assert_eq!(result, Ok(Token::U8))
}

#[test]
fn expect_token_fail() {
    let mut p = Parser::new(Lexer::from_string("abcd"));
    let result = p.expect_token(Token::U8);
    assert_eq!(
        result,
        Err(ParseError::unexpected_token(
            SourceLoc::new(1, 1),
            Token::Identifier("abcd".into()),
            &[Token::U8]
        ))
    )
}
