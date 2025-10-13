use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct LetTree {
    pub loc: SourceLoc,
    pub name: String,
    pub type_: Option<TypeTree>,
    pub mutable: bool,
    pub value: Option<ExpressionTree>,
}

impl LetTree {
    pub fn new(
        loc: SourceLoc,
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

impl treewalk::CollectSymbols for LetTree {
    fn collect_symbols(&self, ctx: &mut treewalk::CollectCtx) {
        ctx.declare(midend::symtab::DefPathComponent::Variable(
            self.name.clone(),
        ))
        .unwrap();
    }
}

impl treewalk::Linearize<midend::ir::ValueId> for LetTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> midend::ir::ValueId {
        let variable_type = match self.type_ {
            Some(type_tree) => Some(type_tree.linearize(ctx)),
            None => None,
        };

        let declared_variable: midend::symtab::Variable =
            midend::symtab::Variable::new(self.name.clone(), variable_type);
        let variable_path: midend::symtab::DefPath = ctx
            .insert::<midend::symtab::Variable>(declared_variable)
            .unwrap();
        ctx.function_mut()
            .values_mut()
            .id_for_variable(variable_path)
    }
}
