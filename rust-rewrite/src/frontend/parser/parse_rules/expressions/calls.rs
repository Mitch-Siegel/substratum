use crate::frontend::parser::parse_rules::*;

impl<'a, 'p> ExpressionParser<'a, 'p> {
    pub fn parse_call_params(
        &mut self,
        _allow_self: bool,
    ) -> Result<ast::expressions::calls::CallParamsTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("call params")?;

        let mut params = Vec::new();

        self.expect_token(Token::LParen)?;
        while !matches!(self.peek_token()?, Token::RParen) {
            if parse_rules::expressions::token_starts_expression(self.peek_token()?) {
                params.push(self.parse_expression()?);
            } else {
                self.unexpected_token(&parse_rules::expressions::expression_starters())?;
            }
        }
        self.expect_token(Token::RParen)?;

        let params_tree = ast::expressions::calls::CallParamsTree::new(start_loc, params);
        self.finish_parsing(&params_tree)?;
        Ok(params_tree)
    }

    pub fn parse_method_call_expression(
        &mut self,
        lhs: ExpressionTree,
    ) -> Result<ExpressionTree, ParseError> {
        self.start_parsing("method call expression")?;
        let start_loc = lhs.loc.clone();

        self.expect_token(Token::Dot)?;

        let method_call_expression = ast::expressions::MethodCallExpressionTree::new(
            start_loc.clone(),
            lhs,
            self.parse_identifier()?,
            self.parse_call_params(true)?,
        );

        let expression_tree = ExpressionTree::new(
            start_loc,
            Expression::MethodCall(Box::from(method_call_expression)),
        );
        self.finish_parsing(&expression_tree)?;
        Ok(expression_tree)
    }
}
