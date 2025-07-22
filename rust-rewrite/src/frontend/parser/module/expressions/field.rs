use crate::frontend::parser::*;

impl<'a> Parser<'a> {
    pub fn parse_field_expression(
        &mut self,
        lhs: ExpressionTree,
    ) -> Result<ExpressionTree, ParseError> {
        self.start_parsing("field expression")?;
        let start_loc = lhs.loc.clone();

        self.expect_token(Token::Dot)?;
        let field_expression =
            FieldExpressionTree::new(start_loc.clone(), lhs, self.parse_identifier()?);

        let expression_tree = ExpressionTree::new(
            start_loc,
            Expression::FieldExpression(Box::from(field_expression)),
        );
        self.finish_parsing(&expression_tree)?;
        Ok(expression_tree)
    }
}
