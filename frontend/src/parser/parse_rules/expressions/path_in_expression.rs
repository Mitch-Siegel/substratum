use crate::{
    ast,
    lexer::Token,
    parser::{ParseError, Parser, parse_rules::ExpressionParser},
};

impl ExpressionParser<'_, '_> {
    fn try_parse_expr_segment_data(
        parser: &mut Parser,
    ) -> Result<Option<ast::GenericArgsListTree>, ParseError> {
        let (_start_loc, _span) = parser.start_parsing("optional path segment data")?;

        let maybe_data = match parser.peek_token()? {
            Token::PathSep => match parser.lookahead_token(1)? {
                Token::LThan => {
                    parser.expect_token(Token::PathSep)?;
                    Some(parser.item_parser().parse_generic_args_list()?)
                }
                _ => None,
            },
            Token::LThan => Some(parser.item_parser().parse_generic_args_list()?),
            _ => None,
        };

        parser.finish_parsing(maybe_data)
    }

    pub(crate) fn parse_path_in_expression(
        &mut self,
    ) -> Result<ast::expressions::PathInExpressionTree, ParseError> {
        let (_start_loc, _span) = self.start_parsing("path in expression")?;

        let underlying_path = self
            .path_parser()
            .parse_path(Self::try_parse_expr_segment_data)?;

        let path_in_expr_tree = ast::expressions::PathInExpressionTree { underlying_path };

        self.finish_parsing(path_in_expr_tree)
    }
}
