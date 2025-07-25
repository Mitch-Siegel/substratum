use crate::frontend::parser::{parse_rules::*, *};

impl<'a, 'p> ExpressionParser<'a, 'p> {
    pub fn parse_field_expression(
        &mut self,
        lhs: ExpressionTree,
    ) -> Result<ExpressionTree, ParseError> {
        self.start_parsing("field expression")?;
        let start_loc = lhs.loc.clone();

        self.expect_token(Token::Dot)?;
        let field_expression = ast::expressions::FieldExpressionTree::new(
            start_loc.clone(),
            lhs,
            self.parse_identifier()?,
        );

        let expression_tree = ast::ExpressionTree::new(
            start_loc,
            ast::expressions::Expression::FieldExpression(Box::from(field_expression)),
        );
        self.finish_parsing(&expression_tree)?;
        Ok(expression_tree)
    }
}
