use std::{collections::VecDeque, fs, path};

use syn::{spanned::Spanned, visit::Visit};

struct ImportVisitor {
    file_path: path::PathBuf,
    mod_path: Vec<String>,
    mod_worklist: Vec<(Vec<String>, String)>,
    has_errors: bool,
    imports_checked: usize,
    imports_passed: usize,
}

impl ImportVisitor {
    fn add_module_to_worklist(&mut self, name: String) {
        self.mod_worklist.push((self.mod_path.clone(), name));
    }
}

impl<'a> syn::visit::Visit<'a> for ImportVisitor {
    fn visit_use_tree(&mut self, i: &'a syn::UseTree) {
                match i {
        syn::UseTree::Path(p) => self.visit_use_path(p),
        syn::UseTree::Name(n) => (),
        syn::UseTree::Rename(r) => (),
        syn::UseTree::Glob(gl) => self.visit_use_glob(gl),
        syn::UseTree::Group(gr) => (),
        }


        syn::visit::visit_use_tree(self, i);    
    }

    fn visit_item_use(&mut self, node: &'a syn::ItemUse) {
        let mut pass = true;
        self.imports_checked += 1;
        let start = node.span().start();
        eprintln!("{}:{}:{}", self.file_path.display(), start.line, start.column);
        match &node.tree {
            syn::UseTree::Path(syn::UsePath { ident, tree, ..}) => {
                eprintln!("\t you are here");
                let root_segment = ident.to_string();

                if root_segment == "super" || root_segment == "self" {
                    eprintln!(
                        "Error: relative import found\n --> {}:{}:{};\n\t{}",
                        self.file_path.display(),
                        start.line,
                        start.column,
                        node.span().source_text().unwrap()
                    );
                    pass = false;
                }

                self.visit_use_tree(tree.as_ref());
            }
            syn::UseTree::Glob(_glob) => {
                eprintln!(
                    "Error: glob import found\n --> {}:{}:{};\n\t{}",
                    self.file_path.display(),
                    start.line,
                    start.column,
                    node.span().source_text().unwrap()
                );
                pass = false
            }
            _ => (),
        }

        if pass {
            self.imports_passed += 1;
        } else {
            self.has_errors = true;
        }

        syn::visit::visit_item_use(self, node);
    }

    fn visit_item_mod(&mut self, i: &'a syn::ItemMod) {
        if let None = i.content {
            self.add_module_to_worklist(i.ident.to_string());
        }

        self.mod_path.push(i.ident.to_string());
        syn::visit::visit_item_mod(self, i);

        self.mod_path.pop();
    }

    fn visit_macro(&mut self, i: &'a syn::Macro) {
        if i.path.segments.iter().next_back().unwrap().ident == "include" {
            eprintln!("import linter doesn't support include!() macros");
            std::process::exit(1);
        }
    }
}

fn try_visit_file_content(
    mod_worklist: &mut VecDeque<(Vec<String>, String)>,
    content: &str,
    mut visitor: ImportVisitor,
    imports_checked: &mut usize,
    imports_passed: &mut usize,
) -> bool {
    if let Ok(ast) = syn::parse_file(content) {
        visitor.visit_file(&ast);
        mod_worklist.extend(visitor.mod_worklist);
        *imports_checked += visitor.imports_checked;
        *imports_passed += visitor.imports_passed;
        visitor.has_errors
    } else {
        panic!();
    }
}

pub(crate) fn lint_from_root(root_path: &path::Path) {
    let root_dir = root_path.parent().unwrap();
    let mut root_errors = false;

    let mut mod_worklist: VecDeque<(Vec<String>, String)> = VecDeque::new();
    let mut imports_checked = 0;
    let mut imports_passed = 0;

    let visitor = ImportVisitor {
        file_path: root_path.into(),
        mod_path: vec![],
        mod_worklist: vec![],
        has_errors: false,
        imports_checked: 0,
        imports_passed: 0,
    };
    let content = fs::read_to_string(root_path).unwrap();
            eprintln!("{}", root_path.display());
    root_errors |= try_visit_file_content(
        &mut mod_worklist,
        &content,
        visitor,
        &mut imports_checked,
        &mut imports_passed,
    );

    while let Some((mod_root_path, mod_name)) = mod_worklist.pop_front() {
        let mod_path = root_dir.join(path::PathBuf::from(mod_root_path.join("/")));

        let file_mod_path = mod_path.clone().join(format!("{mod_name}.rs"));

        if let Ok((path, content)) = fs::read_to_string(file_mod_path.clone())
            .map(|r| (file_mod_path, r))
            .or_else(|_| {
                let dir_mod_path = mod_path.join(
                    vec![mod_name.clone(), String::from("mod.rs")]
                        .into_iter()
                        .collect::<path::PathBuf>(),
                );
                fs::read_to_string(dir_mod_path.clone()).map(|r| (dir_mod_path, r))
            })
        {
            eprintln!("{}", path.display());
            let visitor_mod_root_path = mod_root_path
                .clone()
                .into_iter()
                .chain(std::iter::once(mod_name))
                .collect();
            let visitor = ImportVisitor {
                file_path: path,
                mod_path: visitor_mod_root_path,
                mod_worklist: vec![],
                has_errors: false,
                imports_checked: 0,
                imports_passed: 0,
            };



            root_errors |= try_visit_file_content(
                &mut mod_worklist,
                &content,
                visitor,
                &mut imports_checked,
                &mut imports_passed,
            );
        } else {
            eprintln!("unable to find module {}: {}", mod_name, mod_path.display());
        }
    }

    let pass_percent = (imports_passed as f64) / (imports_checked as f64);

    let pass_percent_string = if pass_percent.is_nan() {
        "100%"
    } else {
        &format!("{:3}%", pass_percent * 100.0)
    };

    eprintln!(
        "{:4}/{:4} ({}) imports passed in {}",
        imports_passed,
        imports_checked,
        pass_percent_string,
        root_path.display()
    );

    if root_errors {
        std::process::exit(1);
    }
}
