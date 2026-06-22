use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct LetTree {
    pub let_keyword_loc: sourceloc::SourceSpan,
    pub name: IdentifierTree,
    pub type_: Option<TypeTree>,
    pub mutable: bool,
    pub value: Expression,
}

impl Ast for LetTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.let_keyword_loc
            .clone()
            .merge(&self.value.loc())
            .unwrap()
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

impl midend::treewalk::Collect<midend::symtab::ValuePath> for LetTree {
    fn collect_symbols(
        &self,
        mut ctx: midend::treewalk::ValueCollectCtx,
    ) -> midend::treewalk::CollectResult {
        ctx.declare_value(self.name.value.clone())?;
        Ok(ctx.take())
    }
}

impl midend::treewalk::Linearize<midend::symtab::ValuePath> for LetTree {
    type Data = ();
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        ctx: midend::treewalk::ValueLinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        unimplemented!();
        /*
        let variable_type = match self.type_ {
            Some(type_tree) => type_tree.linearize(ctx),
            None => None,
        };

        let declared_variable: midend::symtab::Variable =
            midend::symtab::Variable::new(self.name.linearize(ctx), variable_type);
        let variable_path: midend::symtab::DefPath = ctx
            .define::<midend::symtab::Variable>(declared_variable)
            .unwrap();

        let declared_id = ctx.function_mut().values_mut().id_for_path(variable_path);

        let expr_loc = self.value.loc().clone();
        let expr_value = self.value.linearize(ctx);
        let assignment_line =
            midend::ir::IrLine::new_assignment(expr_loc.start(), declared_id, expr_value);
        ctx.function_mut()
            .append_statement_to_current_block(assignment_line)
            .unwrap();
        */
    }
}
