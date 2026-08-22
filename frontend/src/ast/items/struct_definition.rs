use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, IdentifierTree, TypeTree, generics::OptionalGenericParamsListTree},
    sourceloc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct StructFieldTree {
    pub name: IdentifierTree,
    pub type_: TypeTree,
}

impl Ast for StructFieldTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.name.loc().merge(&self.type_.loc()).unwrap()
    }
}

impl fmt::Display for StructFieldTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {}", self.name, self.type_)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct StructDefinitionTree {
    pub struct_keyword_loc: sourceloc::SourceSpan,
    pub name: IdentifierTree,
    pub generic_params: OptionalGenericParamsListTree,
    pub fields: Vec<StructFieldTree>,
    pub(crate) close_brace_loc: sourceloc::SourceSpan,
}

impl Ast for StructDefinitionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.struct_keyword_loc
            .clone()
            .merge(&self.close_brace_loc)
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
