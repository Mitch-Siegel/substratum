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
                self.parse_type()?
            },
        };

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

    pub fn parse_function_definition(
        &mut self,
        prototype: FunctionDeclarationTree,
    ) -> Result<FunctionDefinitionTree, ParseError> {
        self.start_parsing("function definition")?;

        let parsed_definition = FunctionDefinitionTree {
            prototype,
            body: self.parse_block_expression()?,
        };

        self.finish_parsing(&parsed_definition)?;

        Ok(parsed_definition)
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

    pub fn parse_implementation(&mut self) -> Result<TranslationUnit, ParseError> {
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

        let implementation = TranslationUnit::Implementation(ImplementationTree {
            loc: start_loc,
            type_name: implemented_for,
            items,
        });

        self.finish_parsing(&implementation)?;
        Ok(implementation)
    }
}
