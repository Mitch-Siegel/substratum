use crate::frontend::parser::parse_rules::*;

impl<'a, 'p> ItemParser<'a, 'p> {
    fn parse_struct_field_declaration(
        &mut self,
    ) -> Result<ast::items::struct_definition::StructFieldTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("struct field")?;

        let name = self.parse_identifier()?;
        self.expect_token(Token::Colon)?;
        let type_ = self.type_parser().parse_type()?;

        let field_tree = ast::items::struct_definition::StructFieldTree { name, type_ };
        self.finish_parsing(field_tree)
    }

    pub(crate) fn parse_struct_definition(
        &mut self,
    ) -> Result<ast::items::StructDefinitionTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("struct definition")?;

        let struct_keyword_loc = self.expect_token(Token::Struct)?;
        let name = self.parse_identifier()?;
        let generic_params = self.try_parse_generic_params_list()?;
        self.expect_token(Token::LCurly)?;

        let mut fields = Vec::new();

        loop {
            match self.peek_token()? {
                Token::Identifier(_) => {
                    fields.push(self.parse_struct_field_declaration()?);
                    if matches!(self.peek_token()?, Token::Comma) {
                        self.next_token()?;
                    }
                }
                Token::RCurly => {
                    break;
                }
                _ => {
                    self.unexpected_token(&[Token::Identifier("".into())])?;
                }
            }
        }

        let close_brace_loc = self.expect_token(Token::RCurly)?;

        let struct_definition = ast::items::StructDefinitionTree {
            struct_keyword_loc,
            name,
            generic_params,
            fields,
            close_brace_loc,
        };
        self.finish_parsing(struct_definition)
    }
}
