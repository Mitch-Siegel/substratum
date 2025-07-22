use crate::frontend::parser::*;

mod expressions;
mod items;
mod statements;

pub struct ModuleResult {
    pub module_tree: ModuleTree,
    pub module_worklist: BTreeSet<String>,
}

impl<'a> Parser<'a> {
    pub fn parse_module_contents(
        &mut self,
        module_path: &std::path::Path,
        module_name: String,
    ) -> Result<ModuleResult, ParseError> {
        self.start_parsing("module contents")?;

        self.module_parse_stack.push(module_name.clone());

        let mut module_worklist = BTreeSet::<String>::new();
        let mut items = Vec::<Item>::new();
        loop {
            match self.peek_token()? {
                Token::Fun => {
                    let function_definition_item =
                        self.parse_function_declaration_or_definition()?;
                    items.push(function_definition_item);
                }
                Token::Struct => {
                    let struct_def_item = items::parse_struct_definition_item(self)?;
                    items.push(struct_def_item);
                }
                Token::Enum => {
                    let enum_def_item = items::parse_enum_definition_item(self)?;
                    items.push(enum_def_item)
                }
                Token::Impl => {
                    let impl_item = items::parse_implementation_item(self)?;
                    items.push(impl_item);
                }
                Token::RCurly => break,
                // TODO: break out to separate routine
                Token::Mod => {
                    let current_parsing_module_path = module_path.join(module_name.clone());
                    match self.lookahead_token(2)? {
                        Token::LCurly => {
                            let ModuleResult {
                                module_tree,
                                module_worklist: mut child_worklist,
                            } = self.parse_module_item(&current_parsing_module_path)?;
                            module_worklist.append(&mut child_worklist);
                            items.push(Item::Module(module_tree));
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
                            module_worklist.insert(worklist_string);
                            self.expect_token(Token::Semicolon)?;
                        }
                        _ => self.unexpected_token(&[Token::LCurly, Token::Mod])?,
                    }
                }
                Token::Eof => break,
                _ => self.unexpected_token(&[Token::Fun, Token::Struct, Token::Impl])?,
            }
        }

        assert_eq!(self.module_parse_stack.pop().unwrap(), module_name);

        let module_path_vec: Vec<String> = module_path
            .iter()
            .map(|path_component| path_component.to_str().unwrap().into())
            .chain(std::iter::once(module_name.as_str().into()))
            .map(|module| module)
            .collect();

        let module_tree = ModuleTree {
            module_path: module_path_vec,
            name: module_name,
            items,
        };
        self.finish_parsing(&module_tree)?;
        Ok(ModuleResult {
            module_tree,
            module_worklist,
        })
    }
}
