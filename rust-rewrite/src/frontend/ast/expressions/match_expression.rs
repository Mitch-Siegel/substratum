use crate::{frontend::ast::expressions::*, trace};

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

impl midend::treewalk::Treewalk<PatternTree> for TupleStructTree {
    fn linearize(self, _ctx: &mut midend::treewalk::LinearizeCtx) -> PatternTree {
        PatternTree::TupleStruct(self)
        /*
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

impl midend::treewalk::Treewalk<PatternTree> for PatternTree {
    fn collect_symbols(&self, ctx: &mut midend::treewalk::CollectCtx) {
        match self {
            Self::Literal(expr) => expr.collect_symbols(ctx),
            Self::Identifier(ident) => {
                ctx.declare_variable(ident.value.clone()).unwrap();
            }
            Self::TupleStruct(t) => {
                for pattern in &t.subpatterns {
                    pattern.collect_symbols(ctx);
                }
            }
        }
    }

    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> PatternTree {
        match self.clone() {
            Self::Literal(_) => (),
            Self::Identifier(ident) => {
                let variable_name = ident.linearize(ctx);
                let variable_def_path = ctx
                    .define(midend::symtab::values::Variable::new(variable_name, None))
                    .unwrap();
                // TODO: examine if there's a better way to just declare variables and give them a
                // ValueID in one go?
                ctx.function_mut()
                    .values_mut()
                    .id_for_path(variable_def_path);
            }
            Self::TupleStruct(tuple_struct) => {
                tuple_struct.linearize(ctx);
            }
        };
        self
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

impl midend::treewalk::Treewalk<(PatternTree, midend::ir::ValueId)> for MatchArmTree {
    fn collect_symbols(&self, ctx: &mut midend::treewalk::CollectCtx) {
        let arm_subscope_idx = ctx.new_subscope().unwrap();
        self.pattern.collect_symbols(ctx);
        self.expression.collect_symbols(ctx);
        ctx.finish_subscope(arm_subscope_idx).unwrap();
    }

    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(
        self,
        context: &mut midend::treewalk::LinearizeCtx,
    ) -> (PatternTree, midend::ir::ValueId) {
        let pattern = self.pattern.linearize(context);
        let arm_value = self.expression.linearize(context);
        (pattern, arm_value)
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

impl midend::treewalk::Treewalk<midend::ir::ValueId> for MatchExpressionTree {
    fn collect_symbols(&self, ctx: &mut midend::treewalk::CollectCtx) {
        self.scrutinee_expression.collect_symbols(ctx);
        for arm in &self.arms {
            arm.collect_symbols(ctx);
        }
    }

    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> midend::ir::ValueId {
        let match_loc = self.loc();

        let parent_scope_def_path = ctx.def_path().clone();
        let switch_scope_def_path = ctx.reserve_subscope();

        ctx.function_mut()
            .create_switch(
                match_loc.clone().start(),
                parent_scope_def_path,
                switch_scope_def_path,
            )
            .unwrap();

        let scrutinee_value = self.scrutinee_expression.linearize(ctx);

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
            let (pattern, result_value) = arm.linearize(ctx);
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
        result_value
    }
}

impl Display for MatchExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "match {} {{{:?}}}", self.scrutinee_expression, self.arms)
    }
}
