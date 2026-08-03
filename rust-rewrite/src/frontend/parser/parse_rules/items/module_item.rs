use crate::frontend::parser::parse_rules::*;

impl<'a, 'p> ItemParser<'a, 'p> {
    pub(in crate::frontend) fn parse_module_item(
        &mut self,
        parent_module_path: &std::path::Path,
        crate_name: &Option<String>,
    ) -> Result<parse_rules::module::ModuleResult, ParseError> {
        let (_, _span) = self.start_parsing("module item")?;

        trace::debug!(
            "module item parent module path: \"{}\"",
            parent_module_path.display()
        );

        let mod_keyword_loc = self.expect_token(Token::Mod)?;

        let name = self.parse_identifier()?;
        self.expect_token(Token::LCurly)?;
        let module_result = self.module_parser().parse_module_contents(
            mod_keyword_loc,
            parent_module_path,
            name,
            crate_name,
        )?;
        self.expect_token(Token::RCurly)?;

        self.finish_parsing(module_result)
    }
}
