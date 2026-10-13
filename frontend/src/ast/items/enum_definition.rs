use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, IdentifierTree, TypeTree, generics::OptionalGenericParamsListTree},
    loc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TupleDataTree {
    pub(crate) open_paren: loc::Span,
    pub element_types: Vec<TypeTree>,
    pub(crate) close_paren: loc::Span,
}

impl Ast for TupleDataTree {
    fn span(&self) -> loc::Span {
        self.open_paren.clone().merge(&self.close_paren).unwrap()
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum EnumVariantDataTree {
    TupleData(TupleDataTree),
}

impl Ast for EnumVariantDataTree {
    fn span(&self) -> loc::Span {
        match self {
            Self::TupleData(tuple) => tuple.span(),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct EnumVariantTree {
    pub name: IdentifierTree,
    pub data: Option<EnumVariantDataTree>,
}

impl Ast for EnumVariantTree {
    fn span(&self) -> loc::Span {
        match &self.data {
            Some(data) => self.name.span().merge(&data.span()).unwrap(),
            None => self.name.span(),
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
    pub(crate) enum_keyword: loc::Span,
    pub name: IdentifierTree,
    pub generic_params: OptionalGenericParamsListTree,
    pub variants: Vec<EnumVariantTree>,
}

impl Ast for EnumDefinitionTree {
    fn span(&self) -> loc::Span {
        let mut loc = self.enum_keyword.clone().merge(&self.name.span()).unwrap();

        loc = loc.merge(&self.generic_params.span()).unwrap();

        for variant in &self.variants {
            loc = loc.merge(&variant.span()).unwrap();
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
