use crate::frontend::parser::*;

impl<'a> Parser<'a> {
    pub fn parse_method_call_expression(
        &mut self,
        lhs: ExpressionTree,
    ) -> Result<ExpressionTree, ParseError> {
        self.start_parsing("method call expression")?;
        let start_loc = lhs.loc.clone();

        self.expect_token(Token::Dot)?;

        let method_call_expression = MethodCallExpressionTree::new(
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
