use crate::frontend::parser::{parse_rules::*, *};

impl<'a, 'p> ExpressionParser<'a, 'p> {
    pub fn parse_identifier_expression(&mut self) -> Result<ExpressionTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("identifier expression")?;

        let identifier = self.parse_identifier()?;

        let expression_tree = ExpressionTree::new(start_loc, Expression::Identifier(identifier));
        self.finish_parsing(&expression_tree)?;
        Ok(expression_tree)
    }
}
