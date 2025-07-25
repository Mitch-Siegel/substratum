use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct GenericParamTree {
    pub loc: SourceLocWithMod,
    pub name: String,
}
impl GenericParamTree {
    pub fn new(loc: SourceLocWithMod, name: String) -> Self {
        Self { loc, name }
    }
}
impl Display for GenericParamTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct GenericParamsListTree {
    pub loc: SourceLocWithMod,
    pub params: Vec<GenericParamTree>,
}

impl GenericParamsListTree {
    pub fn new(loc: SourceLocWithMod, params: Vec<GenericParamTree>) -> Self {
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

#[derive(ReflectName, Clone, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct IdentifierWithGenericsTree {
    pub loc: SourceLocWithMod,
    pub name: String,
    pub generic_params: Option<generics::GenericParamsListTree>,
}
impl IdentifierWithGenericsTree {
    pub fn new(
        loc: SourceLocWithMod,
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
