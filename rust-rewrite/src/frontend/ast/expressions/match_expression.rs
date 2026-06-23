use crate::{frontend::ast::expressions::*, midend::{symtab::ValuePath, treewalk::PathedCtxTrait}, trace};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct TupleStructTree {
    pub name: IdentifierTree,
    pub subpatterns: Vec<PatternTree>,
    pub close_paren_loc: sourceloc::SourceSpan,
}

impl std::fmt::Display for TupleStructTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}(", self.name)?;
        let mut first = true;
        for subpattern in &self.subpatterns {
            if first {
                write!(f, "{}", subpattern)?;
                first = false;
            } else {
                write!(f, ", {}", subpattern)?;
            }
        }

        write!(f, ")")
    }
}

impl Ast for TupleStructTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.name.loc().merge(&self.close_paren_loc).unwrap()
    }
}

impl midend::treewalk::Linearize<midend::treewalk::TypeLinearizeCtx> for TupleStructTree {
    type Data = PatternTree;
    fn linearize_inner(
        self,
        _ctx: midend::treewalk::TypeLinearizeCtx,
    ) -> <midend::treewalk::TypeLinearizeCtx as midend::treewalk::PathedLinearizeCtxTrait>::Result::<Self::Data>{
        unimplemented!();
        /*
        PatternTree::TupleStruct(self)
        for field in tuple_struct.subpatterns.clone() {
            field.linearize(ctx);
        }
        */
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum PatternTree {
    Literal(Expression),
    Identifier(IdentifierTree),
    // TODO: PathInExpression
    TupleStruct(TupleStructTree),
}

impl Ast for PatternTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::Literal(e) => e.loc(),
            Self::Identifier(i) => i.loc(),
            Self::TupleStruct(t) => t.loc(),
        }
    }
}

impl std::fmt::Display for PatternTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Literal(e) => write!(f, "{}", e),
            Self::Identifier(i) => write!(f, "{}", i),
            Self::TupleStruct(t) => write!(f, "{}", t),
        }
    }
}

impl midend::treewalk::Collect<ValuePath> for PatternTree {
    fn collect_inner(
        &self,
        mut ctx: midend::treewalk::ValueCollectCtx,
    ) -> midend::treewalk::CollectResult {
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

impl midend::treewalk::Linearize<midend::treewalk::ValueFunctionLinearizeCtx> for PatternTree {
    type Data = PatternTree;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        ctx: midend::treewalk::ValueFunctionLinearizeCtx,
    ) -> <midend::treewalk::ValueFunctionLinearizeCtx as midend::treewalk::PathedLinearizeCtxTrait>::Result::<Self::Data>{
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

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct MatchArmTree {
    pub pattern: PatternTree,
    pub expression: BlockExpressionTree,
}

impl Ast for MatchArmTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.pattern.loc().merge(&self.expression.loc()).unwrap()
    }
}

impl Display for MatchArmTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} => {}", self.pattern, self.expression)
    }
}

impl midend::treewalk::Collect<ValuePath> for MatchArmTree {
    fn collect_inner(
        &self,
        mut _ctx: midend::treewalk::ValueCollectCtx,
    ) -> midend::treewalk::CollectResult {
        unimplemented!();
        /*
        let arm_subscope_idx = ctx.new_subscope().unwrap();
        self.pattern.collect_symbols(ctx);
        self.expression.collect_symbols(ctx);
        ctx.finish_subscope(arm_subscope_idx).unwrap();
        */
    }
}

impl midend::treewalk::Linearize<midend::treewalk::ValueFunctionLinearizeCtx> for MatchArmTree {
    type Data = (PatternTree, midend::ir::ValueId);
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        mut ctx: midend::treewalk::ValueFunctionLinearizeCtx,
    ) -> <midend::treewalk::ValueFunctionLinearizeCtx as midend::treewalk::PathedLinearizeCtxTrait>::Result::<Self::Data>{
        let pattern;
        let _arm_value: midend::ir::ValueId;
        (pattern, ctx) = self.pattern.linearize(ctx)?;
        let (arm_value, ctx) = self.expression.linearize(ctx)?;
        ctx.into_result((pattern, arm_value))
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct MatchExpressionTree {
    pub match_keyword_loc: sourceloc::SourceSpan,
    pub scrutinee_expression: Expression,
    pub arms: Vec<MatchArmTree>,
}

impl Ast for MatchExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        let mut loc = self
            .match_keyword_loc
            .clone()
            .merge(&self.scrutinee_expression.loc())
            .unwrap();

        for arm in &self.arms {
            loc = loc.merge(&arm.loc()).unwrap();
        }

        loc
    }
}

impl midend::treewalk::Collect<ValuePath> for MatchExpressionTree {
    fn collect_inner(
        &self,
        mut ctx: midend::treewalk::ValueCollectCtx,
    ) -> midend::treewalk::CollectResult {
        ctx = self.scrutinee_expression.collect_symbols(ctx)?;
        for arm in &self.arms {
            ctx = arm.collect_symbols(ctx)?;
        }

        ctx.into_result()
    }
}

impl midend::treewalk::Linearize<midend::treewalk::ValueFunctionLinearizeCtx> for MatchExpressionTree {
    type Data = midend::ir::ValueId;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        mut ctx: midend::treewalk::ValueFunctionLinearizeCtx,
    ) -> <midend::treewalk::ValueFunctionLinearizeCtx as midend::treewalk::PathedLinearizeCtxTrait>::Result::<Self::Data> {
        let match_loc = self.loc();

        let parent_scope_def_path = ctx.path().clone();
        let switch_scope_def_path = ctx.reserve_subscope();

        ctx.function_mut()
            .create_switch(
                match_loc.clone().start(),
                parent_scope_def_path,
                switch_scope_def_path,
            )
            .unwrap();

        let scrutinee_value;
        (scrutinee_value, ctx) = self.scrutinee_expression.linearize(ctx)?;

        // TODO: consolidate each arm's result into result_value
        let result_value = ctx.function_mut().values_mut().next_temp();

        let mut arm_values = Vec::new();

        for arm in self.arms {
            trace::warning!("start arm");
            let arm_loc = arm.loc();

            let case_scope_def_path = ctx.reserve_subscope();
            let arm_label = ctx
                .function_mut()
                .create_switch_case(case_scope_def_path)
                .unwrap();
            let (pattern, result_value);
            ((pattern, result_value), ctx) = arm.linearize(ctx)?;
            ctx.function_mut()
                .finish_switch_case(arm_loc.end())
                .unwrap();

            arm_values.push(midend::ir::unlowered::operands::MatchArm {
                pattern,
                arm_label,
                result_value,
            });
            trace::warning!("finish arm");
        }

        ctx.function_mut()
            .append_statement_to_current_block(midend::ir::IrLine::new_match(
                match_loc.clone().start(),
                scrutinee_value,
                arm_values,
            ))
            .unwrap();

        trace::warning!("finish match");

        // FIXME: (?) Convergence currently exists from the switch block itself to the after-switch
        // block, resulting in an unreachable jump instruction after the unlowered match IR.
        ctx.function_mut().finish_switch(match_loc.end()).unwrap();
        ctx.into_result(result_value)
    }
}

impl Display for MatchExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "match {} {{{:?}}}", self.scrutinee_expression, self.arms)
    }
}
