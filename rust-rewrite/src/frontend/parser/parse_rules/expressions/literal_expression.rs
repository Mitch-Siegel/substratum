use crate::frontend::parser::{parse_rules::*, *};

impl<'a, 'p> ExpressionParser<'a, 'p> {
    pub fn parse_literal_expression(&mut self) -> Result<ast::ExpressionTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("literal expression")?;

        let literal_expression = match self.peek_token()? {
            Token::UnsignedDecimalConstant(value) => {
                self.next_token()?;
                ast::expressions::Expression::UnsignedDecimalConstant(value)
            }
            _ => self.unexpected_token(&[Token::UnsignedDecimalConstant(0)])?,
        };

        let expression_tree = ast::ExpressionTree::new(start_loc, literal_expression);
        self.finish_parsing(&expression_tree)?;
        Ok(expression_tree)
    }
}
