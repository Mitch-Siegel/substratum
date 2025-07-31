use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct CallParamsTree {
    pub loc: SourceLocWithMod,
    pub params: Vec<ExpressionTree>,
}

impl CallParamsTree {
    pub fn new(loc: SourceLocWithMod, params: Vec<ExpressionTree>) -> Self {
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

impl<'a> ReturnFunctionWalk<'a, Vec<midend::ir::ValueId>> for CallParamsTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> Vec<midend::ir::ValueId> {
        let mut param_values = Vec::new();

        for param in self.params {
            param_values.push(param.walk(context));
        }

        param_values
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct MethodCallExpressionTree {
    pub loc: SourceLocWithMod,
    pub receiver: ExpressionTree,
    pub called_method: String,
    pub params: CallParamsTree,
}
impl MethodCallExpressionTree {
    pub fn new(
        loc: SourceLocWithMod,
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

impl ValueWalk for MethodCallExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> midend::ir::ValueId {
        let receiver = self.receiver.walk(context);

        let receiver_type = context.type_for_value_id(&receiver).unwrap();
        let called_method = context
            .lookup_implemented_function(&receiver_type, &self.called_method)
            .unwrap();
        let return_type = called_method.prototype.return_type.clone();
        let method_name = String::from(called_method.name());

        let return_value_to = if return_type != midend::types::Type::Unit {
            Some(context.next_temp(Some(return_type)))
        } else {
            None
        };

        // //TODO: error handling and checking
        // assert!(called_method.arguments.len() == params.len());

        let params: Vec<midend::ir::ValueId> = self
            .params
            .walk(context)
            .into_iter()
            .map(|value| value.into())
            .collect();

        let method_call_line = midend::ir::IrLine::new_method_call(
            self.loc,
            receiver.into(),
            &method_name,
            params,
            return_value_to.clone(),
        );

        context
            .append_statement_to_current_block(method_call_line)
            .unwrap();

        match return_value_to {
            Some(value_id) => value_id,
            None => context.unit_value_id(),
        }
    }
}
