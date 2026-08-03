use std::{
    collections::{BTreeSet, VecDeque},
    fmt::Display,
    fs::File,
    path::{Path, PathBuf},
};

use crate::{
    frontend::{
        ast::*,
        lexer::{token::Token, LexError},
        sourceloc::*,
        *,
    },
    trace,
};

mod errors;
mod parse_rules;

pub(crate) use errors::ParseError;
pub(in crate::frontend) use parse_rules::module::ModuleResult;

pub(crate) struct Parser<'a> {
    lexer: Lexer<'a>,
    module_parse_stack: Vec<IdentifierTree>,
    last_match: SourceSpan,
    upcoming_tokens: VecDeque<(Token, SourceSpan)>,
    parsing_stack: Vec<(SourceLoc, String)>,
}

impl<'a> Parser<'a> {
    pub(crate) fn new(
        _module_name: String,
        module_path: &std::path::Path,
        lexer: Lexer<'a>,
    ) -> Self {
        let lexer_start_pos = lexer.current_loc();
        let mut module_hierarchy = Vec::new();
        for component in module_path.iter() {
            module_hierarchy.push(IdentifierTree {
                loc: lexer_start_pos.clone().into(),
                value: component.to_str().unwrap().into(),
            })
        }

        trace::debug!("Module hierarchy: {:?}", module_hierarchy);

        Parser {
            lexer,
            module_parse_stack: module_hierarchy,
            last_match: lexer_start_pos.into(),
            upcoming_tokens: VecDeque::new(),
            parsing_stack: Vec::new(),
        }
    }

    fn ensure_n_tokens_in_lookahead(&mut self, n: usize) -> Result<(), LexError> {
        while self.upcoming_tokens.len() <= n && self.lexer.peek()?.0 != Token::Eof {
            self.upcoming_tokens.push_back(self.lexer.next()?);
        }

        Ok(())
    }

    // return the next token from the input stream without advancing
    // utilizes lookahead_token
    fn peek_token(&mut self) -> Result<Token, LexError> {
        match self.peek_token_with_loc() {
            Ok((token, _)) => Ok(token),
            Err(e) => Err(e),
        }
    }

    fn peek_token_with_loc(&mut self) -> Result<(Token, sourceloc::SourceSpan), LexError> {
        let peeked = self.lookahead_token_with_loc(0)?;
        // #[cfg(feature = "loud_parsing")]
        // println!("Parser::peek_token() -> {}", peeked);
        trace::trace!("Peek token: {} @ {}", peeked.0, peeked.1);
        Ok(peeked)
    }

    fn lookahead_token(&mut self, lookahead_by: usize) -> Result<Token, LexError> {
        match self.lookahead_token_with_loc(lookahead_by) {
            Ok((token, _)) => Ok(token),
            Err(e) => Err(e),
        }
    }

    // returns the lookahead_by-th token from the input stream withing advancing, or EOF if that many tokens are not available
    fn lookahead_token_with_loc(
        &mut self,
        lookahead_by: usize,
    ) -> Result<(Token, SourceSpan), LexError> {
        self.ensure_n_tokens_in_lookahead(lookahead_by)?;

        Ok(self
            .upcoming_tokens
            .get(lookahead_by)
            .cloned()
            .unwrap_or((Token::Eof, self.lexer.current_loc().into())))
    }

    fn next_token(&mut self) -> Result<Token, ParseError> {
        self.ensure_n_tokens_in_lookahead(1)?;
        let (next, start_loc) = self
            .upcoming_tokens
            .pop_front()
            .unwrap_or((Token::Eof, self.lexer.current_loc().into()));
        self.last_match = start_loc;
        #[cfg(feature = "loud_parsing")]
        self.annotate_parsing(&format!("Parser::next_token() -> {}@{}", next, start_loc));
        Ok(next)
    }

    fn next_token_with_loc(&mut self) -> Result<(Token, SourceSpan), ParseError> {
        self.ensure_n_tokens_in_lookahead(1)?;
        let next = self
            .upcoming_tokens
            .pop_front()
            .unwrap_or((Token::Eof, self.lexer.current_loc().into()));
        self.last_match = next.1.clone();
        #[cfg(feature = "loud_parsing")]
        self.annotate_parsing(&format!("Parser::next_token() -> {}@{}", next, start_loc));
        Ok(next)
    }

