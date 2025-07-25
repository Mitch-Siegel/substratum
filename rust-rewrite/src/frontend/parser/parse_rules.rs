use crate::frontend::parser::*;

mod declarations;
mod expressions;
mod items;
pub mod module;
mod single_token;
mod statements;
mod types;

pub struct ExpressionParser<'a, 'p>(&'p mut Parser<'a>);

pub struct ItemParser<'a, 'p>(&'p mut Parser<'a>);

pub struct ModuleParser<'a, 'p>(&'p mut Parser<'a>);

pub struct StatementParser<'a, 'p>(&'p mut Parser<'a>);

pub struct TypeParser<'a, 'p>(&'p mut Parser<'a>);

impl<'a, 'p> std::ops::Deref for ExpressionParser<'a, 'p> {
    type Target = Parser<'a>;
    fn deref(&self) -> &Self::Target {
        self.0
    }
}
impl<'a, 'p> std::ops::DerefMut for ExpressionParser<'a, 'p> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.0
    }
}

impl<'a, 'p> std::ops::Deref for ItemParser<'a, 'p> {
    type Target = Parser<'a>;
    fn deref(&self) -> &Self::Target {
        self.0
    }
}
impl<'a, 'p> std::ops::DerefMut for ItemParser<'a, 'p> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.0
    }
}

impl<'a, 'p> std::ops::Deref for ModuleParser<'a, 'p> {
    type Target = Parser<'a>;
    fn deref(&self) -> &Self::Target {
        self.0
    }
}
impl<'a, 'p> std::ops::DerefMut for ModuleParser<'a, 'p> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.0
    }
}

impl<'a, 'p> std::ops::Deref for StatementParser<'a, 'p> {
    type Target = Parser<'a>;
    fn deref(&self) -> &Self::Target {
        self.0
    }
}

impl<'a, 'p> std::ops::DerefMut for StatementParser<'a, 'p> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.0
    }
}

impl<'a, 'p> std::ops::Deref for TypeParser<'a, 'p> {
    type Target = Parser<'a>;
    fn deref(&self) -> &Self::Target {
        self.0
    }
}
impl<'a, 'p> std::ops::DerefMut for TypeParser<'a, 'p> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.0
    }
}

impl<'a> Parser<'a> {
    pub fn expression_parser(&mut self) -> ExpressionParser<'a, '_> {
        ExpressionParser(self)
    }

    pub fn item_parser(&mut self) -> ItemParser<'a, '_> {
        ItemParser(self)
    }

    pub fn module_parser(&mut self) -> ModuleParser<'a, '_> {
        ModuleParser(self)
    }

    pub fn statement_parser(&mut self) -> StatementParser<'a, '_> {
        StatementParser(self)
    }

    pub fn type_parser(&mut self) -> TypeParser<'a, '_> {
        TypeParser(self)
    }
}
