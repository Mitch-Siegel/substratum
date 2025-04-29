use super::token::*;

#[cfg(test)]
fn assert_single_tokenization(input_str: &str, expected_token: Token) {
    use crate::frontend::lexer::*;

    println!(
        "Assert single tokenization against {} == {}",
        input_str, expected_token
    );
    let result = Lexer::from_string(input_str).lex_all().expect("");
    assert_eq!(
        result,
        vec! {(expected_token, SourceLoc::new(1, 1)), (Token::Eof, SourceLoc::new(1, 1 + input_str.len()))}
    );
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
