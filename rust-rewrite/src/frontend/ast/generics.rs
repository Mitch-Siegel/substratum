use crate::frontend::ast::*;
use std::collections::BTreeSet;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct GenericParamTree {
    pub loc: SourceLoc,
    pub name: String,
}
impl GenericParamTree {
    pub fn new(loc: SourceLoc, name: String) -> Self {
        Self { loc, name }
    }
}
impl Display for GenericParamTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct GenericParamsListTree {
    pub loc: SourceLoc,
    pub params: Vec<GenericParamTree>,
}

impl GenericParamsListTree {
    pub fn new(loc: SourceLoc, params: Vec<GenericParamTree>) -> Self {
        Self { loc, params }
    }
}

impl Display for GenericParamsListTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut first = true;
        for param in &self.params {
            if !first {
                write!(f, ", ")?;
            } else {
                first = false;
            }

            write!(f, "{}", param)?;
        }

        Ok(())
    }
}
impl treewalk::Linearize<Vec<String>> for GenericParamsListTree {
    #[tracing::instrument(skip(self), level = "trace")]
    fn linearize(self, _: &mut treewalk::LinearizeCtx) -> Vec<String> {
        let mut generic_params_set = BTreeSet::<String>::new();
        let generic_params: Vec<String> = self
            .params
            .into_iter()
            .map(|param| {
                if !generic_params_set.insert(param.name.clone()) {
                    panic!("Duplicate generic parameter {} @ {}", param.name, param.loc)
                }
                param.name
            })
            .collect();

        generic_params
    }
}

impl treewalk::Linearize<Vec<midend::types::Syntactic>> for GenericArgsListTree {
    #[tracing::instrument(skip(self), level = "trace")]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> Vec<midend::types::Syntactic> {
        let generic_args: Vec<midend::types::Syntactic> = self
            .args
            .into_iter()
            .map(|param| param.linearize(ctx))
            .collect();

        generic_args
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct GenericArgsListTree {
    pub loc: SourceLoc,
    pub args: Vec<TypeTree>,
}

impl GenericArgsListTree {
    pub fn new(loc: SourceLoc, args: Vec<TypeTree>) -> Self {
        Self { loc, args }
    }
}

impl Display for GenericArgsListTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut first = true;
        for param in &self.args {
            if !first {
                write!(f, ", ")?;
            } else {
                first = false;
            }

            write!(f, "{}", param)?;
        }

        Ok(())
    }
}

#[derive(ReflectName, Clone, Debug, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct IdentifierWithGenericsTree {
    pub loc: SourceLoc,
    pub name: String,
    pub generic_params: Option<generics::GenericParamsListTree>,
}
impl IdentifierWithGenericsTree {
    pub fn new(
        loc: SourceLoc,
        name: String,
        generic_params: Option<generics::GenericParamsListTree>,
    ) -> Self {
        Self {
            loc,
            name,
            generic_params,
        }
    }
}
impl Display for IdentifierWithGenericsTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let generic_params_string = match &self.generic_params {
            Some(params) => String::from(format!("<{}>", params)),
            None => String::new(),
        };

        write!(f, "{}{}", self.name, generic_params_string)
    }
}

impl treewalk::Linearize<(String, Vec<String>)> for IdentifierWithGenericsTree {
    #[tracing::instrument(skip(self), level = "trace")]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> (String, Vec<String>) {
        (
            self.name,
            match self.generic_params {
                Some(params) => params.linearize(ctx),
                None => Vec::new(),
            },
        )
    }
}
