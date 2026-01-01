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
    fn path_from_prefix_segments(
        &self,
        prefix_segments: Vec<midend::symtab::PathSegment>,
    ) -> midend::symtab::DefPath {
        let name = self.name.value.clone();
        midend::symtab::DefPath::new(prefix_segments, midend::symtab::PathSegment::Type(name))
    }

    #[tracing::instrument(skip(self, ctx), level = "debug", fields(prefix_segments = format!("{:?}", prefix_segments)))]
    pub fn collect_from_prefix_segments(
        &self,
        mut ctx: midend::treewalk::UnpathedCollectCtx,
        prefix_segments: Vec<midend::symtab::PathSegment>,
    ) -> midend::treewalk::CollectResult {
        let path = self.path_from_prefix_segments(prefix_segments);

        trace::debug!("collect for module {} ({:?}", self.name, self.module_path);

        let module_path = ctx.declare(path).unwrap();
        let mut module_ctx = ctx.with_path(module_path);

        for item in &self.items {
            module_ctx = item.collect_same_path(module_ctx)?;
        }

        Ok(module_ctx.take())
    }

    #[tracing::instrument(skip(self, ctx), level = "debug", fields(prefix_segments = format!("{:?}", prefix_segments)))]
    pub fn linearize_from_prefix_segments(
        self,
        mut ctx: midend::treewalk::UnpathedLinearizeCtx,
        prefix_segments: Vec<midend::symtab::PathSegment>,
    ) -> midend::treewalk::LinearizeResult<<Self as midend::treewalk::Linearize>::Data> {
        let path = self.path_from_prefix_segments(prefix_segments);

        let module_name = path.last().raw().into();

        trace::warning!(
            "here with path {}, module name {}, ctx path ",
            path,
            module_name
        );

        let module_path = ctx
            .define(
                path,
                midend::symtab::SymbolDef::from(midend::symtab::Type::from(
                    midend::symtab::Module::new(module_name),
                )),
            )
            .unwrap();

        trace::warning!("got module path of \"{}\"", module_path);

        let mut ctx = ctx.with_path(module_path);
        for item in self.items {
            (_, ctx) = item.linearize_same_path(ctx)?;
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

impl midend::treewalk::Collect for ModuleTree {
    #[tracing::instrument(skip(self, ctx), level = "debug", fields(tree_name = Self::reflect_name()))]
    fn collect_symbols(
        &self,
        ctx: midend::treewalk::CollectCtx,
    ) -> midend::treewalk::CollectResult {
        let path = ctx.path().clone();
        let prefix_segments = path.into_iter().collect::<Vec<_>>();
        self.collect_from_prefix_segments(ctx.take(), prefix_segments)
    }
}

impl midend::treewalk::Linearize for ModuleTree {
    type Data = ();
    #[tracing::instrument(skip(self, ctx), level = "debug", fields(tree_name = Self::reflect_name()))]
    fn linearize(
        self,
        ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let path = ctx.path().clone();
        let prefix_segments = path.into_iter().collect::<Vec<_>>();
        self.linearize_from_prefix_segments(ctx.take(), prefix_segments)
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
