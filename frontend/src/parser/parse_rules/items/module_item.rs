use crate::{
    lexer::Token,
    parser::{
        ParseError,
        parse_rules::{ItemParser, module},
    },
};

impl ItemParser<'_, '_> {
    pub(crate) fn parse_module_item(
        &mut self,
        parent_module_path: &std::path::Path,
        crate_name: Option<&String>,
    ) -> Result<module::ModuleResult, ParseError> {
        let (_, _span) = self.start_parsing("module item")?;

        trace::debug!(
            "module item parent module path: \"{}\"",
            parent_module_path.display()
        );

        let mod_keyword_loc = self.expect_token(Token::Mod)?;

        let name = self.parse_identifier()?;
        let name_raw = name.value.clone();
        self.expect_token(Token::LCurly)?;
        let module_result = self.module_parser().parse_module_contents(
            Some((mod_keyword_loc, name)),
            name_raw,
            parent_module_path,
            crate_name,
        )?;
        self.expect_token(Token::RCurly)?;

        self.finish_parsing(module_result)
    }
}
