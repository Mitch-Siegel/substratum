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

impl ValueWalk for LetTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut midend::linearizer::FunctionWalkContext) -> midend::ir::ValueId {
        let variable_type = match self.type_ {
            Some(type_tree) => Some(type_tree.walk(context)),
            None => None,
        };

        let declared_variable: midend::symtab::Variable =
            midend::symtab::Variable::new(self.name.clone(), variable_type);
        let variable_path: midend::symtab::DefPath = context
            .insert::<midend::symtab::Variable>(declared_variable)
            .unwrap();
        *context.value_for_variable(&variable_path)
    }
}
