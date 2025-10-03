use crate::midend::ir::unlowered::*;

struct MatchArmContext<'a> {
    pub ctx: &'a mut linearizer::WalkContext,
    pub scrutinee: ValueId,
}

struct MatchArmResult<'a> {
    pub arm_label: usize, // the label of the arm to which we should jump if matched
    pub result_value: ValueId,
    pub comparison_value: ValueId,
    pub ctx: &'a mut linearizer::WalkContext,
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct MatchArm {
    pub pattern: frontend::ast::expressions::match_expression::PatternTree,
    pub arm_label: usize,
    pub result_value: ValueId,
}

fn lower_pattern<'a>(
    pattern: frontend::ast::expressions::match_expression::Pattern,
    arm_ctx: &mut MatchArmContext<'a>,
) -> LoweredPattern {
    use frontend::ast::expressions::match_expression::Pattern;
    match pattern {
        Pattern::Literal(expr) => {
            let value = expr.walk(arm_ctx.ctx);
            LoweredPattern::Constructor(
                PatternConstructor::Constant(
                    123, /*arm_ctx
                        .ctx
                        .function()
                        .values()
                        .value_for_constant(value)
                        .unwrap(),*/
                ),
                Vec::new(),
            )
        }
        Pattern::Identifier(name) => LoweredPattern::Identifier(name),
        Pattern::TupleStruct(name, subpatterns) => {
            let scrutinee_type = arm_ctx
                .ctx
                .function_mut()
                .values()
                .semantic_for_id(&arm_ctx.scrutinee)
                .expect("Scrutinee type not known!");
            let scrutinee_variable_def_path = match arm_ctx.ctx.function_mut().values().def_path_for_id(&arm_ctx.scrutinee) {
        Ok(opt) => opt.cloned(),
        Err(e)=> None 
            };

            let scrutinee_type_def = arm_ctx
                .ctx
                .symtab()
                .types
                .get_definition(&scrutinee_type)
                .unwrap();

            match &scrutinee_type_def.repr {
                symtab::TypeRepr::Enum(_e) => LoweredPattern::Constructor(
                    PatternConstructor::EnumVariant {
                        ty_: scrutinee_type,
                        variant: name,
                    },
                    subpatterns
                        .into_iter()
                        .map(|p| lower_pattern(p.pattern, arm_ctx))
                        .collect(),
                ),
                other => panic!(
                    "Match for type repr {:?} unsupported (matching tuple struct {} from scrutinee value (defpath {:?}))",
                    other, name, scrutinee_variable_def_path 
                ),
            }
        }
    }
}

#[derive(Debug)]
pub enum PatternConstructor {
    EnumVariant {
        ty_: types::Semantic,
        variant: String,
    },
    Struct {
        ty_: types::Semantic,
    },
    Constant(usize),
}

#[derive(Debug)]
pub enum LoweredPattern {
    Constructor(PatternConstructor, Vec<LoweredPattern>), // fields = subpatterns
    Identifier(String),
    Wildcard,
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct MatchOperands {
    pub scrutinee: ValueId,
    pub arms: Vec<MatchArm>,
}

impl Lowerable for MatchOperands {
    fn lower(self, ctx: &mut linearizer::WalkContext, loc: SourceLoc) {
        // TODO: implement actual match decision tree logic

        let matched_type = ctx
            .function_mut()
            .values()
            .semantic_for_id(&self.scrutinee)
            .unwrap();
        let matched_type_definition = ctx.symtab().types.get_definition(&matched_type).unwrap();

        ctx.function().values().diag(ctx.symtab());
        let mut match_arm_ctx = MatchArmContext {
            ctx,
            scrutinee: self.scrutinee,
        };

        let walked_patterns: Vec<LoweredPattern> = self
            .arms
            .into_iter()
            .map(|arm| lower_pattern(arm.pattern.pattern, &mut match_arm_ctx))
            .collect();

        ctx.function().values().diag(ctx.symtab());
        println!("{:#?}", walked_patterns);



        return;
    }
}

impl OperandTypeInference for MatchOperands {
    fn infer_types(&mut self, _ctx: &TypeInferenceContext) -> bool {
        unimplemented!();
    }
}