    #[track_caller]
    fn expect_token(&mut self, expected: Token) -> Result<SourceSpan, ParseError> {
        //#[cfg(feature = "loud_parsing")]
        //self.annotate_parsing(&format!("Parser::expect_token({})", _expected));
        let (current_parse_start_loc, current_parse_string) = self
            .parsing_stack
            .last()
            .unwrap_or(&(SourceLoc::none(), String::from("UNKNOWN")))
            .to_owned();

        let (upcoming_token, upcoming_loc) = self.peek_token_with_loc()?;
        if upcoming_token.eq(&expected) {
            Ok(self.next_token_with_loc()?.1)
        } else {
            Err(ParseError::unexpected_token(
                upcoming_loc.start(),
                upcoming_token,
                &[expected],
                current_parse_string,
                current_parse_start_loc,
                SourceLoc::from(std::panic::Location::caller()),
            ))
        }
    }

    #[track_caller]
    fn unexpected_token<T>(&mut self, expected_tokens: &[Token]) -> Result<T, ParseError> {
        let (current_parse_start_loc, current_parse_string) = self
            .parsing_stack
            .last()
            .unwrap_or(&(SourceLoc::none(), String::from("UNKNOWN")))
            .to_owned();

        let (upcoming_token, upcoming_loc) = match self.peek_token_with_loc() {
            Ok(tok) => tok,
            Err(error) => return Err(ParseError::from(error)),
        };

        Err(ParseError::unexpected_token(
            upcoming_loc.start(),
            upcoming_token,
            expected_tokens,
            current_parse_string,
            current_parse_start_loc,
            SourceLoc::from(std::panic::Location::caller()),
        ))
    }

    fn start_parsing(
        &mut self,
        what_parsing: &str,
    ) -> Result<(SourceLoc, trace::ExitOnDropSpan), ParseError> {
        let start_loc = self.peek_token_with_loc()?.1.start();

        let exit_on_drop_span = trace::span_auto!(
            tracing::Level::TRACE,
            "parse rule start",
            what_parsing,
            "{}",
            start_loc
        );

        self.parsing_stack
            .push((start_loc.clone(), String::from(what_parsing)));

        tracing::trace!("{}", start_loc);

        Ok((start_loc, exit_on_drop_span))
    }

    // FUTURE: is putting everything in a box really the right choice?
    fn finish_parsing<T>(&mut self, parsed: T) -> Result<T, ParseError>
    where
        T: std::fmt::Debug,
    {
        let (_parse_start, parsed_description) = self
            .parsing_stack
            .pop()
            .expect("Mismatched loud parsing tracking");
        tracing::event!(
            tracing::Level::DEBUG,
            "Finish parsing {}: {:?}",
            parsed_description,
            parsed
        );

        #[cfg(feature = "loud_parsing")]
        {
            let annotation_string = format!(
                "Done parsing {} ({}-{}): {}",
                _parsed_description,
                _parse_start,
                self.peek_token_with_loc()?.1,
                _parsed
            );

            self.annotate_parsing(&annotation_string);
        }

        Ok(parsed)
    }

    #[cfg(feature = "loud_parsing")]
    fn annotate_parsing(&self, output: &str) {
        for _ in 0..self.parsing_stack.len() {
            print!("\t");
        }

        println!("{}", output);
    }
}

impl<'a> Parser<'a> {
    pub(in crate::frontend) fn parse(
        &mut self,
        mod_keyword_loc: sourceloc::SourceSpan,
        parent_module_path: &std::path::Path,
        module_name: String,
        crate_name: &str,
    ) -> Result<parse_rules::module::ModuleResult, ParseError> {
        let module_name_tree = ast::IdentifierTree {
            loc: mod_keyword_loc.clone(),
            value: module_name,
        };

        self.module_parser().parse_module_contents(
            mod_keyword_loc,
            parent_module_path,
            module_name_tree,
            &Some(String::from(crate_name)),
        )
    }
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, serde::Serialize, serde::Deserialize)]
pub(in crate::frontend) struct WorklistItem {
    module_name: String,
    parent_modules: Vec<String>,
}

impl WorklistItem {
    fn new(module_name: String, parent_modules: Vec<String>) -> Self {
        Self {
            module_name,
            parent_modules,
        }
    }
}

