use crate::{
    frontend::ast::{
        midend, sourceloc, Ast, Display, IdentifierTree, ItemTree, LinearizeResult,
        NameReflectable, ReflectName,
    },
    midend::{
        symtab::{self, TypeOwner},
        treewalk::{self, Collect, Linearize, PathedCtxTrait, UnpathedCtxTrait},
    },
    trace,
};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct ModuleTree {
    pub(crate) module_path: Vec<String>,
    pub(crate) mod_keyword_loc: sourceloc::SourceSpan,
    pub name: IdentifierTree,
    pub items: Vec<ItemTree>,
}

impl ModuleTree {
    fn path_from_parent(&self, parent_path: midend::symtab::TypePath) -> midend::symtab::TypePath {
        let name = self.name.value.clone();
        parent_path.with_child_type(name)
    }

    #[tracing::instrument(skip(self, ctx), level = "debug")]
    pub(crate) fn collect_from_parent_path(
        &self,
        ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        trace::debug!("collect for module {} ({:?}", self.name, self.module_path);

        let mut module_ctx = ctx.with_child_type(self.name.value.clone());

        for item in &self.items {
            module_ctx = item.collect_symbols(module_ctx)?;
        }

        module_ctx.into_result()
    }

    pub(crate) fn collect_from_crate_root(
        &self,
        unpathed_ctx: midend::treewalk::UnpathedCollectCtx,
    ) -> midend::treewalk::CollectResult {
        let mut module_ctx = unpathed_ctx.with_path(symtab::TypePath::new(
            None::<symtab::TypePath>,
            self.name.value.clone(),
        ));

        for item in &self.items {
            module_ctx = item.collect_symbols(module_ctx)?;
        }

        module_ctx.into_result()
    }

    #[tracing::instrument(skip(self, ctx), level = "debug")]
    pub(crate) fn linearize_from_prefix_segments<C>(
        self,
        mut ctx: C,
    ) -> LinearizeResult<(), C::Unpathed>
    where
        C::Path: symtab::TypeOwner,
        C::Unpathed: treewalk::UnpathedLinearizeCtxTrait,
        C: treewalk::PathedLinearizeCtxTrait<Unpathed = treewalk::UnpathedLinearizeCtx>,
    {
        let module_name: String = self.name.value;

        trace::warning!(
            "here with path {}, module name {}, ctx path ",
            ctx.path(),
            module_name
        );

        let module_path = ctx
            .define_type(midend::symtab::Type::from(
                midend::symtab::types::Module::new(module_name.clone()),
            ))
            .unwrap();

        trace::warning!("got module path of \"{}\"", module_path);

        let mut ctx = ctx.with_child_type(module_name);
        for item in self.items {
            ((), ctx) = item.linearize(ctx).expect("unable to linearize item");
        }

        ctx.into_result(())
    }

    pub(crate) fn linearize_from_crate_root(
        self,
        unpathed_ctx: treewalk::UnpathedLinearizeCtx,
        crate_name: &str,
    ) -> LinearizeResult<(), treewalk::UnpathedLinearizeCtx> {
        let module_name: String = self.name.value;

        let mut ctx = unpathed_ctx.with_path(symtab::TypePath::new(
            None::<symtab::TypePath>,
            String::from(crate_name),
        ));

        trace::warning!(
            "here with path {}, module name {}, ctx path ",
            ctx.path(),
            module_name
        );

        // let module_path = ctx
        //     .define_type(midend::symtab::Type::from(
        //         midend::symtab::types::Module::new(module_name.clone()),
        //     ))
        //     .unwrap();

        // trace::warning!("got module path of \"{}\"", module_path);

        for item in self.items {
            ((), ctx) = item.linearize(ctx).expect("unable to linearize item");
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
            loc = loc.merge(&item.loc()).unwrap();
        }

        loc
    }
}

impl midend::treewalk::Collect<midend::symtab::TypePath> for ModuleTree {
    #[tracing::instrument(skip(self, ctx), level = "debug", fields(tree_name = Self::reflect_name()))]
    fn collect_inner(
        &self,
        ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        self.collect_from_parent_path(ctx)
    }
}

impl<C> midend::treewalk::Linearize<treewalk::UnpathedLinearizeCtx, symtab::TypePath, C>
    for ModuleTree
where
    C: treewalk::PathedLinearizeCtxTrait<
        Unpathed = treewalk::UnpathedLinearizeCtx,
        Path = symtab::TypePath,
    >,
{
    type Data = ();
    #[tracing::instrument(skip(self, ctx), level = "debug", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        ctx: C,
    ) -> treewalk::LinearizeResult<Self::Data, treewalk::UnpathedLinearizeCtx> {
        self.linearize_from_prefix_segments(ctx)
    }
}

impl Display for ModuleTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(f, "Module {}", self.name)?;
        for item in &self.items {
            writeln!(f, " - {item}")?;
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
