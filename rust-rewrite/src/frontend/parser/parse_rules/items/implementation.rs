use crate::frontend::parser::parse_rules::*;

impl<'a, 'p> ItemParser<'a, 'p> {
    pub fn parse_implementation(&mut self) -> Result<ast::items::ImplementationTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("impl block")?;

        self.expect_token(Token::Impl)?;
        let generic_params = self.try_parse_generic_params_list()?;
        let implemented_for = self.parse_identifier_with_generic_params()?;
        self.expect_token(Token::LCurly)?;

        let mut items: Vec<ast::items::FunctionDefinitionTree> = Vec::new();

        while self.peek_token()? != Token::RCurly {
            let prototype = self.parse_function_prototype(true)?;
            items.push(self.parse_function_definition(prototype)?);
        }

        self.expect_token(Token::RCurly)?;

        let implementation =
            ast::items::ImplementationTree::new(start_loc, generic_params, implemented_for, items);
        self.finish_parsing(&implementation)?;
        Ok(implementation)
    }
}
