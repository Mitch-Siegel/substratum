use crate::frontend::parser::{module::expressions::*, *};

mod method_call;

impl<'a> Parser<'a> {
    pub fn parse_call_params(&mut self, _allow_self: bool) -> Result<CallParamsTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("call params")?;

        let mut params = Vec::new();

        self.expect_token(Token::LParen)?;
        while !matches!(self.peek_token()?, Token::RParen) {
            if token_starts_expression(self.peek_token()?) {
                params.push(self.parse_expression()?);
            } else {
                self.unexpected_token(&expression_starters())?;
            }
        }
        self.expect_token(Token::RParen)?;

        let params_tree = CallParamsTree::new(start_loc, params);
        self.finish_parsing(&params_tree)?;
        Ok(params_tree)
    }
}
