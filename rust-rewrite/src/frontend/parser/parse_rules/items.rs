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
    ) -> Result<(ItemTree, Option<BTreeSet<WorklistItem>>), ParseError> {
        match self.peek_token()? {
            Token::Fn_ => Ok((self.parse_function_declaration_or_definition()?, None)),
            Token::Struct => Ok((self.parse_struct_definition_item()?, None)),
            Token::Enum => Ok((self.parse_enum_definition_item()?, None)),
            Token::Impl => Ok((self.parse_implementation_item()?, None)),
            // TODO: break out to separate routine
            Token::Mod => {
                let current_parsing_module_path = module_path.join(module_name.value);
                match self.lookahead_token(2)? {
                    Token::LCurly => {
                        let parse_rules::module::ModuleResult {
                            module_tree,
                            module_worklist: child_worklist,
                        } = self.parse_module_item(&current_parsing_module_path, crate_name)?;
                        Ok((ItemTree::Module(Ok(module_tree)), Some(child_worklist)))
                    }
                    Token::Semicolon => {
                        let mod_loc = self.expect_token(Token::Mod)?;
                        let module_name = self.parse_identifier()?;

                        let worklist_string: String = current_parsing_module_path
                            .clone()
                            .join(module_name.value.clone())
                            .to_str()
                            .unwrap()
                            .into();
                        trace::debug!("Add module worklist string: \"{}\"", worklist_string);

                        let child_worklist: BTreeSet<WorklistItem> =
                            std::iter::once(WorklistItem::new(
                                module_name.value.clone(),
                                current_parsing_module_path
                                    .iter()
                                    .map(|os_str| String::from(os_str.to_str().unwrap()))
                                    .collect(),
                            ))
                            .collect();
                        self.expect_token(Token::Semicolon)?;
                        Ok((
                            ItemTree::Module(Err((mod_loc, module_name.value))),
                            Some(child_worklist),
                        ))
                    }
                    _ => self.unexpected_token(&[Token::LCurly, Token::Mod])?,
                }
            }
            _ => self.unexpected_token(&[Token::Fn_, Token::Struct, Token::Impl])?,
        }
    }
}
