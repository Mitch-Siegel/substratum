use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, IdentifierTree, TypeTree, generics::OptionalGenericParamsListTree},
    loc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct StructFieldTree {
    pub name: IdentifierTree,
    pub type_: TypeTree,
}

impl Ast for StructFieldTree {
    fn span(&self) -> loc::Span {
        self.name.span().merge(&self.type_.span()).unwrap()
    }
}

impl fmt::Display for StructFieldTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {}", self.name, self.type_)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct StructDefinitionTree {
    pub struct_keyword: loc::Span,
    pub name: IdentifierTree,
    pub generic_params: OptionalGenericParamsListTree,
    pub fields: Vec<StructFieldTree>,
    pub(crate) close_brace: loc::Span,
}

impl Ast for StructDefinitionTree {
    fn span(&self) -> loc::Span {
        self.struct_keyword
            .clone()
            .merge(&self.close_brace)
            .unwrap()
    }
}

impl fmt::Display for StructDefinitionTree {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut fields = String::new();
        for field in &self.fields {
            fields += &field.to_string();
            fields += " ";
        }

        write!(f, "Struct Definition: {}: {}", self.name, fields)
    }
}
