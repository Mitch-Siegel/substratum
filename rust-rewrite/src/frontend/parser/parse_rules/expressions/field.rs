use crate::frontend::parser::parse_rules::*;

impl<'a, 'p> ExpressionParser<'a, 'p> {
    pub(crate) fn parse_field_expression(&mut self, lhs: Expression) -> Result<Expression, ParseError> {
        let (_start_loc, _span) = self.start_parsing("field expression")?;

        self.expect_token(Token::Dot)?;
        let field_expression = ast::expressions::FieldExpressionTree {
            receiver: lhs,
            field: self.parse_identifier()?,
        };

        let expression_tree = ast::expressions::Expression::Field(Box::from(field_expression));
        self.finish_parsing(expression_tree)
    }
}
