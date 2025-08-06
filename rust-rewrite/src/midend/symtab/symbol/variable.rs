use std::fmt::Display;

use serde::Serialize;

use crate::midend::{symtab::*, types::Type};

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize, Hash)]
pub struct Variable {
    pub name: String,
    type_: Option<types::Syntactic>,
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
    pub fn new(name: String, type_: Option<types::Syntactic>) -> Self {
        Variable { name, type_ }
    }

    pub fn type_(&self) -> Option<&types::Syntactic> {
        self.type_.as_ref()
    }

    pub fn mangle_name_at_index(&mut self, index: usize) {
        self.name = String::from(format!("{}_{}", index, self.name));
    }
}

impl<'a> From<DefResolver<'a>> for &'a Variable {
    fn from(resolver: DefResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Variable(variable) => variable,
            symbol => panic!("Unexpected symbol seen for variable: {}", symbol),
        }
    }
}
impl<'a> From<MutDefResolver<'a>> for &'a mut Variable {
    fn from(resolver: MutDefResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Variable(variable) => variable,
            symbol => panic!("Unexpected symbol seen for variable: {}", symbol),
        }
    }
}

impl<'a> Into<SymbolDef> for DefGenerator<'a, Variable> {
    fn into(self) -> SymbolDef {
        SymbolDef::Variable(self.to_generate_def_for)
    }
}

impl Symbol for Variable {
    type SymbolKey = String;
    fn symbol_key(&self) -> &Self::SymbolKey {
        &self.name
    }
}
