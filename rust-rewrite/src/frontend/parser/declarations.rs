use crate::frontend::{ast::*, lexer::token::Token};

use super::{ParseError, Parser};

impl<'a> Parser<'a> {
    // TODO: pass loc of string to get true start loc of declaration
    pub fn parse_variable_declaration(&mut self) -> Result<VariableDeclarationTree, ParseError> {
        let start_loc = self.start_parsing("variable declaration")?;

        let declaration = VariableDeclarationTree::new(start_loc, self.parse_identifier()?, {
            self.expect_token(Token::Colon)?;
            self.parse_type()?
        });

        self.finish_parsing(&declaration)?;

        Ok(declaration)
    }

    pub fn parse_function_declaration_or_definition(
        &mut self,
    ) -> Result<TranslationUnit, ParseError> {
        let _start_loc = self.start_parsing("function declaration/definition")?;

        let prototype = self.parse_function_prototype()?;

        let decl_or_def = match self.parse_function_definition(prototype.clone()) {
            Ok(definition) => Ok(TranslationUnit::FunctionDefinition(definition)),
            Err(_) => Ok(TranslationUnit::FunctionDeclaration(prototype)),
        };

        self.finish_parsing(decl_or_def.as_ref().unwrap())?;

        decl_or_def
    }

    fn parse_function_prototype(&mut self) -> Result<FunctionDeclarationTree, ParseError> {
        let start_loc = self.start_parsing("function prototype")?;

        // start with fun
        self.expect_token(Token::Fun)?;

        let name = self.parse_identifier()?;

        self.expect_token(Token::LParen)?;
        let mut arguments = Vec::<VariableDeclarationTree>::new();
        loop {
            match self.peek_token()? {
                // argument declaration
                Token::Identifier(_) => {
                    arguments.push(self.parse_variable_declaration()?);
                    match self.peek_token()? {
                        Token::Comma => self.next_token()?, // expect another argument declaration after comma
                        Token::RParen => break,             // loop again to handle the rparen
                        _ => self.unexpected_token(&[Token::Comma, Token::RParen])?, // everything else is uenxpected
                    };
                }
                Token::RParen => break, // done on rparen
                _ => self.unexpected_token(&[Token::Identifier("".into())])?,
            }
        }
        // consume closing paren
        self.expect_token(Token::RParen)?;

        let return_type = match self.peek_token()? {
            Token::Arrow => {
                self.next_token()?;
                Some(self.parse_type()?)
            }
            _ => None,
        };

        let prototype = FunctionDeclarationTree::new(start_loc, name, arguments, return_type);
        self.finish_parsing(&prototype)?;
        Ok(prototype)
    }

    pub fn parse_function_definition(
        &mut self,
        prototype: FunctionDeclarationTree,
    ) -> Result<FunctionDefinitionTree, ParseError> {
        self.start_parsing("function definition")?;

        let function_body = self.parse_block_expression()?;

        let parsed_definition = FunctionDefinitionTree::new(prototype, function_body);
        self.finish_parsing(&parsed_definition)?;
        Ok(parsed_definition)
    }

    pub fn parse_struct_definition(&mut self) -> Result<StructDefinitionTree, ParseError> {
        let start_loc = self.start_parsing("struct definition")?;

        self.expect_token(Token::Struct)?;
        let struct_name = self.parse_identifier()?;
        self.expect_token(Token::LCurly)?;

        let mut struct_fields = Vec::new();

        loop {
            match self.peek_token()? {
                Token::Identifier(_) => {
                    struct_fields.push(self.parse_variable_declaration()?);

                    if matches!(self.peek_token()?, Token::Comma) {
                        self.next_token()?;
                    }
                }
                Token::RCurly => {
                    self.next_token()?;
                    break;
                }
                _ => {
                    self.unexpected_token::<TranslationUnit>(&[Token::Identifier("".into())])?;
                }
            }
        }

        let struct_definition = StructDefinitionTree::new(start_loc, struct_name, struct_fields);
        self.finish_parsing(&struct_definition)?;
        Ok(struct_definition)
    }

    pub fn parse_implementation(&mut self) -> Result<ImplementationTree, ParseError> {
        let start_loc = self.start_parsing("impl block")?;

        self.expect_token(Token::Impl)?;
        let implemented_for = self.parse_type()?;
        self.expect_token(Token::LCurly)?;

        let mut items: Vec<FunctionDefinitionTree> = Vec::new();

        while self.peek_token()? != Token::RCurly {
            let prototype = self.parse_function_prototype()?;
            items.push(self.parse_function_definition(prototype)?);
        }

        self.expect_token(Token::RCurly)?;

        let implementation = ImplementationTree::new(start_loc, implemented_for, items);
        self.finish_parsing(&implementation)?;
        Ok(implementation)
    }
}

#[cfg(test)]
mod tests {
    use super::Parser;
    use crate::{
        frontend::{ast::*, sourceloc::SourceLoc},
        midend::types::Type,
        Lexer,
    };

    #[test]
    fn parse_variable_declaration() {
        let mut p = Parser::new(Lexer::from_string("counter: u16"));

        assert_eq!(
            p.parse_variable_declaration(),
            Ok(VariableDeclarationTree::new(
                SourceLoc::new(1, 1),
                "counter".into(),
                TypeTree::new(SourceLoc::new(1, 10), Type::U16,)
            ))
        );
    }

    #[test]
    fn parse_function_declaration_or_definition() {
        let mut p = Parser::new(Lexer::from_string(
            "fun declared_only()
fun declared_and_defined() {}",
        ));

        assert_eq!(
            p.parse_function_declaration_or_definition(),
            Ok(TranslationUnit::FunctionDeclaration(
                FunctionDeclarationTree::new(
                    SourceLoc::new(1, 1),
                    "declared_only".into(),
                    vec![],
                    None,
                )
            ))
        );

        assert_eq!(
            p.parse_function_declaration_or_definition(),
            Ok(TranslationUnit::FunctionDefinition(
                FunctionDefinitionTree::new(
                    FunctionDeclarationTree::new(
                        SourceLoc::new(2, 1),
                        "declared_and_defined".into(),
                        vec![],
                        None,
                    ),
                    CompoundExpressionTree::new(SourceLoc::new(2, 28), vec![],)
                )
            ))
        );
    }

    #[test]
    fn parse_function_prototype() {
        let mut p = Parser::new(Lexer::from_string("fun add(a: u32, b: u32) -> u64"));

        assert_eq!(
            p.parse_function_prototype(),
            Ok(FunctionDeclarationTree::new(
                SourceLoc::new(1, 1),
                "add".into(),
                vec![
                    VariableDeclarationTree::new(
                        SourceLoc::new(1, 9),
                        "a".into(),
                        TypeTree::new(SourceLoc::new(1, 12), Type::U32,)
                    ),
                    VariableDeclarationTree::new(
                        SourceLoc::new(1, 17),
                        "b".into(),
                        TypeTree::new(SourceLoc::new(1, 20), Type::U32)
                    )
                ],
                Some(TypeTree::new(SourceLoc::new(1, 28), Type::U64)),
            ))
        );
    }
}
