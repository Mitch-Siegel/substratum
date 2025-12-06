use crate::frontend::{ast::expressions::MatchExpressionTree, parser::parse_rules::*};

impl<'a, 'p> ExpressionParser<'a, 'p> {
    fn parse_matched_pattern(
        &mut self,
    ) -> Result<ast::expressions::match_expression::PatternTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("pattern")?;

        let pattern_tree = match self.peek_token()? {
            // TODO: PathInExpression
            Token::Identifier(_) => {
                let name = self.parse_identifier()?;
                match self.peek_token()? {
                    Token::LParen => {
                        self.expect_token(Token::LParen)?;
                        // only support single destructuring for now
                        let single_tuple_contents = self.parse_identifier()?;
                        let tuple_contents_loc = self.last_match.clone();
                        let close_paren_loc = self.expect_token(Token::RParen)?;
                        ast::expressions::match_expression::PatternTree::TupleStruct(
                            ast::expressions::match_expression::TupleStructTree {
                                name,
                                subpatterns: vec![
                                    ast::expressions::match_expression::PatternTree::Identifier(
                                        single_tuple_contents,
                                    ),
                                ],
                                close_paren_loc,
                            },
                        )
                    }
                    _ => ast::expressions::match_expression::PatternTree::Identifier(name),
                }
            }
            Token::UnsignedDecimalConstant(_) => {
                ast::expressions::match_expression::PatternTree::Literal(
                    self.parse_literal_expression()?,
                )
            }
            _ => self.unexpected_token(&[
                Token::Identifier("".into()),
                Token::UnsignedDecimalConstant(0),
            ])?,
        };

        self.finish_parsing(pattern_tree)
    }

    pub fn parse_match_expression(&mut self) -> Result<MatchExpressionTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("match")?;

        let match_keyword_loc = self.expect_token(Token::Match)?;
        let scrutinee_expression = self.parse_expression()?;
        self.expect_token(Token::LCurly)?;

        let mut arms = Vec::<ast::expressions::match_expression::MatchArmTree>::new();
        loop {
            match self.peek_token()? {
                Token::RCurly => break,
                _ => {
                    let pattern = self.parse_matched_pattern()?;
                    self.expect_token(Token::FatArrow)?;
                    let expression = match self.peek_token()? {
                        Token::LCurly => self.parse_block_expression()?,
                        _ => {
                            let single_expression = self.parse_expression()?;
                            let dummy_loc = single_expression.loc();
                            ast::expressions::BlockExpressionTree {
                                open_brace_loc: dummy_loc.clone(),
                                statements: vec![ast::statements::StatementTree::Expression(
                                    single_expression,
                                )],
                                close_brace_loc: dummy_loc,
                            }
                        }
                    };
                    arms.push(ast::expressions::match_expression::MatchArmTree {
                        pattern,
                        expression,
                    });

                    match self.peek_token()? {
                        Token::Comma => {
                            self.expect_token(Token::Comma)?;
                        }
                        Token::RParen => break,
                        _ => self.unexpected_token(&[Token::Comma, Token::RParen])?,
                    }
                }
            }
        }

        self.expect_token(Token::RCurly)?;

        let match_expression = ast::expressions::match_expression::MatchExpressionTree {
            match_keyword_loc,
            scrutinee_expression,
            arms,
        };

        self.finish_parsing(match_expression)
    }
}
