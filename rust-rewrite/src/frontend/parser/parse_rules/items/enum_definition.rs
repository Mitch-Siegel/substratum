use crate::frontend::parser::parse_rules::*;

impl<'a, 'p> ItemParser<'a, 'p> {
    fn parse_tuple_enum_variant(
        &mut self,
    ) -> Result<ast::items::enum_definition::EnumVariantData, ParseError> {
        let (_start_loc, _span) = self.start_parsing("tuple enum variant")?;

        self.expect_token(Token::LParen)?;
        let mut tuple_elements = Vec::new();
        loop {
            tuple_elements.push(self.type_parser().parse_type()?);
            match self.peek_token()? {
                Token::Comma => {
                    self.expect_token(Token::Comma)?;
                }
                Token::RParen => {
                    break;
                }
                _ => self.unexpected_token(&[Token::Comma, Token::RParen])?,
            }
        }
        self.expect_token(Token::RParen)?;
        let tuple_data = ast::items::enum_definition::EnumVariantData::TupleData(tuple_elements);

        self.finish_parsing(tuple_data)
    }

    fn parse_enum_variant_data(
        &mut self,
    ) -> Result<Option<ast::items::enum_definition::EnumVariantDataTree>, ParseError> {
        let (start_loc, _span) = self.start_parsing("enum variant data")?;
        let variant_data = match self.peek_token()? {
            Token::LParen => Some(ast::items::enum_definition::EnumVariantDataTree {
                loc: start_loc,
                data: self.parse_tuple_enum_variant()?,
            }),
            _ => None,
        };

        self.finish_parsing(variant_data)
    }

    pub fn parse_enum_variant(
        &mut self,
    ) -> Result<ast::items::enum_definition::EnumVariantTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("enum variant")?;

        let name = self.parse_identifier()?;
        let variant_data = self.parse_enum_variant_data()?;

        let enum_variant_tree =
            ast::items::enum_definition::EnumVariantTree::new(start_loc, name, variant_data);

        self.finish_parsing(enum_variant_tree)
    }

    pub fn parse_enum_definition(
        &mut self,
    ) -> Result<ast::items::enum_definition::EnumDefinitionTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("enum definition")?;

        self.expect_token(Token::Enum)?;
        let name = self.parse_identifier()?;
        let generic_params = self.try_parse_generic_params_list()?;
        self.expect_token(Token::LCurly)?;

        let mut variants = Vec::<ast::items::enum_definition::EnumVariantTree>::new();
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

        let enum_definition_tree = ast::items::enum_definition::EnumDefinitionTree::new(
            start_loc,
            name,
            generic_params,
            variants,
        );
        self.finish_parsing(enum_definition_tree)
    }
}
