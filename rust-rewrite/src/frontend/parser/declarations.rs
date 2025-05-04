use crate::frontend::{ast::*, lexer::token::Token};

use super::{ParseError, Parser};

impl<'a> Parser<'a> {
    // TODO: pass loc of string to get true start loc of declaration
    pub fn parse_variable_declaration(&mut self) -> Result<VariableDeclarationTree, ParseError> {
        let start_loc = self.start_parsing("variable declaration")?;

        let declaration = VariableDeclarationTree {
            loc: start_loc,
            name: self.parse_identifier()?,
            typename: {
                self.expect_token(Token::Colon)?;
                self.parse_typename()?
            },
        };

        self.finish_parsing(&declaration)?;

        Ok(declaration)
    }

    pub fn parse_function_declaration_or_definition(
        &mut self,
    ) -> Result<TranslationUnit, ParseError> {
        let _start_loc = self.start_parsing("function declaration/definition")?;

        let function_declaration = self.parse_function_prototype()?;
        let function_declaration_or_definition = match self.peek_token()? {
            Token::LCurly => TranslationUnit::FunctionDefinition(FunctionDefinitionTree {
                prototype: function_declaration,
                body: self.parse_block_expression()?,
            }),
            _ => TranslationUnit::FunctionDeclaration(function_declaration),
        };

        self.finish_parsing(&function_declaration_or_definition)?;

        Ok(function_declaration_or_definition)
    }

    pub fn parse_struct_definition(&mut self) -> Result<TranslationUnit, ParseError> {
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

        let struct_definition = TranslationUnit::StructDefinition(StructDefinitionTree {
            loc: start_loc,
            name: struct_name,
            fields: struct_fields,
        });

        self.finish_parsing(&struct_definition)?;

        Ok(struct_definition)
    }
}