impl Display for WorklistItem {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}",
            self.parent_modules
                .iter()
                .chain(std::iter::once(&self.module_name))
                .cloned()
                .collect::<Vec<String>>()
                .join(", ")
        )
    }
}

fn file_path_to_module_name<'a>(
    filepath_to_parse: &'a std::path::Path,
    crate_path: &std::path::PathBuf,
) -> (String, &'a std::path::Path) {
    let stem: String = filepath_to_parse
        .file_stem()
        .unwrap()
        .to_str()
        .unwrap()
        .into();
    let module_name: String = match stem.as_str() {
        "mod" | "lib" => filepath_to_parse.parent().unwrap().to_str().unwrap().into(),
        _ => stem,
    };
    (
        module_name,
        filepath_to_parse
            .strip_prefix(crate_path)
            .unwrap()
            .parent()
            .unwrap_or(std::path::Path::new("")),
    )
}

fn lex_and_parse_file(
    crate_name: &str,
    name: String,
    path: &Path,
    file: File,
) -> Result<ModuleResult, ParseError> {
    let lexer = Lexer::from_file(path, file);

    let lexer_start_loc = lexer.current_loc();

    let mut parser = Parser::new(name.clone(), path, lexer);

    parser.parse(lexer_start_loc.into(), path, name, crate_name)
}

#[derive(Debug)]
enum FindParseModuleError {
    ModuleNotFound(String),
    ParseError(ParseError),
}

impl Display for FindParseModuleError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::ModuleNotFound(name) => write!(f, "unable to find module {}", name),
            Self::ParseError(e) => write!(f, "parse error: {}", e),
        }
    }
}

impl From<ParseError> for FindParseModuleError {
    fn from(value: ParseError) -> Self {
        Self::ParseError(value)
    }
}

fn find_and_parse_module(
    item: WorklistItem,
    crate_name: &str,
    crate_path: &Path,
    allow_subdir: bool,
) -> Result<ModuleResult, FindParseModuleError> {
    let parent_module_path: PathBuf = item.parent_modules.iter().collect();
    let full_parent_path = crate_path.join(parent_module_path.clone());

    let direct_mod_path = full_parent_path
        .clone()
        .join(item.module_name.clone())
        .with_extension("sb");
    println!("trying direct mod path: {:?}", direct_mod_path);
    if let Ok(infile) = File::open(direct_mod_path) {
        return Ok(lex_and_parse_file(
            crate_name,
            item.module_name,
            &parent_module_path,
            infile,
        )?);
    }

    if allow_subdir {
        let subdir_path: PathBuf = [item.module_name.clone(), String::from("mod.sb")]
            .iter()
            .collect();

        let subdir_mod_path = full_parent_path.clone().join(subdir_path);

        if let Ok(infile) = File::open(subdir_mod_path) {
            return Ok(lex_and_parse_file(
                crate_name,
                item.module_name,
                &parent_module_path,
                infile,
            )?);
        }
    }

    Err(FindParseModuleError::ModuleNotFound(item.module_name))
}

pub(crate) fn parse_crate(
    crate_name: &str,
    bin_name: &str,
    crate_path: PathBuf,
) -> Result<BTreeSet<ast::ModuleTree>, ParseError> {
    let mut modules = BTreeSet::new();
    let mut worklist = BTreeSet::<WorklistItem>::new();

    let ModuleResult {
        module_tree,
        module_worklist,
    } = find_and_parse_module(
        WorklistItem::new(String::from(bin_name), vec![]),
        crate_name,
        &crate_path,
        false,
    )
    .expect("error parsing crate root ");
    modules.insert(module_tree);
    worklist.extend(module_worklist);

    while let Some(worklist_item) = worklist.pop_last() {
        let module_name = worklist_item.module_name.clone();
        let ModuleResult {
            module_tree,
            mut module_worklist,
        } = find_and_parse_module(worklist_item, crate_name, &crate_path, true)
            .unwrap_or_else(|e| panic!("Error in file {}: {}", module_name, e));

        worklist.append(&mut module_worklist);

        assert!(modules.insert(module_tree));
    }

    Ok(modules)
}

#[cfg(test)]
mod tests {
    use crate::frontend::parser::*;

    pub(crate) fn test_parser<'a>(input: &'a str) -> Parser<'a> {
        let module_path = std::path::Path::new("");
        Parser::new(
            "".into(),
            module_path,
            lexer::Lexer::<'a>::from_string(&input),
        )
    }
}
