use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, IdentifierTree, TypeTree, generics::OptionalGenericParamsListTree},
    sourceloc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TupleDataTree {
    pub(crate) open_paren_loc: sourceloc::SourceSpan,
    pub element_types: Vec<TypeTree>,
    pub(crate) close_paren_loc: sourceloc::SourceSpan,
}

impl Ast for TupleDataTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.open_paren_loc
            .clone()
            .merge(&self.close_paren_loc)
            .unwrap()
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum EnumVariantDataTree {
    TupleData(TupleDataTree),
}

impl Ast for EnumVariantDataTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::TupleData(tuple) => tuple.loc(),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct EnumVariantTree {
    pub name: IdentifierTree,
    pub data: Option<EnumVariantDataTree>,
}

impl Ast for EnumVariantTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match &self.data {
            Some(data) => self.name.loc().merge(&data.loc()).unwrap(),
            None => self.name.loc(),
        }
    }
}

impl fmt::Display for EnumVariantTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match &self.data {
            Some(variant_data) => write!(f, "{}: {:?}", self.name, variant_data),
            None => write!(f, "{}", self.name),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct EnumDefinitionTree {
    pub(crate) enum_keyword_loc: sourceloc::SourceSpan,
    pub name: IdentifierTree,
    pub generic_params: OptionalGenericParamsListTree,
    pub variants: Vec<EnumVariantTree>,
}

impl Ast for EnumDefinitionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        let mut loc = self
            .enum_keyword_loc
            .clone()
            .merge(&self.name.loc())
            .unwrap();

        loc = loc.merge(&self.generic_params.loc()).unwrap();

        for variant in &self.variants {
            loc = loc.merge(&variant.loc()).unwrap();
        }

        loc
    }
}

impl fmt::Display for EnumDefinitionTree {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut fields = String::new();
        for variant in &self.variants {
            fields += &variant.to_string();
            fields += " ";
        }

        write!(f, "Enum Definition: {}: {}", self.name, fields)
    }
}
