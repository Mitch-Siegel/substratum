use crate::frontend::parser::parse_rules::{ast, Token};

use super::{ParseError, Parser};

impl Parser<'_> {
    // TODO: pass loc of string to get true start loc of declaration
    pub(crate) fn parse_argument_declaration(
        &mut self,
    ) -> Result<ast::items::function::ArgumentDeclarationTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("argument declaration")?;

        let mutable = match self.peek_token()? {
            Token::Mut => {
                self.expect_token(Token::Mut)?;
                true
            }
            _ => false,
        };

        let declaration = ast::items::function::ArgumentDeclarationTree {
            name: self.parse_identifier()?,
            type_: {
                self.expect_token(Token::Colon)?;
                self.type_parser().parse_type()?
            },
            mutable,
        };

        self.finish_parsing(declaration)
    }
}
