use crate::frontend::parser::*;

impl<'a> Parser<'a> {
    pub fn parse_if_expression(&mut self) -> Result<ExpressionTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("if expression")?;

        self.expect_token(Token::If)?;

        self.expect_token(Token::LParen)?;
        let condition: ExpressionTree = self.parse_expression()?;
        self.expect_token(Token::RParen)?;

        let true_block = self.parse_block_expression()?;
        let false_block = match self.peek_token()? {
            Token::Else => {
                self.next_token()?;
                Some(self.parse_block_expression()?)
            }
            _ => None,
        };

        let if_expression = ExpressionTree {
            loc: start_loc.clone(),
            expression: Expression::If(Box::new(IfExpressionTree {
                loc: start_loc,
                condition,
                true_block,
                false_block,
            })),
        };

        self.finish_parsing(&if_expression)?;

        Ok(if_expression)
    }
}
