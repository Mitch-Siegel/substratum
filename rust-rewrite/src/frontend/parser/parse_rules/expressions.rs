use crate::frontend::parser::parse_rules::*;

mod arithmetic;
mod block_expression;
mod calls;
mod field;
mod if_expression;
mod literal_expression;
mod match_expression;
mod parenthesized_expression;
mod path_in_expression;
mod primary_expression;
mod while_expression;

impl<'a, 'p> ExpressionParser<'a, 'p> {
    pub fn parse_expression(&mut self) -> Result<Expression, ParseError> {
        let (_start_loc, _span) = self.start_parsing("expression")?;

        let mut expr = match self.peek_token()? {
            Token::SelfLower | Token::Identifier(_) => {
                Expression::PathIn(self.parse_path_in_expression()?)
            }
            Token::If => Expression::If(Box::new(self.parse_if_expression()?)),
            Token::Match => Expression::Match(Box::new(self.parse_match_expression()?)),
            Token::While => Expression::While(Box::new(self.parse_while_expression()?)),
            Token::UnsignedDecimalConstant(_) => self.parse_literal_expression()?,
            Token::LParen => self.parse_parenthesized_expression()?,
            _ => self.unexpected_token(&[
                Token::SelfLower,
                Token::Identifier("".into()),
                Token::If,
                Token::Match,
                Token::While,
                Token::UnsignedDecimalConstant(0),
                Token::LParen,
            ])?,
        };

        loop {
            trace::trace!("Expression builder loop top");
            match self.peek_token()? {
                Token::Dot => {
                    expr = self.parse_field_expression(expr)?;
                }
                Token::LParen => {
                    expr = self.parse_call_expression(expr)?;
                }
                Token::Plus
                | Token::Minus
                | Token::Star
                | Token::FSlash
                | Token::LThan
                | Token::GThan
                | Token::LThanE
                | Token::GThanE
                | Token::Equals
                | Token::NotEquals => expr = self.parse_binary_expression(expr)?,
                _ => break,
            }
        }

        self.finish_parsing(expr)
    }
}

#[cfg(test)]
mod tests {
    use crate::frontend::ast::builder as ast_builder;
    use crate::frontend::parser::{parse_rules::*, tests::*};

    pub fn example_expression() -> String {
        "(a + b * 2)".into()
    }

    #[test]
    fn parse_expression() {
        let expression = example_expression();
        let mut p = test_parser(&expression);
        assert_eq!(
            format!("{:?}", p.expression_parser().parse_expression()),
            format!(
                "{:?}",
                Ok::<_, ParseError>(ast_builder::add(
                    ast_builder::id(1, 2, "a"),
                    ast_builder::mul(
                        ast_builder::id(1, 6, "b"),
                        ast_builder::unsigned_decimal_constant(1, 10, 2)
                    )
                ))
            )
        );
    }
}
