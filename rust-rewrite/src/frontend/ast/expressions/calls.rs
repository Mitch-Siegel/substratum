use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct CallParamsTree {
    pub open_paren_loc: sourceloc::SourceSpan,
    pub params: Vec<Expression>,
    pub close_paren_loc: sourceloc::SourceSpan,
}

impl Ast<Vec<midend::ir::ValueId>> for CallParamsTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.open_paren_loc
            .clone()
            .merge(&self.close_paren_loc)
            .unwrap()
    }
}

impl Display for CallParamsTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut params = String::new();
        for p in &self.params {
            if params.len() > 0 {
                params += &", ";
            }
            params += &format!("{}", p);
        }
        write!(f, "{}", params)
    }
}

impl midend::treewalk::Treewalk<Vec<midend::ir::ValueId>> for CallParamsTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> Vec<midend::ir::ValueId> {
        let mut param_values = Vec::new();

        for param in self.params {
            param_values.push(param.linearize(ctx));
        }

        param_values
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct CallExpressionTree {
    pub function_operand: Expression,
    pub params: CallParamsTree,
}

impl Ast<midend::ir::ValueId> for CallExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.function_operand
            .loc()
            .merge(&self.params.loc())
            .unwrap()
    }
}

impl midend::treewalk::Treewalk<midend::ir::ValueId> for CallExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> midend::ir::ValueId {
        let call_start = self.loc().start();

        let function_operand = self.function_operand.linearize(ctx);

        let return_value_to = ctx.function_mut().values_mut().next_temp();

        // //TODO: error handling and checking
        // assert!(called_method.arguments.len() == params.len());

        let params: Vec<midend::ir::ValueId> = self
            .params
            .linearize(ctx)
            .into_iter()
            .map(|value| value.into())
            .collect();

        let method_call_line = midend::ir::IrLine::new_call(
            call_start,
            function_operand.into(),
            params,
            return_value_to.clone(),
        );

        ctx.function_mut()
            .append_statement_to_current_block(method_call_line)
            .unwrap();

        return_value_to
    }
}

impl Display for CallExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}({})", self.function_operand, self.params)
    }
}
