use std::{
    collections::{HashMap, HashSet, VecDeque},
    fs,
    ops::Range,
    path,
};

use annotate_snippets::{
    Annotation, AnnotationKind, Level, Renderer, Snippet, renderer::DecorStyle,
};
use syn::{spanned::Spanned, visit::Visit};

mod use_path {
    use proc_macro2::Span;

    #[derive(Default)]
    pub(crate) struct UsePath {
        modules: Vec<String>,
        spans: Vec<Span>,
    }

    impl UsePath {
        pub(crate) fn push(&mut self, module: String, span: Span) {
            self.modules.push(module);
            self.spans.push(span);
        }

        pub(crate) fn pop(&mut self, expect_module: &str) {
            assert_eq!(self.modules.pop().unwrap(), expect_module);
            self.spans.pop().unwrap();
        }

        pub(crate) fn modules(&self) -> &Vec<String> {
            &self.modules
        }

        pub(crate) fn spans(&self) -> &Vec<Span> {
            &self.spans
        }

        pub(crate) fn is_empty(&self) -> bool {
            self.modules.is_empty()
        }

        pub(crate) fn module_path_string(&self) -> String {
            self.modules.join("::")
        }
    }
}

use use_path::UsePath;

struct ImportVisitor<'a> {
    file_path: path::PathBuf,
    file_contents: String,
    mod_path: Vec<String>,
    declared_submodules: HashMap<Vec<String>, HashSet<String>>,
    use_path: UsePath,
    use_is_pub: bool,
    mod_worklist: Vec<(Vec<String>, String)>,
    has_errors: bool,
    imports_checked: u32,
    imports_passed: u32,
    ignore_crates: &'a HashSet<String>,
    only_crates: &'a HashSet<String>,
}

impl<'a> ImportVisitor<'a> {
    fn new(
        file_path: path::PathBuf,
        file_contents: String,
        mod_path: Vec<String>,
        ignore_crates: &'a HashSet<String>,
        only_crates: &'a HashSet<String>,
    ) -> Self {
        Self {
            file_path,
            file_contents,
            mod_path,
            declared_submodules: HashMap::new(),
            use_path: UsePath::default(),
            use_is_pub: false,
            mod_worklist: Vec::new(),
            has_errors: false,
            imports_checked: 0,
            imports_passed: 0,
            ignore_crates,
            only_crates,
        }
    }

    fn add_module_to_worklist(&mut self, name: String) {
        self.mod_worklist.push((self.mod_path.clone(), name));
    }
}

fn make_snippet<'a>(
    path: &'a path::Path,
    file_contents: &'a str,
    start_line: usize,
    substring_byte_range: Range<usize>,
    maybe_label: Option<&'a str>,
) -> Snippet<'a, Annotation<'a>> {
    let annotation = AnnotationKind::Primary
        .span(substring_byte_range.start..substring_byte_range.end)
        .highlight_source(true);

    let annotation = match maybe_label {
        Some(label) => annotation.label(label),
        None => annotation,
    };

    Snippet::source(file_contents)
        .path(path.to_string_lossy())
        .line_start(start_line)
        .annotation(annotation)
}

fn report_import_substring(
    path: &path::Path,
    file_contents: &str,
    start_line: usize,
    substring_byte_range: Range<usize>,
    import_path: &str,
) {
    let title = "import path contains substrings of itself";

    let label = &format!("imported name is substring of import path '{import_path}'");

    let report = &[Level::WARNING
        .with_name(Some("warning"))
        .primary_title(title)
        .element(make_snippet(
            path,
            file_contents,
            start_line,
            substring_byte_range,
            Some(label),
        ))];

    let renderer = Renderer::styled().decor_style(DecorStyle::Unicode);
    anstream::eprintln!("{}\n", renderer.render(report));
}

fn report_relative_import(
    path: &path::Path,
    file_contents: &str,
    start_line: usize,
    substring_byte_range: Range<usize>,
) {
    let title = "relative import";

    let report = &[Level::WARNING
        .with_name(Some("warning"))
        .primary_title(title)
        .element(make_snippet(
            path,
            file_contents,
            start_line,
            substring_byte_range,
            None,
        ))];

    let renderer = Renderer::styled().decor_style(DecorStyle::Unicode);
    anstream::eprintln!("{}\n", renderer.render(report));
}

fn report_public_reexport(
    path: &path::Path,
    file_contents: &str,
    start_line: usize,
    substring_byte_range: Range<usize>,
) {
    let title = "non-owned re-export";

    let report = &[Level::WARNING
        .with_name(Some("warning"))
        .primary_title(title)
        .element(make_snippet(
            path,
            file_contents,
            start_line,
            substring_byte_range,
            None,
        ))];

    let renderer = Renderer::styled().decor_style(DecorStyle::Unicode);
    anstream::eprintln!("{}\n", renderer.render(report));
}

impl ImportVisitor<'_> {
    fn can_visit(&self, name: &str) -> bool {
        if self.use_path.is_empty() {
            !self.ignore_crates.contains(name)
                && (self.only_crates.is_empty() || self.only_crates.contains(name))
        } else {
            true
        }
    }
}

