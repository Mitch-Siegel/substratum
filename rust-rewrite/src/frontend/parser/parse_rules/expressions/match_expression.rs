use crate::frontend::parser::{parse_rules::*, *};

impl<'a, 'p> ExpressionParser<'a, 'p> {
    fn parse_matched_pattern(
        &mut self,
    ) -> Result<ast::expressions::match_expression::PatternTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("pattern")?;

        let pattern = match self.peek_token()? {
            // TODO: PathInExpression
            Token::Identifier(_) => {
                let ident = self.parse_identifier()?;
                match self.peek_token()? {
                    Token::LParen => {
                        self.expect_token(Token::LParen)?;
                        // only support single destructuring for now
                        let single_tuple_contents = self.parse_identifier()?;
                        let tuple_contents_loc = SourceLocWithMod::new(
                            self.last_match.clone(),
                            self.current_module().into(),
                        );
                        self.expect_token(Token::RParen)?;
                        ast::expressions::match_expression::Pattern::TupleStructPattern(
                            ident,
                            vec![Box::new(
                                ast::expressions::match_expression::PatternTree::new(
                                    tuple_contents_loc,
                                    ast::expressions::match_expression::Pattern::IdentifierPattern(
                                        single_tuple_contents,
                                    ),
                                ),
                            )],
                        )
                    }
                    _ => ast::expressions::match_expression::Pattern::IdentifierPattern(ident),
                }
            }
            Token::UnsignedDecimalConstant(_) => {
                ast::expressions::match_expression::Pattern::LiteralPattern(
                    self.parse_literal_expression()?,
                )
            }
            _ => self.unexpected_token(&[
                Token::Identifier("".into()),
                Token::UnsignedDecimalConstant(0),
            ])?,
        };

        let pattern_tree = ast::expressions::match_expression::PatternTree::new(start_loc, pattern);
        self.finish_parsing(&pattern_tree)?;
        Ok(pattern_tree)
    }

    pub fn parse_match_expression(&mut self) -> Result<ExpressionTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("match")?;

        self.expect_token(Token::Match)?;
        let scrutinee_expression = self.parse_expression()?;
        self.expect_token(Token::LCurly)?;

        let mut arms = Vec::<ast::expressions::match_expression::MatchArmTree>::new();
        loop {
            match self.peek_token()? {
                Token::RCurly => break,
                _ => {
                    let matched_pattern = self.parse_matched_pattern()?;
                    self.expect_token(Token::FatArrow)?;
                    let match_action = match self.peek_token()? {
                        Token::LCurly => self.parse_block_expression()?,
                        _ => {
                            let single_expression = self.parse_expression()?;
                            ast::expressions::BlockExpressionTree::new(
                                single_expression.loc.clone(),
                                vec![StatementTree::new(
                                    single_expression.loc.clone(),
                                    ast::statements::Statement::Expression(single_expression),
                                )],
                            )
                        }
                    };
                    arms.push(ast::expressions::match_expression::MatchArmTree::new(
                        matched_pattern.loc.clone(),
                        matched_pattern,
                        match_action,
                    ));

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

        let match_expression = ExpressionTree::new(
            start_loc.clone(),
            Expression::Match(Box::new(
                ast::expressions::match_expression::MatchExpressionTree::new(
                    start_loc,
                    scrutinee_expression,
                    arms,
                ),
            )),
        );

        self.finish_parsing(&match_expression)?;
        Ok(match_expression)
    }
}
