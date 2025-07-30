use crate::frontend::parser::parse_rules::*;

pub struct ModuleResult {
    pub module_tree: ModuleTree,
    pub module_worklist: BTreeSet<String>,
}

impl<'a, 'p> ModuleParser<'a, 'p> {
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
                Token::RCurly | Token::Eof => break,
                _ => {
                    let parsed_item = self
                        .item_parser()
                        .parse_item(module_name.clone(), module_path)?;
                    match &parsed_item {
                        Item::Module((_, child_worklist)) => {
                            module_worklist.append(&mut child_worklist.clone());
                        }
                        _ => (),
                    }
                    items.push(parsed_item);
                }
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
