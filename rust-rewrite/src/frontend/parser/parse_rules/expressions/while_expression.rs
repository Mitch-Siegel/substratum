use crate::frontend::parser::{parse_rules::*, *};

impl<'a, 'p> ExpressionParser<'a, 'p> {
    pub fn parse_while_expression(&mut self) -> Result<ast::ExpressionTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("while loop")?;

        self.expect_token(Token::While)?;

        self.expect_token(Token::LParen)?;
        let condition = self.parse_expression()?;
        self.expect_token(Token::RParen)?;

        let body = self.parse_block_expression()?;

        let while_loop = ast::expressions::WhileExpressionTree {
            loc: start_loc.clone(),
            condition,
            body,
        };

        self.finish_parsing(&while_loop)?;

        Ok(ExpressionTree {
            loc: start_loc,
            expression: ast::Expression::While(Box::from(while_loop)),
        })
    }
}
