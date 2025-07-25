use crate::frontend::parser::parse_rules::*;

mod enum_definition;
mod function;
mod generics;
mod implementation;
mod module_item;
mod struct_definition;

impl<'a, 'p> ItemParser<'a, 'p> {
    fn parse_implementation_item(&mut self) -> Result<Item, ParseError> {
        Ok(Item::Implementation(self.parse_implementation()?))
    }

    fn parse_enum_definition_item(&mut self) -> Result<Item, ParseError> {
        Ok(Item::EnumDefinition(self.parse_enum_definition()?))
    }

    fn parse_struct_definition_item(&mut self) -> Result<Item, ParseError> {
        Ok(Item::StructDefinition(self.parse_struct_definition()?))
    }

    fn parse_function_definition_item(
        &mut self,
        allow_self_param: bool,
    ) -> Result<Item, ParseError> {
        let prototype = self.parse_function_prototype(allow_self_param)?;
        Ok(Item::FunctionDefinition(
            self.parse_function_definition(prototype)?,
        ))
    }

    pub fn parse_item(
        &mut self,
        module_name: String,
        module_path: &std::path::Path,
    ) -> Result<Item, ParseError> {
        match self.peek_token()? {
            Token::Fn_ => self.parse_function_declaration_or_definition(),
            Token::Struct => self.parse_struct_definition_item(),
            Token::Enum => self.parse_enum_definition_item(),
            Token::Impl => self.parse_implementation_item(),
            // TODO: break out to separate routine
            Token::Mod => {
                let current_parsing_module_path = module_path.join(module_name.clone());
                match self.lookahead_token(2)? {
                    Token::LCurly => {
                        let parse_rules::module::ModuleResult {
                            module_tree,
                            module_worklist: child_worklist,
                        } = self.parse_module_item(&current_parsing_module_path)?;
                        Ok(Item::Module((Some(module_tree), child_worklist)))
                    }
                    Token::Semicolon => {
                        self.expect_token(Token::Mod)?;
                        let module_name = self.parse_identifier()?;

                        let worklist_string: String = current_parsing_module_path
                            .clone()
                            .join(module_name)
                            .to_str()
                            .unwrap()
                            .into();
                        trace::debug!("Add module worklist string: \"{}\"", worklist_string);

                        let child_worklist: BTreeSet<String> =
                            std::iter::once(worklist_string).collect();
                        self.expect_token(Token::Semicolon)?;
                        Ok(Item::Module((None, child_worklist)))
                    }
                    _ => self.unexpected_token(&[Token::LCurly, Token::Mod])?,
                }
            }
            _ => self.unexpected_token(&[Token::Fn_, Token::Struct, Token::Impl])?,
        }
    }
}
