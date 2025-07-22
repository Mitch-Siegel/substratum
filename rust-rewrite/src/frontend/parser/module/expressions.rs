use crate::frontend::parser::*;

mod arithmetic;
mod assignment;
mod block_expression;
mod calls;
mod field;
mod identifier_expression;
mod if_expression;
mod literal_expression;
mod match_expression;
mod parenthesized_expression;
mod primary_expression;
mod while_expression;

fn expression_starters() -> [Token; 5] {
    [
        Token::If,
        Token::While,
        Token::Identifier("".into()),
        Token::UnsignedDecimalConstant(0),
        Token::LParen,
    ]
}

fn token_starts_expression(t: Token) -> bool {
    match t {
        Token::SelfLower
        | Token::If
        | Token::Match
        | Token::While
        | Token::Identifier(_)
        | Token::UnsignedDecimalConstant(_)
        | Token::LParen => true,
        _ => false,
    }
}

impl<'a> Parser<'a> {
    pub fn parse_expression(&mut self) -> Result<ExpressionTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("expression")?;

        assert!(
            token_starts_expression(self.peek_token()?),
            "{} does not start an expression",
            self.peek_token()?
        ); // sanity-check this method call to self-validate

        let mut expr = match self.peek_token()? {
            Token::SelfLower => {
                let basic_loc = self.expect_token_with_loc(Token::SelfLower)?.1;
                let self_loc = SourceLocWithMod::new(basic_loc, self.current_module().into());

                let self_expression = ExpressionTree::new(self_loc, Expression::SelfLower);
                match self.lookahead_token(2)? {
                    Token::Dot => self.parse_method_call_expression(self_expression)?,
                    _ => self_expression,
                }
            }
            Token::If => self.parse_if_expression()?,
            Token::Match => self.parse_match_expression()?,
            Token::While => self.parse_while_expression()?,
            Token::Identifier(_) => self.parse_identifier_expression()?,
            Token::UnsignedDecimalConstant(_) => self.parse_literal_expression()?,
            Token::LParen => self.parse_parenthesized_expression()?,
            _ => self.unexpected_token(&[
                Token::If,
                Token::While,
                Token::Identifier("".into()),
                Token::UnsignedDecimalConstant(0),
                Token::LParen,
            ])?,
        };

        match self.peek_token()? {
            Token::Dot => {
                if matches!(self.lookahead_token(2)?, Token::LParen) {
                    expr = self.parse_method_call_expression(expr)?;
                } else {
                    expr = self.parse_field_expression(expr)?;
                }
            }
            _ => {}
        }

        let peeked = self.peek_token()?;
        match peeked {
            Token::Plus
            | Token::Minus
            | Token::Star
            | Token::FSlash
            | Token::LThan
            | Token::GThan
            | Token::LThanE
            | Token::GThanE
            | Token::Equals
            | Token::NotEquals => {
                let lhs = expr;
                expr = self.parse_binary_expression(lhs)?
            }

            _ => {
                #[cfg(feature = "loud_parsing")]
                println!("Peeked {} after lhs {} of expression", peeked, expr);
            }
        }

        match self.peek_token()? {
            Token::Assign => {
                expr = self.parse_assignment_expression(expr)?;
            }
            _ => {}
        }

        self.finish_parsing(&expr)?;

        Ok(expr)
    }
}
