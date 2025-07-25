// #[cfg(test)]
// mod tests_old;

use std::collections::{BTreeSet, VecDeque};

use crate::{frontend::ast, midend::ir};

use super::{
    ast::*,
    lexer::{token::Token, *},
    sourceloc::{SourceLoc, SourceLocWithMod},
};

mod errors;
mod parse_rules;

pub use parse_rules::module::ModuleResult;

use crate::trace;
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

    fn current_module(&self) -> &str {
        self.module_parse_stack.last().unwrap().as_str()
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

    #[track_caller]
    fn expect_token(&mut self, expected: Token) -> Result<Token, ParseError> {
        //#[cfg(feature = "loud_parsing")]
        //self.annotate_parsing(&format!("Parser::expect_token({})", _expected));
        let (current_parse_start_loc, current_parse_string) = self
            .parsing_stack
            .last()
            .unwrap_or(&(SourceLoc::none(), String::from("UNKNOWN")))
            .to_owned();

        let (upcoming_token, upcoming_loc) = self.peek_token_with_loc()?;
        if upcoming_token.eq(&expected) {
            Ok(self.next_token()?)
        } else {
            Err(ParseError::unexpected_token(
                upcoming_loc,
                upcoming_token,
                &[expected],
                current_parse_string,
                current_parse_start_loc,
                SourceLoc::from(std::panic::Location::caller()),
            ))
        }
    }

    fn expect_token_with_loc(&mut self, expected: Token) -> Result<(Token, SourceLoc), ParseError> {
        match self.expect_token(expected) {
            Ok(token) => Ok((token, self.last_match.clone())),
            Err(e) => Err(e),
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
            upcoming_loc,
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
        let (_parse_start, parsed_description) = self
            .parsing_stack
            .pop()
            .expect("Mismatched loud parsing tracking");
        tracing::event!(
            tracing::Level::TRACE,
            "Finish parsing {}: {}",
            parsed_description,
            _parsed
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

impl<'a> Parser<'a> {
    pub fn parse(
        &mut self,
        parent_module_path: &std::path::Path,
        module_name: String,
    ) -> Result<parse_rules::module::ModuleResult, ParseError> {
        self.module_parser()
            .parse_module_contents(parent_module_path, module_name)
    }
}
