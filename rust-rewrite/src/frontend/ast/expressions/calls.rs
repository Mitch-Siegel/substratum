use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct CallParamsTree {
    pub open_paren_loc: sourceloc::SourceSpan,
    pub params: Vec<Expression>,
    pub close_paren_loc: sourceloc::SourceSpan,
}

impl Ast for CallParamsTree {
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
            if !params.is_empty() {
                params += ", ";
            }
            params += &format!("{}", p);
        }
        write!(f, "{}", params)
    }
}

impl
    treewalk::Linearize<
        treewalk::UnpathedFunctionLinearizeCtx,
        symtab::ValuePath,
        treewalk::ValueFunctionLinearizeCtx,
    > for CallParamsTree
{
    type Data = Vec<midend::ir::ValueId>;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        mut ctx: treewalk::ValueFunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, midend::treewalk::UnpathedFunctionLinearizeCtx> {
        let mut param_values = Vec::new();

        for param in self.params {
            let param_id;
            (param_id, ctx) = param.linearize(ctx)?;
            param_values.push(param_id);
        }

        ctx.into_result(param_values)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct CallExpressionTree {
    pub function_operand: Expression,
    pub params: CallParamsTree,
}

impl Ast for CallExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.function_operand
            .loc()
            .merge(&self.params.loc())
            .unwrap()
    }
}

impl
    treewalk::Linearize<
        treewalk::UnpathedFunctionLinearizeCtx,
        symtab::ValuePath,
        treewalk::ValueFunctionLinearizeCtx,
    > for CallExpressionTree
{
    type Data = midend::ir::ValueId;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        mut ctx: treewalk::ValueFunctionLinearizeCtx,
    ) -> treewalk::LinearizeResult<Self::Data, treewalk::UnpathedFunctionLinearizeCtx> {
        let call_start = self.loc().start();

        let function_operand;
        (function_operand, ctx) = self.function_operand.linearize(ctx)?;

        let return_value_to = ctx.function_mut().values_mut().next_temp();

        // //TODO: error handling and checking
        // assert!(called_method.arguments.len() == params.len());

        let (params, mut ctx) = self.params.linearize(ctx)?;

        let method_call_line =
            midend::ir::IrLine::new_call(call_start, function_operand, params, return_value_to);

        ctx.function_mut()
            .append_statement_to_current_block(method_call_line)
            .unwrap();

        ctx.into_result(return_value_to)
    }
}

impl Display for CallExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}({})", self.function_operand, self.params)
    }
}
