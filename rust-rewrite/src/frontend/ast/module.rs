use crate::{
    frontend::ast::*,
    midend::{
        symtab::{Path, Symtab},
        treewalk::{Collect, Linearize, PathableContext},
    },
    trace,
};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct ModuleTree {
    pub module_path: Vec<String>,
    pub mod_keyword_loc: sourceloc::SourceSpan,
    pub name: IdentifierTree,
    pub items: Vec<ItemTree>,
}

impl ModuleTree {
    fn path_from_parent(&self, parent_path: midend::symtab::TypePath) -> midend::symtab::TypePath {
        let name = self.name.value.clone();
        parent_path.with_child_type(name)
    }

    #[tracing::instrument(skip(self, ctx), level = "debug", fields(parent_path = format!("{:?}", parent_path)))]
    pub fn collect_from_parent_path(
        &self,
        mut ctx: midend::treewalk::UnpathedCollectCtx,
        parent_path: midend::symtab::TypePath,
    ) -> midend::treewalk::CollectResult {
        let path = self.path_from_parent(parent_path);

        trace::debug!("collect for module {} ({:?}", self.name, self.module_path);

        let module_path = ctx.declare_type(path).unwrap();
        let mut module_ctx = ctx.with_path(module_path);

        for item in &self.items {
            module_ctx = item.collect_in_place(module_ctx)?;
        }

        Ok(module_ctx.take())
    }

    #[tracing::instrument(skip(self, ctx), level = "debug", fields(prefix_segments = format!("{:?}", parent_path)))]
    pub fn linearize_from_prefix_segments(
        self,
        mut ctx: midend::treewalk::UnpathedLinearizeCtx,
        parent_path: midend::symtab::TypePath,
    ) -> midend::treewalk::LinearizeResult<
        <Self as midend::treewalk::Linearize<midend::symtab::TypePath>>::Data,
    > {
        let path = self.path_from_parent(parent_path);

        let module_name = path.last().raw().into();

        trace::warning!(
            "here with path {}, module name {}, ctx path ",
            path,
            module_name
        );

        let module_path = ctx
            .define_type(
                path,
                midend::symtab::Type::from(midend::symtab::types::Module::new(module_name)),
            )
            .unwrap();

        trace::warning!("got module path of \"{}\"", module_path);

        let mut ctx = ctx.with_path(module_path);
        for item in self.items {
            (_, ctx) = item.linearize_in_place(ctx)?;
        }

        ctx.into_result(())
    }
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

impl midend::treewalk::Collect<midend::symtab::TypePath> for ModuleTree {
    #[tracing::instrument(skip(self, ctx), level = "debug", fields(tree_name = Self::reflect_name()))]
    fn collect_symbols(
        &self,
        ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        let path = ctx.path().clone();
        self.collect_from_parent_path(ctx.take(), path)
    }
}

impl midend::treewalk::Linearize<midend::symtab::TypePath> for ModuleTree {
    type Data = ();
    #[tracing::instrument(skip(self, ctx), level = "debug", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        ctx: midend::treewalk::TypeLinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let path = ctx.path().clone();
        self.linearize_from_prefix_segments(ctx.take(), path)
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

impl Ord for ModuleTree {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.module_path.cmp(&other.module_path)
    }
}

impl PartialOrd for ModuleTree {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}
