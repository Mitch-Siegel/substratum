use crate::frontend::parser::parse_rules::*;

mod enum_definition;
mod function;
mod generics;
mod implementation;
mod module_item;
mod struct_definition;

impl<'a, 'p> ItemParser<'a, 'p> {
    fn parse_implementation_item(&mut self) -> Result<ItemTree, ParseError> {
        Ok(ItemTree::Implementation(self.parse_implementation()?))
    }

    fn parse_enum_definition_item(&mut self) -> Result<ItemTree, ParseError> {
        Ok(ItemTree::EnumDefinition(self.parse_enum_definition()?))
    }

    fn parse_struct_definition_item(&mut self) -> Result<ItemTree, ParseError> {
        Ok(ItemTree::StructDefinition(self.parse_struct_definition()?))
    }

    pub(crate) fn parse_item(
        &mut self,
        module_name: IdentifierTree,
        module_path: &std::path::Path,
        crate_name: &Option<String>,
    ) -> Result<ItemTree, ParseError> {
        match self.peek_token()? {
            Token::Fn_ => self.parse_function_declaration_or_definition(),
            Token::Struct => self.parse_struct_definition_item(),
            Token::Enum => self.parse_enum_definition_item(),
            Token::Impl => self.parse_implementation_item(),
            // TODO: break out to separate routine
            Token::Mod => {
                let current_parsing_module_path = module_path.join(module_name.value);
                match self.lookahead_token(2)? {
                    Token::LCurly => {
                        let parse_rules::module::ModuleResult {
                            module_tree,
                            module_worklist: child_worklist,
                        } = self.parse_module_item(&current_parsing_module_path, crate_name)?;
                        Ok(ItemTree::Module((Ok(module_tree), child_worklist)))
                    }
                    Token::Semicolon => {
                        let mod_loc = self.expect_token(Token::Mod)?;
                        let module_name = self.parse_identifier()?;

                        let worklist_string: String = current_parsing_module_path
                            .clone()
                            .join(module_name.value)
                            .to_str()
                            .unwrap()
                            .into();
                        trace::debug!("Add module worklist string: \"{}\"", worklist_string);

                        let child_worklist: BTreeSet<String> =
                            std::iter::once(worklist_string).collect();
                        self.expect_token(Token::Semicolon)?;
                        Ok(ItemTree::Module((Err(mod_loc), child_worklist)))
                    }
                    _ => self.unexpected_token(&[Token::LCurly, Token::Mod])?,
                }
            }
            _ => self.unexpected_token(&[Token::Fn_, Token::Struct, Token::Impl])?,
        }
    }
}
