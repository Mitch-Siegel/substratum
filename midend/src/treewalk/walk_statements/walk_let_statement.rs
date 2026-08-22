use frontend::ast::{self, Ast};

use crate::{
    symtab,
    treewalk::{
        Collect, CollectResult, Linearize, LinearizeResult, PathedCtxTrait,
        PathedLinearizeCtxTrait, UnpathedFunctionLinearizeCtx, ValueCollectCtx,
    },
};

impl Collect<symtab::ValuePath> for ast::statements::LetTree {
    fn collect_inner(&self, mut ctx: ValueCollectCtx) -> CollectResult {
        ctx.declare_value(self.name.value.clone())?;
        ctx.into_result()
    }
}

impl<P, C> Linearize<UnpathedFunctionLinearizeCtx, P, C> for ast::statements::LetTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedFunctionLinearizeCtx, Path = P>,
{
    type Data = ();
    #[trace::instrument(skip(self), level = "trace", fields(tree_name = Self::name()))]
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        unimplemented!();
        /*
        let variable_type = match self.type_ {
            Some(type_tree) => type_tree.linearize(ctx),
            None => None,
        };

        let declared_variable: symtab::Variable =
            symtab::Variable::new(self.name.linearize(ctx), variable_type);
        let variable_path: symtab::DefPath = ctx
            .define::<symtab::Variable>(declared_variable)
            .unwrap();

        let declared_id = ctx.function_mut().values_mut().id_for_path(variable_path);

        let expr_loc = self.value.loc().clone();
        let expr_value = self.value.linearize(ctx);
        let assignment_line =
            ir::IrLine::new_assignment(expr_loc.start(), declared_id, expr_value);
        ctx.function_mut()
            .append_statement_to_current_block(assignment_line)
            .unwrap();
        */
    }
}
