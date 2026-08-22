use std::fmt;

use crate::{
    ast::{Ast, IdentifierTree, TypeTree},
    sourceloc,
};

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct GenericParamTree {
    pub name: IdentifierTree,
}

impl Ast for GenericParamTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.name.loc()
    }
}

impl fmt::Display for GenericParamTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct GenericParamsListTree {
    pub(crate) open_angle_bracket_loc: sourceloc::SourceSpan,
    pub params: Vec<GenericParamTree>,
    pub(crate) close_angle_bracket_loc: sourceloc::SourceSpan,
}

impl Ast for GenericParamsListTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.open_angle_bracket_loc
            .clone()
            .merge(&self.close_angle_bracket_loc)
            .unwrap()
    }
}

impl GenericParamsListTree {
    pub(crate) fn into_vec(self) -> Vec<IdentifierTree> {
        self.params.into_iter().map(|param| param.name).collect()
    }

    #[must_use]
    pub fn into_vec_with_locs(self) -> Vec<(IdentifierTree, sourceloc::SourceSpan)> {
        self.params
            .into_iter()
            .map(|param| {
                let param_loc = param.loc();
                (param.name, param_loc)
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
    pub(crate) start_loc: sourceloc::SourceLoc,
    pub maybe_params: Option<GenericParamsListTree>,
}

impl Ast for OptionalGenericParamsListTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        if let Some(params) = &self.maybe_params {
            params.loc()
        } else {
            self.start_loc.clone().into()
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
    pub(crate) open_angle_bracket_loc: sourceloc::SourceSpan,
    pub args: Vec<TypeTree>,
    pub(crate) close_angle_bracket_loc: sourceloc::SourceSpan,
}

impl Ast for GenericArgsListTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.open_angle_bracket_loc
            .clone()
            .merge(&self.close_angle_bracket_loc)
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