impl<'a> syn::visit::Visit<'a> for ImportVisitor<'_> {
    fn visit_use_tree(&mut self, i: &'a syn::UseTree) {
        match i {
            syn::UseTree::Path(p) => {
                if self.can_visit(&p.ident.to_string()) {
                    self.visit_use_path(p);
                }
            }
            syn::UseTree::Name(n) => {
                if self.can_visit(&n.ident.to_string()) {
                    self.visit_use_name(n);
                }
            }
            syn::UseTree::Rename(r) => {
                if self.can_visit(&r.rename.to_string()) {
                    self.visit_use_rename(r);
                }
            }
            syn::UseTree::Glob(gl) => self.visit_use_glob(gl),
            syn::UseTree::Group(gr) => self.visit_use_group(gr),
        }
    }

    fn visit_use_path(&mut self, i: &'a syn::UsePath) {
        let ident = i.ident.to_string();

        self.use_path.push(ident.clone(), ident.span());
        self.visit_use_tree(&i.tree);
        self.use_path.pop(&ident);
    }

    fn visit_use_name(&mut self, i: &'a syn::UseName) {
        let mut pass = true;

        let name = i.ident.to_string().to_lowercase();

        // if we are 'use'-ing something that is a substring of its parent path
        // such as use syn::spanned::Spanned like at the top of this file!
        if self.use_path.modules().contains(&name) {
            let span = i.span();

            report_import_substring(
                &self.file_path,
                &self.file_contents,
                self.use_path.spans()[0].start().line,
                span.byte_range(),
                &self.use_path.module_path_string(),
            );
            pass = false;
        }

        // TODO: the declared_submodules need to be populated by an initial pass over the tree
        if self.use_is_pub // public re-export
            // this 'use' starts with a module at a higher scope than our current scope
            && !self.declared_submodules.entry(self.mod_path.clone()).or_default().contains(&self.use_path.modules()[0])
        {
            let span = i.span();
            report_public_reexport(
                &self.file_path,
                &self.file_contents,
                self.use_path.spans()[0].start().line,
                span.byte_range(),
            );
            pass = false;
        }

        self.imports_checked += 1;
        if pass {
            self.imports_passed += 1;
        }
    }

    fn visit_use_rename(&mut self, i: &'a syn::UseRename) {
        syn::visit::visit_use_rename(self, i);
    }
    fn visit_use_glob(&mut self, i: &'a syn::UseGlob) {
        syn::visit::visit_use_glob(self, i);
    }
    fn visit_use_group(&mut self, i: &'a syn::UseGroup) {
        syn::visit::visit_use_group(self, i);
    }

    fn visit_item_use(&mut self, node: &'a syn::ItemUse) {
        let mut pass = true;
        match &node.vis {
            syn::Visibility::Public(_) | syn::Visibility::Restricted(_) => {
                self.use_is_pub = true;
            }
            syn::Visibility::Inherited => {
                self.use_is_pub = false;
            }
        }

        self.imports_checked += 1;
        let start = node.span().start();

        match &node.tree {
            syn::UseTree::Path(syn::UsePath { ident, .. }) => {
                let root_segment = ident.to_string();

                if root_segment == "super" || root_segment == "self" {
                    report_relative_import(
                        &self.file_path,
                        &self.file_contents,
                        ident.span().start().line,
                        ident.span().byte_range(),
                    );
                    pass = false;
                }
            }
            syn::UseTree::Glob(_glob) => {
                eprintln!(
                    "Error: glob import found\n --> {}:{}:{};\n\t{}",
                    self.file_path.display(),
                    start.line,
                    start.column,
                    node.span().source_text().unwrap()
                );
                pass = false;
            }
            _ => (),
        }

        self.visit_use_tree(&node.tree);

        if pass {
            self.imports_passed += 1;
        } else {
            self.has_errors = true;
        }
    }

    fn visit_item_mod(&mut self, i: &'a syn::ItemMod) {
        self.declared_submodules
            .entry(self.mod_path.clone())
            .or_default()
            .insert(i.ident.to_string());

        match &i.content {
            None => self.add_module_to_worklist(i.ident.to_string()),
            Some((_, content)) => {
                self.mod_path.push(i.ident.to_string());
                for item in content {
                    self.visit_item(item);
                }
                self.mod_path.pop();
            }
        }
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
    imports_checked: &mut u32,
    imports_passed: &mut u32,
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

pub(crate) fn lint_from_root(
    root_path: &path::Path,
    ignore_crates: &HashSet<String>,
    only_crates: &HashSet<String>,
) {
    let root_dir = root_path.parent().unwrap();
    let mut root_errors = false;

    let mut mod_worklist: VecDeque<(Vec<String>, String)> = VecDeque::new();
    let mut imports_checked = 0u32;
    let mut imports_passed = 0u32;

    let content = fs::read_to_string(root_path).unwrap();
    let visitor = ImportVisitor::new(
        root_path.into(),
        content,
        vec![],
        ignore_crates,
        only_crates,
    );
    eprintln!("{}", root_path.display());
    root_errors |= try_visit_file_content(
        &mut mod_worklist,
        &(visitor.file_contents.clone()),
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
            let visitor_mod_root_path = mod_root_path
                .clone()
                .into_iter()
                .chain(std::iter::once(mod_name))
                .collect();
            let visitor = ImportVisitor::new(
                path,
                content,
                visitor_mod_root_path,
                ignore_crates,
                only_crates,
            );

            root_errors |= try_visit_file_content(
                &mut mod_worklist,
                &(visitor.file_contents.clone()),
                visitor,
                &mut imports_checked,
                &mut imports_passed,
            );
        } else {
            panic!("unable to find module {}: {}", mod_name, mod_path.display());
        }
    }

    let pass_percent = f64::from(imports_passed) / f64::from(imports_checked);

    let pass_percent_string = if pass_percent.is_nan() {
        "100%"
    } else {
        &format!("{:.0}%", pass_percent * 100.0)
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
