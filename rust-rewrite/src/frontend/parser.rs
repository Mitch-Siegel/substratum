use std::collections::{BTreeSet, VecDeque};

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

pub use errors::ParseError;
pub use parse_rules::module::ModuleResult;

pub struct Parser<'a> {
    lexer: Lexer<'a>,
    module_parse_stack: Vec<IdentifierTree>,
    last_match: SourceSpan,
    upcoming_tokens: VecDeque<(Token, SourceSpan)>,
    parsing_stack: Vec<(SourceLoc, String)>,
}

impl<'a> Parser<'a> {
    pub fn new(_module_name: String, module_path: &std::path::Path, lexer: Lexer<'a>) -> Self {
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
    pub fn parse(
        &mut self,
        mod_keyword_loc: sourceloc::SourceSpan,
        parent_module_path: &std::path::Path,
        module_name: String,
    ) -> Result<parse_rules::module::ModuleResult, ParseError> {
        let module_name_tree = ast::IdentifierTree {
            loc: mod_keyword_loc.clone(),
            value: module_name,
        };

        self.module_parser().parse_module_contents(
            mod_keyword_loc,
            parent_module_path,
            module_name_tree,
        )
    }
}

#[cfg(test)]
mod tests {
    use crate::frontend::parser::*;

    pub fn test_parser<'a>(input: &'a str) -> Parser<'a> {
        let module_path = std::path::Path::new("");
        Parser::new(
            "".into(),
            module_path,
            lexer::Lexer::<'a>::from_string(&input),
        )
    }
}
