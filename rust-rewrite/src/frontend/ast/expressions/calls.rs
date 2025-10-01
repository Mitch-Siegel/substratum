use crate::{frontend::ast::*, midend::linearizer::Walk};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct CallParamsTree {
    pub loc: SourceLoc,
    pub params: Vec<ExpressionTree>,
}

impl CallParamsTree {
    pub fn new(loc: SourceLoc, params: Vec<ExpressionTree>) -> Self {
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

impl Walk<Vec<midend::ir::ValueId>> for CallParamsTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, ctx: &mut midend::linearizer::WalkContext) -> Vec<midend::ir::ValueId> {
        let mut param_values = Vec::new();

        for param in self.params {
            param_values.push(param.walk(ctx));
        }

        param_values
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct MethodCallExpressionTree {
    pub loc: SourceLoc,
    pub receiver: ExpressionTree,
    pub called_method: String,
    pub params: CallParamsTree,
}
impl MethodCallExpressionTree {
    pub fn new(
        loc: SourceLoc,
        receiver: ExpressionTree,
        called_method: String,
        params: CallParamsTree,
    ) -> Self {
        Self {
            loc,
            receiver,
            called_method,
            params,
        }
    }
}
impl Display for MethodCallExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}.{}({})",
            self.receiver, self.called_method, self.params
        )
    }
}

impl Walk<midend::ir::ValueId> for MethodCallExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, ctx: &mut midend::linearizer::WalkContext) -> midend::ir::ValueId {
        let receiver = self.receiver.walk(ctx);

        let return_value_to = ctx.function_mut().values_mut().next_temp();

        // //TODO: error handling and checking
        // assert!(called_method.arguments.len() == params.len());

        let params: Vec<midend::ir::ValueId> = self
            .params
            .walk(ctx)
            .into_iter()
            .map(|value| value.into())
            .collect();

        let method_call_line = midend::ir::IrLine::new_method_call(
            self.loc,
            receiver.into(),
            self.called_method,
            params,
            return_value_to.clone(),
        );

        ctx.function_mut()
            .append_statement_to_current_block(method_call_line)
            .unwrap();

        return_value_to
    }
}
