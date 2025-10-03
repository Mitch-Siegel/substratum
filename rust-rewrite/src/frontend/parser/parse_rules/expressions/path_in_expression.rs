use crate::frontend::parser::parse_rules::*;

impl<'a, 'p> ExpressionParser<'a, 'p> {
    fn parse_ident_segment(
        &mut self,
    ) -> Result<ast::expressions::PathIdentSegmentTree, ParseError> {
        let (loc, _span) = self.start_parsing("path ident segment")?;

        let ident = match self.peek_token()? {
            Token::Identifier(ident) => ast::expressions::PathIdentSegment::Ident(ident),
            Token::Super => ast::expressions::PathIdentSegment::Super,
            _ => self.unexpected_token(&[Token::Identifier("".into()), Token::Super])?,
        };

        let segment_tree = ast::expressions::PathIdentSegmentTree { loc, ident };

        self.finish_parsing(&segment_tree)?;
        Ok(segment_tree)
    }

    fn parse_path_expr_segment(
        &mut self,
    ) -> Result<ast::expressions::PathExprSegmentTree, ParseError> {
        let (loc, _span) = self.start_parsing("path expr segment")?;

        let ident_tree = self.parse_ident_segment()?;
        let generic_args = match self.peek_token()? {
            Token::PathSep => match self.lookahead_token(2)? {
                Token::LThan => {
                    self.expect_token(Token::PathSep);
                    self.item_parser().try_parse_generic_params_list()?
                }
                _ => None,
            },
            _ => None,
        };

        let segment_tree = ast::expressions::PathExprSegmentTree {
            loc,
            ident_tree,
            generic_args,
        };
        self.finish_parsing(&segment_tree)?;
        Ok(segment_tree)
    }

    pub fn parse_path_in_expression(&mut self) -> Result<ExpressionTree, ParseError> {
        let (loc, _span) = self.start_parsing("path in expression")?;

        let mut segments = vec![self.parse_path_expr_segment()?];
        loop {
            match self.next_token()? {
                Token::PathSep => {
                    self.expect_token(Token::PathSep)?;
                    segments.push(self.parse_path_expr_segment()?);
                }
                _ => break,
            }
        }

        let path_in_expr_tree = ast::expressions::PathInExpressionTree {
            loc: loc.clone(),
            segments,
        };

        let expression_tree =
            ExpressionTree::new(loc, ast::Expression::PathInExpression(path_in_expr_tree));
        self.finish_parsing(&expression_tree)?;
        Ok(expression_tree)
    }
}
