// #[cfg(test)]
// mod tests_old;

use std::collections::{BTreeSet, VecDeque};

use crate::midend::ir;

use super::{
    ast::*,
    lexer::{token::Token, *},
    sourceloc::{SourceLoc, SourceLocWithMod},
};

use crate::trace;

mod declarations;
mod errors;
mod expressions;
mod single_token;
#[cfg(test)]
mod tests;
mod types;

pub use errors::ParseError;

pub struct Parser<'a> {
    lexer: Lexer<'a>,
    module_parse_stack: Vec<String>,
    last_match: SourceLoc,
    upcoming_tokens: VecDeque<(Token, SourceLoc)>,
    parsing_stack: Vec<(SourceLoc, String)>,
}

impl ir::BinaryOperations {
    pub fn get_precedence(&self) -> usize {
        match self {
            Self::Add(_) => 1,
            Self::Subtract(_) => 1,
            Self::Multiply(_) => 2,
            Self::Divide(_) => 2,
            Self::LThan(_) => 3,
            Self::GThan(_) => 3,
            Self::LThanE(_) => 3,
            Self::GThanE(_) => 3,
            Self::Equals(_) => 4,
            Self::NotEquals(_) => 4,
        }
    }

    pub fn precedence_of_token(token: &Token) -> usize {
        match token {
            Token::Plus => 1,
            Token::Minus => 1,
            Token::Star => 2,
            Token::FSlash => 2,
            Token::LThan => 3,
            Token::GThan => 3,
            Token::LThanE => 3,
            Token::GThanE => 3,
            Token::Equals => 4,
            Token::NotEquals => 4,
            _ => {
                panic!(
                    "Invalid token {} passed to BinaryOperations::precedence_of_token",
                    token
                );
            }
        }
    }
}

impl<'a> Parser<'a> {
    pub fn new(_module_name: String, module_path: &std::path::Path, lexer: Lexer<'a>) -> Self {
        let lexer_start_pos = lexer.current_loc();
        let mut module_hierarchy = Vec::new();
        for component in module_path.iter() {
            module_hierarchy.push(component.to_str().unwrap().into())
        }

        trace::debug!("Module hierarchy: {:?}", module_hierarchy);

        Parser {
            lexer: lexer,
            module_parse_stack: module_hierarchy,
            last_match: lexer_start_pos,
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

    fn peek_token_with_loc(&mut self) -> Result<(Token, SourceLoc), LexError> {
        let peeked = self.lookahead_token_with_loc(0)?;
        // #[cfg(feature = "loud_parsing")]
        // println!("Parser::peek_token() -> {}", peeked);
        trace::trace!("Peek token: {} @ {}", peeked.0, peeked.1);
        return Ok(peeked);
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
    ) -> Result<(Token, SourceLoc), LexError> {
        self.ensure_n_tokens_in_lookahead(lookahead_by)?;

        Ok(self
            .upcoming_tokens
            .get(lookahead_by)
            .cloned()
            .unwrap_or((Token::Eof, SourceLoc::none())))
    }

    fn next_token(&mut self) -> Result<Token, ParseError> {
        self.ensure_n_tokens_in_lookahead(1)?;
        let (next, start_loc) = self
            .upcoming_tokens
            .pop_front()
            .unwrap_or((Token::Eof, self.lexer.current_loc()));
        self.last_match = start_loc;
        #[cfg(feature = "loud_parsing")]
        self.annotate_parsing(&format!("Parser::next_token() -> {}@{}", next, start_loc));
        Ok(next)
    }

    fn expect_token(&mut self, _expected: Token) -> Result<Token, ParseError> {
        //#[cfg(feature = "loud_parsing")]
        //self.annotate_parsing(&format!("Parser::expect_token({})", _expected));
        let (current_parse_start_loc, current_parse_string) = self
            .parsing_stack
            .last()
            .unwrap_or(&(SourceLoc::none(), String::from("UNKNOWN")))
            .to_owned();

        let (upcoming_token, upcoming_loc) = self.peek_token_with_loc()?;
        if upcoming_token.eq(&_expected) {
            self.next_token()
        } else {
            Err(ParseError::unexpected_token(
                upcoming_loc,
                upcoming_token,
                &[_expected],
                current_parse_string,
                current_parse_start_loc,
            ))
        }
    }

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
            upcoming_loc,
            upcoming_token,
            expected_tokens,
            current_parse_string,
            current_parse_start_loc,
        ))
    }

    fn start_parsing(
        &mut self,
        what_parsing: &str,
    ) -> Result<(SourceLocWithMod, trace::ExitOnDropSpan), ParseError> {
        let start_loc = self.peek_token_with_loc()?.1;

        let exit_on_drop_span = trace::span_auto!(
            tracing::Level::DEBUG,
            "parse rule start",
            what_parsing,
            "{}",
            start_loc
        );

        self.parsing_stack
            .push((start_loc.clone(), String::from(what_parsing)));

        tracing::trace!("{}", start_loc);

        Ok((
            SourceLocWithMod::new(
                start_loc,
                self.module_parse_stack
                    .last()
                    .unwrap_or(&String::new())
                    .clone(),
            ),
            exit_on_drop_span,
        ))
    }

    fn finish_parsing<T>(&mut self, _parsed: &T) -> Result<(), ParseError>
    where
        T: std::fmt::Display,
    {
        tracing::event!(tracing::Level::TRACE, "{}", _parsed);
        let (_parse_start, _parsed_description) = self
            .parsing_stack
            .pop()
            .expect("Mismatched loud parsing tracking");

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

        Ok(())
    }

    #[cfg(feature = "loud_parsing")]
    fn annotate_parsing(&self, output: &str) {
        for _ in 0..self.parsing_stack.len() {
            print!("\t");
        }

        println!("{}", output);
    }
}

