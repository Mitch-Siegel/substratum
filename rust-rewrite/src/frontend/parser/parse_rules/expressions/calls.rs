use crate::frontend::parser::parse_rules::*;

impl<'a, 'p> ExpressionParser<'a, 'p> {
    pub fn parse_call_params(
        &mut self,
        _allow_self: bool,
    ) -> Result<ast::expressions::calls::CallParamsTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("call params")?;

        let mut params = Vec::new();

        self.expect_token(Token::LParen)?;
        while self.peek_token()? != Token::RParen {
            params.push(self.parse_expression()?);
            if self.peek_token()? == Token::Comma {
                self.expect_token(Token::Comma)?;
            } else {
                break;
            }

            if self.peek_token()? == Token::RParen {
                break;
            }
        }
        self.expect_token(Token::RParen)?;

        let params_tree = ast::expressions::calls::CallParamsTree::new(start_loc, params);
        self.finish_parsing(params_tree)
    }

    pub fn parse_call_expression(
        &mut self,
        function_operand: Expression,
    ) -> Result<Expression, ParseError> {
        self.start_parsing("method call expression")?;
        let start_loc = function_operand.loc().clone();

        let call_params = self.parse_call_params(true)?;

        let call_expression_tree = ast::expressions::CallExpressionTree::new(
            start_loc.clone(),
            function_operand,
            call_params,
        );

        let expression_tree = Expression::Call(Box::from(call_expression_tree));
        self.finish_parsing(expression_tree)
    }
}
