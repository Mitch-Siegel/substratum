use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct LetTree {
    pub loc: SourceLocWithMod,
    pub name: String,
    pub type_: Option<TypeTree>,
    pub mutable: bool,
    pub value: Option<ExpressionTree>,
}
impl LetTree {
    pub fn new(
        loc: SourceLocWithMod,
        name: String,
        type_: Option<TypeTree>,
        mutable: bool,
        value: Option<ExpressionTree>,
    ) -> Self {
        Self {
            loc,
            name,
            type_,
            mutable,
            value,
        }
    }
}
impl Display for LetTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match &self.type_ {
            Some(type_) => write!(f, "let {}: {}", self.name, type_)?,
            None => write!(f, "let {}", self.name)?,
        }

        if self.mutable {
            write!(f, "mut ")?;
        }

        match &self.type_ {
            Some(type_) => write!(f, ": {}", type_),
            None => write!(f, ": ?"),
        }
    }
}
