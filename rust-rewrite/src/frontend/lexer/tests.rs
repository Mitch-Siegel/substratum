use crate::frontend::lexer::Lexer;

use super::token::Token;

#[test]
fn peek_none() {
    let lexer = Lexer::from_string(&"");
    assert!(lexer.peek_char() == None);
}

#[test]
fn peek() {
    let lexer = Lexer::from_string(&"qwerty");
    assert!(lexer.peek_char() == Some('q'));
}

#[test]
fn advance_char() {
    let mut lexer = Lexer::from_string(&"ab");
    assert_eq!(lexer.peek_char(), Some('a'));
    lexer.advance_char();
    assert_eq!(lexer.peek_char(), Some('b'));
}

#[test]
fn advance_to_end() {
    let mut lexer = Lexer::from_string(&"a");
    lexer.advance_char();
    assert!(lexer.peek_char() == None);
}

#[test]
fn test_loc_chars() {
    let mut lexer = Lexer::from_string(&"the quick brown\nfox jumps\nover the lazy\ndog\n\n");

    let mut line_lengths = Vec::new();
    let mut cols = 0;

    while lexer.peek_char().is_some() {
        let examined = lexer.peek_char().unwrap();
        lexer.advance_char();
        match examined {
            '\n' => {
                line_lengths.push(cols);
                cols = 0;
            }
            _ => {
                cols += 1;
            }
        }
    }

    assert_eq!(line_lengths, vec![15, 9, 13, 3, 0]);
}

// helper function - do some basic prefixing and suffixing
// ensure keyword matching behaves as expected with alpha/num pre/suffixes
fn kw_or_ident(string: &str, expected_token: Token) {
    let mut positive_match = Lexer::from_string(&string);

    let matched = positive_match.match_kw_or_ident();
    assert_eq!(matched, Some(expected_token.clone()));

    let prefix_alpha = "a".to_owned() + string;
    let mut negative_match = Lexer::from_string(&&prefix_alpha);
    assert_eq!(
        negative_match.match_kw_or_ident(),
        Some(Token::Identifier(prefix_alpha))
    );

    let prefix_num = "1".to_owned() + string;
    negative_match = Lexer::from_string(&&prefix_num);
    assert_ne!(negative_match.match_kw_or_ident(), Some(expected_token));

    let suffix_alpha = string.to_owned() + "a";
    negative_match = Lexer::from_string(&&suffix_alpha);
    assert_eq!(
        negative_match.match_kw_or_ident(),
        Some(Token::Identifier(suffix_alpha))
    );

    let suffix_num = string.to_owned() + "1";
    negative_match = Lexer::from_string(&&suffix_num);
    assert_eq!(
        negative_match.match_kw_or_ident(),
        Some(Token::Identifier(suffix_num))
    );
}

// test every token
#[test]
fn kw_u8() {
    kw_or_ident("u8", Token::U8);
}

#[test]
fn kw_u16() {
    kw_or_ident("u16", Token::U16);
}

#[test]
fn kw_u32() {
    kw_or_ident("u32", Token::U32);
}

#[test]
fn kw_u64() {
    kw_or_ident("u64", Token::U64);
}

#[test]
fn kw_i8() {
    kw_or_ident("i8", Token::I8);
}

#[test]
fn kw_i16() {
    kw_or_ident("i16", Token::I16);
}

#[test]
fn kw_i32() {
    kw_or_ident("i32", Token::I32);
}

#[test]
fn kw_i64() {
    kw_or_ident("i64", Token::I64);
}

#[test]
fn kw_mod() {
    kw_or_ident("mod", Token::Mod);
}

#[test]
fn kw_fun() {
    kw_or_ident("fun", Token::Fun);
}

#[test]
fn kw_if() {
    kw_or_ident("if", Token::If);
}

#[test]
fn kw_else() {
    kw_or_ident("else", Token::Else);
}

#[test]
fn kw_while() {
    kw_or_ident("while", Token::While);
}

#[test]
fn kw_pub() {
    kw_or_ident("pub", Token::Pub);
}

#[test]
fn kw_struct() {
    kw_or_ident("struct", Token::Struct);
}

#[test]
fn ident() {
    // test out some basic identifiers - such as ones containing keywords
    kw_or_ident("foobar", Token::Identifier("foobar".to_owned()));
    kw_or_ident("the_u8", Token::Identifier("the_u8".to_owned()));
    kw_or_ident("big_if_true", Token::Identifier("big_if_true".to_owned()));

    // make sure that we can correctly parse the end of identifiers
    let space_after = "space_after abcde";
    let space_after_ident = Token::Identifier(String::from("space_after"));
    let mut positive_match = Lexer::from_string(&space_after);
    assert_eq!(
        positive_match.match_kw_or_ident(),
        Some(space_after_ident.clone())
    );

    let prefix_alpha = "a".to_owned() + space_after;
    let mut negative_match = Lexer::from_string(&&prefix_alpha);
    assert_eq!(
        negative_match.match_kw_or_ident(),
        Some(Token::Identifier(String::from("aspace_after")))
    );

    let prefix_num = "1".to_owned() + space_after;
    negative_match = Lexer::from_string(&&prefix_num);
    assert_ne!(
        negative_match.match_kw_or_ident(),
        Some(space_after_ident.clone())
    );

    let suffix_alpha = space_after.to_owned() + "a";
    negative_match = Lexer::from_string(&&suffix_alpha);
    assert_eq!(
        negative_match.match_kw_or_ident(),
        Some(space_after_ident.clone())
    );

    let suffix_num = space_after.to_owned() + "1";
    negative_match = Lexer::from_string(&&suffix_num);
    assert_eq!(negative_match.match_kw_or_ident(), Some(space_after_ident));
}

#[test]
fn token_display_to_token() {
    let tokens = vec![
        Token::U8,
        Token::U16,
        Token::U32,
        Token::U64,
        Token::I8,
        Token::I16,
        Token::I32,
        Token::I64,
        Token::SelfLower,
        Token::SelfUpper,
        Token::Reference,
        Token::Mut,
        Token::Plus,
        Token::Minus,
        Token::Star,
        Token::FSlash,
        Token::GThan,
        Token::GThanE,
        Token::LThan,
        Token::LThanE,
        Token::Equals,
        Token::NotEquals,
        Token::Assign,
        Token::Fun,
        Token::If,
        Token::Else,
        Token::While,
        Token::Pub,
        Token::Struct,
        Token::Impl,
        Token::LParen,
        Token::RParen,
        Token::Arrow,
        Token::LCurly,
        Token::RCurly,
        Token::Comma,
        Token::Semicolon,
        Token::Colon,
    ];

    for token in tokens {
        let lex_result = Lexer::from_string(&format!("{}", token))
            .lex_all()
            .expect("");
        assert_eq!(lex_result.len(), 2);
        assert_eq!(token, lex_result[0].0);
    }
}
