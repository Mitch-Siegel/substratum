use crate::parser::{ParseError, Parser, Token, WorklistItem, parse_rules};

mod declarations;
mod expressions;
mod items;
pub(crate) mod module;
mod path;
mod single_token;
mod statements;
mod types;

pub(crate) struct ExpressionParser<'a, 'p>(&'p mut Parser<'a>);

pub(crate) struct ItemParser<'a, 'p>(&'p mut Parser<'a>);

pub(crate) struct ModuleParser<'a, 'p>(&'p mut Parser<'a>);

pub(crate) struct StatementParser<'a, 'p>(&'p mut Parser<'a>);

pub(crate) struct TypeParser<'a, 'p>(&'p mut Parser<'a>);

pub(crate) struct PathParser<'a, 'p>(&'p mut Parser<'a>);

impl<'a> std::ops::Deref for ExpressionParser<'a, '_> {
    type Target = Parser<'a>;
    fn deref(&self) -> &Self::Target {
        self.0
    }
}
impl std::ops::DerefMut for ExpressionParser<'_, '_> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.0
    }
}

impl<'a> std::ops::Deref for ItemParser<'a, '_> {
    type Target = Parser<'a>;
    fn deref(&self) -> &Self::Target {
        self.0
    }
}
impl std::ops::DerefMut for ItemParser<'_, '_> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.0
    }
}

impl<'a> std::ops::Deref for ModuleParser<'a, '_> {
    type Target = Parser<'a>;
    fn deref(&self) -> &Self::Target {
        self.0
    }
}
impl std::ops::DerefMut for ModuleParser<'_, '_> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.0
    }
}

impl<'a> std::ops::Deref for StatementParser<'a, '_> {
    type Target = Parser<'a>;
    fn deref(&self) -> &Self::Target {
        self.0
    }
}

impl std::ops::DerefMut for StatementParser<'_, '_> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.0
    }
}

impl<'a> std::ops::Deref for TypeParser<'a, '_> {
    type Target = Parser<'a>;
    fn deref(&self) -> &Self::Target {
        self.0
    }
}
impl std::ops::DerefMut for TypeParser<'_, '_> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.0
    }
}

impl<'a> std::ops::Deref for PathParser<'a, '_> {
    type Target = Parser<'a>;
    fn deref(&self) -> &Self::Target {
        self.0
    }
}
impl std::ops::DerefMut for PathParser<'_, '_> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.0
    }
}

impl<'a> Parser<'a> {
    pub(crate) fn expression_parser(&mut self) -> ExpressionParser<'a, '_> {
        ExpressionParser(self)
    }

    pub(crate) fn item_parser(&mut self) -> ItemParser<'a, '_> {
        ItemParser(self)
    }

    pub(crate) fn module_parser(&mut self) -> ModuleParser<'a, '_> {
        ModuleParser(self)
    }

    pub(crate) fn statement_parser(&mut self) -> StatementParser<'a, '_> {
        StatementParser(self)
    }

    pub(crate) fn type_parser(&mut self) -> TypeParser<'a, '_> {
        TypeParser(self)
    }

    pub(crate) fn path_parser(&mut self) -> PathParser<'a, '_> {
        PathParser(self)
    }
}
