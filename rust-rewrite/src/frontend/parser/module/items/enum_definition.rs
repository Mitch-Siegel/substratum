use crate::frontend::parser::*;

impl<'a> Parser<'a> {
    pub fn parse_enum_variant(&mut self) -> Result<EnumVariantTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("enum variant")?;

        let name = self.parse_identifier()?;
        let variant_data = match self.peek_token()? {
            Token::LParen => {
                self.expect_token(Token::LParen)?;
                let data_type = self.parse_type()?;
                self.expect_token(Token::RParen)?;
                Some(data_type)
            }
            Token::Comma | Token::RCurly => None,
            _ => self.unexpected_token(&[Token::LParen, Token::Comma, Token::RCurly])?,
        };

        let enum_variant_tree = EnumVariantTree::new(start_loc, name, variant_data);
        self.finish_parsing(&enum_variant_tree)?;
        Ok(enum_variant_tree)
    }

    pub fn parse_enum_definition(&mut self) -> Result<EnumDefinitionTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("enum definition")?;

        self.expect_token(Token::Enum)?;
        let name = self.parse_identifier_with_generic_params()?;
        self.expect_token(Token::LCurly)?;

        let mut variants = Vec::<EnumVariantTree>::new();
        loop {
            match self.peek_token()? {
                Token::Identifier(_) => {
                    let variant = self.parse_enum_variant()?;
                    variants.push(variant);
                    match self.peek_token()? {
                        Token::Comma => {
                            self.expect_token(Token::Comma)?;
                        }
                        Token::RCurly => break,
                        _ => (),
                    }
                }
                Token::RCurly => break,
                _ => self.unexpected_token(&[Token::Identifier("".into()), Token::RCurly])?,
            }
        }
        self.expect_token(Token::RCurly)?;

        let enum_definition_tree = EnumDefinitionTree::new(start_loc, name, variants);
        self.finish_parsing(&enum_definition_tree)?;
        Ok(enum_definition_tree)
    }
}
