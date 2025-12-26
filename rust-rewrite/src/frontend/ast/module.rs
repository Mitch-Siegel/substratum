use crate::{frontend::ast::*, trace};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct ModuleTree {
    pub module_path: Vec<String>,
    pub mod_keyword_loc: sourceloc::SourceSpan,
    pub name: IdentifierTree,
    pub items: Vec<ItemTree>,
}

impl Ast for ModuleTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        let mut loc = self
            .mod_keyword_loc
            .clone()
            .merge(&self.name.loc())
            .unwrap();

        for item in &self.items {
            loc = loc.merge(&item.loc()).unwrap()
        }

        loc
    }
}

impl midend::treewalk::Collect for ModuleTree {
    fn collect_symbols(
        &self,
        mut ctx: midend::treewalk::CollectCtx,
    ) -> midend::treewalk::CollectResult {
        let _span = trace::span_auto_debug!(
            "collect for module ",
            "{} ({:?}",
            self.name,
            self.module_path
        );
        let name = self.name.value.clone();

        let module_path = ctx.declare_type(name).unwrap();
        let mut module_ctx = ctx.with_path(module_path);

        for item in &self.items {
            module_ctx = item.collect_same_path(module_ctx)?;
        }

        Ok(module_ctx.take())
    }
}

impl midend::treewalk::Linearize for ModuleTree {
    type Data = ();
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(
        self,
        ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        tracing::trace!(
            "Create symtab module \"{}\" at \"{}\"",
            self.name,
            ctx.path()
        );
        unimplemented!();
        /*
        let module_name = self.name.linearize(ctx);

        let module_path = ctx
            .define_type(midend::symtab::Module::new(module_name.clone()).into())
            .unwrap();
        (ctx, _) = ctx.with_path(module_path);

        for item in self.items {
            item.linearize(ctx)
        }*/
    }
}

impl Display for ModuleTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(f, "Module {}", self.name)?;
        for item in &self.items {
            writeln!(f, " - {}", item)?;
        }
        Ok(())
    }
}
