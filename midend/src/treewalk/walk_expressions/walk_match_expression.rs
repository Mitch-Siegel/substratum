use frontend::ast::{self, Ast};

use crate::{
    ir, symtab,
    treewalk::{
        Collect, CollectResult, FunctionLinearizeCtx, Linearize, LinearizeResult, PathedCtxTrait,
        UnpathedFunctionLinearizeCtx, ValueCollectCtx,
    },
};

impl Linearize<UnpathedFunctionLinearizeCtx, symtab::ScopePath, FunctionLinearizeCtx>
    for ast::expressions::match_expression::TupleStructTree
{
    type Data = ast::expressions::match_expression::PatternTree;
    fn linearize_inner(
        self,
        _ctx: FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        unimplemented!();
        /*
        PatternTree::TupleStruct(self)
        for field in tuple_struct.subpatterns.clone() {
            field.linearize(ctx);
        }
        */
    }
}

impl Collect<symtab::ValuePath> for ast::expressions::match_expression::PatternTree {
    fn collect_inner(&self, mut ctx: ValueCollectCtx) -> CollectResult {
        match self {
            Self::Literal(expr) => expr.collect_inner(ctx),
            Self::Identifier(ident) => {
                ctx.declare_value(ident.value.clone())?;
                ctx.into_result()
            }
            Self::TupleStruct(t) => {
                for pattern in &t.subpatterns {
                    ctx = pattern.collect_symbols(ctx)?;
                }
                ctx.into_result()
            }
        }
    }
}

impl Linearize<UnpathedFunctionLinearizeCtx, symtab::ScopePath, FunctionLinearizeCtx>
    for ast::expressions::match_expression::PatternTree
{
    type Data = Self;
    #[trace::instrument(skip(self), level = "trace", fields(tree_name = Self::name()))]
    fn linearize_inner(
        self,
        ctx: FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        unimplemented!("patterns");
        // match self.clone() {
        //     Self::Literal(_) => (),
        //     Self::Identifier(ident) => {
        //         let _variable_name = ident.linearize(ctx);
        //         unimplemented!();
        //     }
        //     Self::TupleStruct(tuple_struct) => {
        //         (_, ctx) = tuple_struct.linearize_in_place(ctx)?;
        //     }
        // };

        // ctx.into_result(self)
    }
}

impl Collect<symtab::ValuePath> for ast::expressions::match_expression::MatchArmTree {
    fn collect_inner(&self, mut _ctx: ValueCollectCtx) -> CollectResult {
        unimplemented!();
        /*
        let arm_subscope_idx = ctx.new_subscope().unwrap();
        self.pattern.collect_symbols(ctx);
        self.expression.collect_symbols(ctx);
        ctx.finish_subscope(arm_subscope_idx).unwrap();
        */
    }
}

impl Linearize<UnpathedFunctionLinearizeCtx, symtab::ScopePath, FunctionLinearizeCtx>
    for ast::expressions::match_expression::MatchArmTree
{
    type Data = (ast::expressions::match_expression::PatternTree, ir::ValueId);
    #[trace::instrument(skip(self), level = "trace", fields(tree_name = Self::name()))]
    fn linearize_inner(
        self,
        mut ctx: FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let pattern;
        let _arm_value: ir::ValueId;
        (pattern, ctx) = self.pattern.linearize(ctx)?;
        let (arm_value, ctx) = self.expression.linearize(ctx)?;
        ctx.into_result((pattern, arm_value))
    }
}

impl Collect<symtab::ValuePath> for ast::expressions::match_expression::MatchExpressionTree {
    fn collect_inner(&self, mut ctx: ValueCollectCtx) -> CollectResult {
        ctx = self.scrutinee_expression.collect_symbols(ctx)?;
        for arm in &self.arms {
            ctx = arm.collect_symbols(ctx)?;
        }

        ctx.into_result()
    }
}

impl Linearize<UnpathedFunctionLinearizeCtx, symtab::ScopePath, FunctionLinearizeCtx>
    for ast::expressions::match_expression::MatchExpressionTree
{
    type Data = ir::ValueId;
    #[trace::instrument(skip(self), level = "trace", fields(tree_name = Self::name()))]
    fn linearize_inner(
        self,
        mut ctx: FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let match_loc = self.loc();

        let parent_scope_def_path = ctx.path().clone();
        let switch_scope_def_path = ctx.reserve_subscope();

        ctx.create_switch(
            match_loc.clone().start(),
            parent_scope_def_path,
            switch_scope_def_path,
        );

        let scrutinee_value;
        (scrutinee_value, ctx) = self.scrutinee_expression.linearize(ctx)?;

        // TODO: consolidate each arm's result into result_value
        let result_value = ctx.values_mut().next_temp();

        let mut arm_values = Vec::new();

        for arm in self.arms {
            trace::warn!("start arm");
            let arm_loc = arm.loc();

            let case_scope_def_path = ctx.reserve_subscope();
            let arm_label = ctx.create_switch_case(case_scope_def_path);

            let (pattern, result_value);
            ((pattern, result_value), ctx) = arm.linearize(ctx)?;
            ctx.finish_switch_case(arm_loc.end());

            arm_values.push(ir::unlowered::operands::MatchArm {
                pattern,
                arm_label,
                result_value,
            });
            trace::warn!("finish arm");
        }

        ctx.append_statement_to_current_block(ir::IrLine::new_match(
            match_loc.clone().start(),
            scrutinee_value,
            arm_values,
        ));

        trace::warn!("finish match");

        // FIXME: (?) Convergence currently exists from the switch block itself to the after-switch
        // block, resulting in an unreachable jump instruction after the unlowered match IR.
        ctx.finish_switch(match_loc.end());
        ctx.into_result(result_value)
    }
}
