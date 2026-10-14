use crate::{
    ast,
    lexer::Token,
    parser::{ParseError, parse_rules::ItemParser},
};

impl ItemParser<'_, '_> {
    pub(crate) fn parse_implementation(
        &mut self,
    ) -> Result<ast::items::ImplementationTree, ParseError> {
        let (_start, _span) = self.start_parsing("impl block")?;

        let impl_keyword = self.expect_token(Token::Impl)?;
        let generic_params = self.try_parse_generic_params_list()?;
        let for_ = self.parse_identifier()?;
        let implemented_for_generic_params = self.try_parse_generic_params_list()?;
        self.expect_token(Token::LCurly)?;

        let mut items: Vec<ast::items::FunctionDefinitionTree> = Vec::new();

        while self.peek_token()? != Token::RCurly {
            let prototype = self.parse_function_prototype(true)?;
            items.push(self.parse_function_definition(prototype)?);
        }

        let close_brace = self.expect_token(Token::RCurly)?;

        let implementation = ast::items::ImplementationTree {
            impl_keyword,
            generic_params,
            for_,
            implemented_for_generic_params,
            items,
            close_brace,
        };
        self.finish_parsing(implementation)
    }
}
