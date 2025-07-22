use crate::frontend::parser::*;

impl<'a> Parser<'a> {
    pub fn parse_generic_param(&mut self) -> Result<GenericParamTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("generic param")?;

        let param = match self.peek_token()? {
            Token::Identifier(_) => {
                let param_name = self.parse_identifier()?;
                GenericParamTree::new(start_loc, param_name)
            }
            _ => self.unexpected_token(&[Token::Identifier("".into())])?,
        };

        self.finish_parsing(&param)?;
        Ok(param)
    }

    pub fn try_parse_generic_params_list(
        &mut self,
    ) -> Result<Option<GenericParamsListTree>, ParseError> {
        let (start_loc, _span) = self.start_parsing("generic params list")?;
        let maybe_params_tree = match self.peek_token()? {
            Token::LThan => {
                self.expect_token(Token::LThan)?;
                let mut params_list: Vec<GenericParamTree> = Vec::new();
                loop {
                    match self.peek_token()? {
                        Token::GThan => break,
                        _ => {
                            params_list.push(self.parse_generic_param()?);
                            match self.peek_token()? {
                                Token::Comma => {
                                    self.expect_token(Token::Comma)?;
                                }
                                _ => (),
                            }
                        }
                    }
                }
                self.expect_token(Token::GThan)?;
                Some(GenericParamsListTree::new(start_loc, params_list))
            }
            _ => None,
        };

        match &maybe_params_tree {
            Some(params_tree) => self.finish_parsing(params_tree)?,
            None => self.finish_parsing(&String::from("no generic params"))?,
        }
        Ok(maybe_params_tree)
    }

    pub fn parse_identifier_with_generic_params(
        &mut self,
    ) -> Result<IdentifierWithGenericsTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("identifier with generic params")?;

        let name = self.parse_identifier()?;
        let generic_params = self.try_parse_generic_params_list()?;

        let ident_tree = IdentifierWithGenericsTree::new(start_loc, name, generic_params);
        self.finish_parsing(&ident_tree)?;
        Ok(ident_tree)
    }
}
