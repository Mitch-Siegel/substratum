use frontend::ast::{self, Ast};

use crate::{
    symtab::{self, TypeOwner},
    treewalk::{
        Collect, CollectResult, Linearize, LinearizeResult, PathedCtxTrait,
        PathedLinearizeCtxTrait, TypeCollectCtx, UnpathedCollectCtx, UnpathedCtxTrait,
        UnpathedLinearizeCtx, UnpathedLinearizeCtxTrait,
    },
};

fn path_from_parent(module: &ast::ModuleTree, parent_path: symtab::TypePath) -> symtab::TypePath {
    let name = module.name.value.clone();
    parent_path.with_child_type(name)
}

#[trace::instrument(skip(module, ctx), level = "debug")]
pub(crate) fn collect_from_parent_path(
    module: &ast::ModuleTree,
    ctx: TypeCollectCtx,
) -> CollectResult {
    trace::debug!("collect for module {}", module.name);

    let mut module_ctx = ctx.with_child_type(module.name.value.clone());

    for item in &module.items {
        module_ctx = item.collect_symbols(module_ctx)?;
    }

    module_ctx.into_result()
}

pub(crate) fn collect_from_crate_root(
    module: &ast::ModuleTree,
    unpathed_ctx: UnpathedCollectCtx,
) -> CollectResult {
    let mut module_ctx = unpathed_ctx.with_path(symtab::TypePath::new(
        None::<symtab::TypePath>,
        module.name.value.clone(),
    ));

    for item in &module.items {
        module_ctx = item.collect_symbols(module_ctx)?;
    }

    module_ctx.into_result()
}

#[trace::instrument(skip(module, ctx), level = "debug")]
pub(crate) fn linearize_from_prefix_segments<C>(
    module: ast::ModuleTree,
    mut ctx: C,
) -> LinearizeResult<(), C::Unpathed>
where
    C::Path: symtab::TypeOwner,
    C::Unpathed: UnpathedLinearizeCtxTrait,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedLinearizeCtx>,
{
    let module_name: String = module.name.value;

    trace::warn!(
        "here with path {}, module name {}, ctx path ",
        ctx.path(),
        module_name
    );

    let module_path = ctx
        .define_type(symtab::Type::Module(symtab::types::Module::new(
            module_name.clone(),
        )))
        .unwrap();

    trace::warn!("got module path of \"{}\"", module_path);

    let mut ctx = ctx.with_child_type(module_name);
    for item in module.items {
        ((), ctx) = item.linearize(ctx).expect("unable to linearize item");
    }

    ctx.into_result(())
}

pub(crate) fn linearize_from_crate_root(
    module: ast::ModuleTree,
    unpathed_ctx: UnpathedLinearizeCtx,
    crate_name: &str,
) -> LinearizeResult<(), UnpathedLinearizeCtx> {
    let module_name: String = module.name.value;

    let mut ctx = unpathed_ctx.with_path(symtab::TypePath::new(
        None::<symtab::TypePath>,
        String::from(crate_name),
    ));

    trace::warn!(
        "here with path {}, module name {}, ctx path ",
        ctx.path(),
        module_name
    );

    // let module_path = ctx
    //     .define_type(symtab::Type::from(
    //         symtab::types::Module::new(module_name.clone()),
    //     ))
    //     .unwrap();

    // trace::warning!("got module path of \"{}\"", module_path);

    for item in module.items {
        ((), ctx) = item.linearize(ctx).expect("unable to linearize item");
    }

    ctx.into_result(())
}

impl Collect<symtab::TypePath> for ast::ModuleTree {
    #[trace::instrument(skip(self, ctx), level = "debug", fields(tree_name = Self::name()))]
    fn collect_inner(&self, ctx: TypeCollectCtx) -> CollectResult {
        collect_from_parent_path(self, ctx)
    }
}

impl<C> Linearize<UnpathedLinearizeCtx, symtab::TypePath, C> for ast::ModuleTree
where
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedLinearizeCtx, Path = symtab::TypePath>,
{
    type Data = ();
    #[trace::instrument(skip(self, ctx), level = "debug", fields(tree_name = Self::name()))]
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, UnpathedLinearizeCtx> {
        linearize_from_prefix_segments(self, ctx)
    }
}
