use crate::frontend::parser::*;

impl<'a> Parser<'a> {
    pub fn parse_assignment_expression(
        &mut self,
        lhs: ExpressionTree,
    ) -> Result<ExpressionTree, ParseError> {
        self.start_parsing("assignment (rhs)")?;
        let start_loc = lhs.loc.clone();

        self.expect_token(Token::Assign)?;

        let rhs = self.parse_expression()?;

        let assignment = AssignmentTree::new(start_loc.clone(), lhs, rhs);

        let expression_tree = ExpressionTree::new(start_loc, Expression::Assignment(assignment));
        self.finish_parsing(&expression_tree)?;
        Ok(expression_tree)
    }
}
