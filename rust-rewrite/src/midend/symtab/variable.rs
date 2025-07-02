use std::fmt::Display;

use serde::Serialize;

use crate::midend::{symtab::*, types::Type};

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize, Hash)]
pub struct Variable {
    pub name: String,
    type_: Option<Type>,
}

impl Display for Variable {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}: {}",
            self.name,
            match &self.type_ {
                Some(type_) => format!("{}", type_),
                None => "?Unknown Type?".into(),
            },
        )
    }
}

impl Variable {
    pub fn new(name: String, type_: Option<Type>) -> Self {
        Variable { name, type_ }
    }

    pub fn type_(&self) -> &Type {
        &self.type_.as_ref().unwrap()
    }

    pub fn mangle_name_at_index(&mut self, index: usize) {
        self.name = String::from(format!("{}_{}", index, self.name));
    }
}

impl<'a> From<DefinitionResolver<'a>> for &'a Variable {
    fn from(resolver: DefinitionResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Variable(variable) => variable,
            symbol => panic!("Unexpected symbol seen for variable: {}", symbol),
        }
    }
}

impl<'a> Into<SymbolDef> for SymbolDefGenerator<'a, Variable> {
    fn into(self) -> SymbolDef {
        SymbolDef::Variable(self.to_generate_def_for)
    }
}

impl<'a> Symbol<'a> for Variable {
    type SymbolKey = String;
    fn symbol_key(&self) -> &Self::SymbolKey {
        &self.name
    }
}
