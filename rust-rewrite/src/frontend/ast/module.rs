use crate::{frontend::ast::*, trace};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct ModuleTree {
    pub module_path: Vec<String>,
    pub mod_keyword_loc: sourceloc::SourceSpan,
    pub name: IdentifierTree,
    pub items: Vec<ItemTree>,
}

impl Ast<()> for ModuleTree {
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

impl midend::treewalk::Treewalk<()> for ModuleTree {
    fn collect_symbols(&self, ctx: &mut midend::treewalk::CollectCtx) {
        trace::span_auto_debug!(
            "collect for module ",
            "{} ({:?}",
            self.name,
            self.module_path
        );
        let module_component = midend::symtab::DefPathComponent::Module(
            midend::symtab::ModuleName::new(self.name.value.clone()),
        );
        ctx.declare(module_component.clone()).unwrap();

        ctx.push_def_path(module_component.clone()).unwrap();

        for item in &self.items {
            item.collect_symbols(ctx);
        }
        ctx.pop_def_path(module_component).unwrap()
    }

    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> () {
        tracing::trace!(
            "Create symtab module \"{}\" at \"{}\"",
            self.name,
            ctx.def_path()
        );

        let module_name = self.name.linearize(ctx);

        ctx.define(midend::symtab::symbol::Module::new(module_name.clone()))
            .unwrap();
        ctx.push_def_path(
            midend::symtab::DefPathComponent::Module(midend::symtab::ModuleName {
                name: module_name.clone(),
            }),
            &Vec::new(),
        );

        for item in self.items {
            item.linearize(ctx)
        }

        ctx.pop_def_path(midend::symtab::DefPathComponent::Module(
            midend::symtab::ModuleName { name: module_name },
        ))
        .unwrap();
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
