use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{
        Ast, IdentifierTree, generics::OptionalGenericParamsListTree, items::FunctionDefinitionTree,
    },
    loc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ImplementationTree {
    pub impl_keyword: loc::Span,
    pub generic_params: OptionalGenericParamsListTree,
    pub for_: IdentifierTree,
    pub implemented_for_generic_params: OptionalGenericParamsListTree,
    pub items: Vec<FunctionDefinitionTree>,
    pub close_brace: loc::Span,
}

impl Ast for ImplementationTree {
    fn span(&self) -> loc::Span {
        self.impl_keyword.clone().merge(&self.close_brace).unwrap()
    }
}

impl fmt::Display for ImplementationTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut params: String = format!("{}", self.generic_params);
        if !params.is_empty() {
            params = format!("<{params}>");
        }

        let mut for_params = format!("{}", self.implemented_for_generic_params);
        if !for_params.is_empty() {
            for_params = format!("<{for_params}>");
        }

        write!(f, "Impl{} {}{}", params, self.for_, for_params).and_then(|()| {
            for item in &self.items {
                write!(f, "{item}")?;
            }
            Ok(())
        })
    }
}
