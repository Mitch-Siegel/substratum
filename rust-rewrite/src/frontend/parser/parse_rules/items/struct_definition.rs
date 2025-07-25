use crate::frontend::parser::{parse_rules::*, *};

impl<'a, 'p> ItemParser<'a, 'p> {
    fn parse_struct_field_declaration(
        &mut self,
    ) -> Result<ast::items::struct_definition::StructFieldTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("struct field")?;

        let field_name = self.parse_identifier()?;
        self.expect_token(Token::Colon)?;
        let field_type = self.parse_type()?;

        let field_tree =
            ast::items::struct_definition::StructFieldTree::new(start_loc, field_name, field_type);
        self.finish_parsing(&field_tree)?;
        Ok(field_tree)
    }

    pub fn parse_struct_definition(
        &mut self,
    ) -> Result<ast::items::StructDefinitionTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("struct definition")?;

        self.expect_token(Token::Struct)?;
        let struct_name = self.parse_identifier_with_generic_params()?;
        self.expect_token(Token::LCurly)?;

        let mut struct_fields = Vec::new();

        loop {
            match self.peek_token()? {
                Token::Identifier(_) => {
                    struct_fields.push(self.parse_struct_field_declaration()?);
                    if matches!(self.peek_token()?, Token::Comma) {
                        self.next_token()?;
                    }
                }
                Token::RCurly => {
                    self.next_token()?;
                    break;
                }
                _ => {
                    self.unexpected_token(&[Token::Identifier("".into())])?;
                }
            }
        }

        let struct_definition =
            ast::items::StructDefinitionTree::new(start_loc, struct_name, struct_fields);
        self.finish_parsing(&struct_definition)?;
        Ok(struct_definition)
    }
}
