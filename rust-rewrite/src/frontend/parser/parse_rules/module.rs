use crate::frontend::parser::parse_rules::*;
#[derive(Debug)]
pub(crate) struct ModuleResult {
    pub(crate) module_tree: ModuleTree,
    pub(crate) module_worklist: BTreeSet<String>,
}

impl<'a, 'p> ModuleParser<'a, 'p> {
    pub(crate) fn parse_module_contents(
        &mut self,
        mod_keyword_loc: sourceloc::SourceSpan,
        module_path: &std::path::Path,
        name: IdentifierTree,
        crate_name: &Option<String>,
    ) -> Result<ModuleResult, ParseError> {
        let (_start_loc, _span) = self.start_parsing("module contents")?;

        self.module_parse_stack.push(name.clone());

        let mut module_worklist = BTreeSet::<String>::new();
        let mut items = Vec::<ItemTree>::new();
        loop {
            match self.peek_token()? {
                Token::RCurly | Token::Eof => break,
                _ => {
                    let parsed_item =
                        self.item_parser()
                            .parse_item(name.clone(), module_path, crate_name)?;
                    if let ItemTree::Module((_, child_worklist)) = &parsed_item {
                        module_worklist.append(&mut child_worklist.clone());
                    }
                    items.push(parsed_item);
                }
            }
        }

        assert_eq!(self.module_parse_stack.pop().unwrap(), name);

        let module_path_vec: Vec<String> = match crate_name {
            Some(name) => vec![name.clone()],
            None => vec![],
        }
        .into_iter()
        .chain(
            module_path
                .iter()
                .map(|path_component| path_component.to_str().unwrap().into())
                .chain(std::iter::once(name.value.clone())),
        )
        .collect();

        let module_tree = ModuleTree {
            module_path: module_path_vec,
            mod_keyword_loc,
            name,
            items,
        };

        let module_result = ModuleResult {
            module_tree,
            module_worklist,
        };

        self.finish_parsing(module_result)
    }
}
