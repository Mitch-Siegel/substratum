use crate::frontend::parser::parse_rules::{ast, Expression, ExpressionParser, ParseError, Token};

impl ExpressionParser<'_, '_> {
    pub(crate) fn parse_call_params(
        &mut self,
        _allow_self: bool,
    ) -> Result<ast::expressions::calls::CallParamsTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("call params")?;

        let mut params = Vec::new();

        let open_paren_loc = self.expect_token(Token::LParen)?;
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
        let close_paren_loc = self.expect_token(Token::RParen)?;

        let params_tree = ast::expressions::calls::CallParamsTree {
            open_paren_loc,
            params,
            close_paren_loc,
        };
        self.finish_parsing(params_tree)
    }

    pub(crate) fn parse_call_expression(
        &mut self,
        function_operand: Expression,
    ) -> Result<Expression, ParseError> {
        let (_start_loc, _span) = self.start_parsing("method call expression")?;

        let params = self.parse_call_params(true)?;

        let call_expression_tree = ast::expressions::CallExpressionTree {
            function_operand,
            params,
        };

        let expression_tree = Expression::Call(Box::from(call_expression_tree));
        self.finish_parsing(expression_tree)
    }
}