pub struct ModuleResult {
    pub module_tree: ModuleTree,
    pub module_worklist: BTreeSet<String>,
}

impl<'a> Parser<'a> {
    pub fn parse(
        &mut self,
        parent_module_path: &std::path::Path,
        module_name: String,
    ) -> Result<ModuleResult, ParseError> {
        self.parse_module_contents(parent_module_path, module_name)
    }

    fn parse_module_item(
        &mut self,
        parent_module_path: &std::path::Path,
    ) -> Result<ModuleResult, ParseError> {
        let (_, _) = self.start_parsing("module item")?;

        trace::debug!(
            "module item parent module path: \"{}\"",
            parent_module_path.display()
        );
        self.expect_token(Token::Mod)?;
        let name = self.parse_identifier()?;
        self.expect_token(Token::LCurly)?;
        let module_result = self.parse_module_contents(parent_module_path, name)?;
        self.expect_token(Token::RCurly)?;

        self.finish_parsing(&module_result.module_tree)?;
        Ok(module_result)
    }

    fn parse_module_contents(
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
                    let struct_definition = self.parse_struct_definition()?;
                    let struct_definition_item = Item::StructDefinition(struct_definition);
                    items.push(struct_definition_item);
                }
                Token::Impl => {
                    let implementation = self.parse_implementation()?;
                    let impl_item = Item::Implementation(implementation);
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

    fn parse_statement(&mut self) -> Result<StatementTree, ParseError> {
        let (start_loc, _) = self.start_parsing("statement")?;

        let statement = StatementTree {
            loc: start_loc,
            statement: match self.peek_token()? {
                Token::Identifier(_) => match self.lookahead_token(1)? {
                    Token::Colon => {
                        Statement::VariableDeclaration(self.parse_variable_declaration()?)
                    }
                    _ => Statement::Expression(self.parse_expression()?),
                },
                Token::If | Token::While | Token::LCurly => {
                    Statement::Expression(self.parse_expression()?)
                }
                _ => self.unexpected_token::<Statement>(&[
                    Token::Identifier("".into()),
                    Token::If,
                    Token::While,
                    Token::LCurly,
                ])?,
            },
        };

        if matches!(self.peek_token()?, Token::Semicolon) {
            self.expect_token(Token::Semicolon)?;
        }

        self.finish_parsing(&statement)?;

        Ok(statement)
    }

    fn token_is_operator_of_at_least_precedence(token: &Token, precedence: usize) -> bool {
        match token {
            Token::Plus
            | Token::Minus
            | Token::Star
            | Token::FSlash
            | Token::LThan
            | Token::GThan
            | Token::LThanE
            | Token::GThanE
            | Token::Equals
            | Token::NotEquals => ir::BinaryOperations::precedence_of_token(&token) >= precedence,
            _ => false,
        }
    }
}
