use std::fmt;

use crate::{
    ast::{Ast, IdentifierTree, TypeTree},
    loc,
};

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct GenericParamTree {
    pub name: IdentifierTree,
}

impl Ast for GenericParamTree {
    fn span(&self) -> loc::Span {
        self.name.span()
    }
}

impl fmt::Display for GenericParamTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct GenericParamsListTree {
    pub(crate) open_angle_bracket: loc::Span,
    pub params: Vec<GenericParamTree>,
    pub(crate) close_angle_bracket: loc::Span,
}

impl Ast for GenericParamsListTree {
    fn span(&self) -> loc::Span {
        self.open_angle_bracket
            .clone()
            .merge(&self.close_angle_bracket)
            .unwrap()
    }
}

impl GenericParamsListTree {
    #[must_use]
    pub fn into_vec_with_spans(self) -> Vec<(IdentifierTree, loc::Span)> {
        self.params
            .into_iter()
            .map(|param| {
                let span = param.span();
                (param.name, span)
            })
            .collect()
    }
}

impl fmt::Display for GenericParamsListTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut first = true;
        for param in &self.params {
            if first {
                first = false;
            } else {
                write!(f, ", ")?;
            }

            write!(f, "{param}")?;
        }

        Ok(())
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct OptionalGenericParamsListTree {
    pub(crate) start: loc::Loc,
    pub maybe_params: Option<GenericParamsListTree>,
}

impl Ast for OptionalGenericParamsListTree {
    fn span(&self) -> loc::Span {
        if let Some(params) = &self.maybe_params {
            params.span()
        } else {
            self.start.clone().into()
        }
    }
}

impl fmt::Display for OptionalGenericParamsListTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match &self.maybe_params {
            Some(p) => write!(f, "{p}"),
            None => Ok(()),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct GenericArgsListTree {
    pub(crate) open_angle_bracket: loc::Span,
    pub args: Vec<TypeTree>,
    pub(crate) close_angle_bracket: loc::Span,
}

impl Ast for GenericArgsListTree {
    fn span(&self) -> loc::Span {
        self.open_angle_bracket
            .clone()
            .merge(&self.close_angle_bracket)
            .unwrap()
    }
}

impl fmt::Display for GenericArgsListTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut first = true;
        for param in &self.args {
            if first {
                first = false;
            } else {
                write!(f, ", ")?;
            }

            write!(f, "{param}")?;
        }

        Ok(())
    }
}
