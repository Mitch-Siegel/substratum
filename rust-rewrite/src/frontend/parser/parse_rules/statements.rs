use crate::frontend::{ast, parser::parse_rules::*};

// parsing functions which yield an ExpressionTree
mod let_statement;

impl<'a, 'p> StatementParser<'a, 'p> {
    pub fn parse_statement(&mut self) -> Result<ast::StatementTree, ParseError> {
        let (start_loc, _span) = self.start_parsing("statement")?;

        let statement = ast::StatementTree {
            loc: start_loc,
            statement: match self.peek_token()? {
                Token::Let => {
                    let let_stmt = ast::statements::Statement::Let(
                        self.statement_parser().parse_let_statement()?,
                    );
                    self.expect_token(Token::Semicolon)?;
                    let_stmt
                }
                Token::Identifier(_) => ast::statements::Statement::Expression(
                    self.expression_parser().parse_expression()?,
                ),
                Token::If | Token::Match | Token::While | Token::LCurly => {
                    ast::statements::Statement::Expression(
                        self.expression_parser().parse_expression()?,
                    )
                }
                _ => self.unexpected_token::<ast::statements::Statement>(&[
                    Token::Identifier("".into()),
                    Token::If,
                    Token::While,
                    Token::LCurly,
                ])?,
            },
        };

        self.finish_parsing(statement)
    }
}
