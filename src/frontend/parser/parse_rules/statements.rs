use crate::frontend::{
    ast,
    parser::parse_rules::{ParseError, StatementParser, Token},
};

// parsing functions which yield an ExpressionTree
mod let_statement;

impl StatementParser<'_, '_> {
    pub(crate) fn parse_statement(&mut self) -> Result<ast::StatementTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("statement")?;

        let statement = match self.peek_token()? {
            Token::Let => {
                let let_stmt = ast::statements::StatementTree::Let(Box::new(
                    self.statement_parser().parse_let_statement()?,
                ));
                self.expect_token(Token::Semicolon)?;
                let_stmt
            }
            Token::Identifier(_) => ast::statements::StatementTree::Expression(
                self.expression_parser().parse_expression()?,
            ),
            Token::If | Token::Match | Token::While | Token::LCurly => {
                ast::statements::StatementTree::Expression(
                    self.expression_parser().parse_expression()?,
                )
            }
            _ => self.unexpected_token(&[
                Token::Identifier(String::new()),
                Token::If,
                Token::While,
                Token::LCurly,
            ])?,
        };

        self.finish_parsing(statement)
    }
}
