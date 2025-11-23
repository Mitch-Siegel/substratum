use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct CallParamsTree {
    pub loc: SourceLoc,
    pub params: Vec<Expression>,
}

impl CallParamsTree {
    pub fn new(loc: SourceLoc, params: Vec<Expression>) -> Self {
        Self { loc, params }
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

impl treewalk::Linearize<Vec<midend::ir::ValueId>> for CallParamsTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> Vec<midend::ir::ValueId> {
        let mut param_values = Vec::new();

        for param in self.params {
            param_values.push(param.linearize(ctx));
        }

        param_values
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct CallExpressionTree {
    pub loc: SourceLoc,
    pub function_operand: Expression,
    pub params: CallParamsTree,
}

impl CallExpressionTree {
    pub fn new(loc: SourceLoc, function_operand: Expression, params: CallParamsTree) -> Self {
        Self {
            loc,
            function_operand,
            params,
        }
    }
}

impl Display for CallExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}({})", self.function_operand, self.params)
    }
}

impl treewalk::Linearize<midend::ir::ValueId> for CallExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> midend::ir::ValueId {
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
            self.loc,
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
