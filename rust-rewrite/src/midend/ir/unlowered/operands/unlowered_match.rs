use crate::midend::{ir::unlowered::*, treewalk::Treewalk};

struct MatchArmContext<'a> {
    pub ctx: &'a mut treewalk::LinearizeCtx,
    pub scrutinee: ValueId,
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct MatchArm {
    pub pattern: frontend::ast::expressions::match_expression::PatternTree,
    pub arm_label: usize,
    pub result_value: ValueId,
}

fn lower_pattern<'a>(
    pattern: frontend::ast::expressions::match_expression::PatternTree,
    arm_ctx: &mut MatchArmContext<'a>,
) -> LoweredPattern {
    use frontend::ast::expressions::match_expression::PatternTree;
    match pattern {
        PatternTree::Literal(expr) => {
            expr.linearize(arm_ctx.ctx);
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
        PatternTree::Identifier(name) => LoweredPattern::Identifier(name.linearize(arm_ctx.ctx)),
        PatternTree::TupleStruct(tuple_struct) => {
            let scrutinee_type = arm_ctx
                .ctx
                .function_mut()
                .values()
                .semantic_for_id(&arm_ctx.scrutinee)
                .expect("Scrutinee type not known!");
            let scrutinee_variable_def_path = match arm_ctx
                .ctx
                .function_mut()
                .values()
                .def_path_for_id(&arm_ctx.scrutinee)
            {
                Ok(opt) => opt.cloned(),
                Err(_e) => None,
            };

            let variant = tuple_struct.name.linearize(arm_ctx.ctx);

            unimplemented!();
            /*
            let subpatterns = tuple_struct
                .subpatterns
                .into_iter()
                .map(|subpattern| lower_pattern(subpattern, arm_ctx))
                .collect();

            let scrutinee_type_def = arm_ctx
                .ctx
                .symtab()
                .types
                .get_type_definition(&scrutinee_type)
                .unwrap();


            match &scrutinee_type_def.repr {
                symtab::SymbolDef::Type(symtab::Type::Enum(_e)) =>
                    LoweredPattern::Constructor(
                    PatternConstructor::EnumVariant {
                        ty: scrutinee_type,
                        variant
                    },
                    subpatterns
                ),
                other => panic!(
                    "Match for type repr {:?} unsupported (matching tuple struct {} from scrutinee value (defpath {:?}))",
                    other, variant, scrutinee_variable_def_path
                ),
            }
            */
        }
    }
}

#[derive(Debug)]
pub enum PatternConstructor {
    EnumVariant {
        ty: types::Semantic,
        variant: String,
    },
    /*Struct {
        ty: types::Semantic,
    },*/
    Constant(usize),
}

impl std::fmt::Display for PatternConstructor {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::EnumVariant { ty, variant } => write!(f, "enum variant {}::{}", ty, variant),
            //Self::Struct{ty} => write!(f, "struct {}", ty),
            Self::Constant(constant) => write!(f, "constant {}", constant),
        }
    }
}

#[derive(Debug)]
pub enum LoweredPattern {
    Constructor(PatternConstructor, Vec<LoweredPattern>), // fields = subpatterns
    Identifier(String),
    //Wildcard,
}

impl std::fmt::Display for LoweredPattern {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Constructor(pat, subpats) => {
                write!(f, "{} [", pat)?;
                let mut first = true;
                for subpat in subpats {
                    if !first {
                        write!(f, ", {}", subpat)?;
                    } else {
                        write!(f, "{}", subpat)?;
                        first = false;
                    }
                }
                write!(f, "]")
            }
            Self::Identifier(ident) => write!(f, "{}", ident),
            //Self::Wildcard => write!(f, "*"),
        }
    }
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct MatchOperands {
    pub scrutinee: ValueId,
    pub arms: Vec<MatchArm>,
}

impl Lowerable for MatchOperands {
    fn lower(self, ctx: &mut treewalk::LinearizeCtx, _loc: SourceLoc) {
        // TODO: implement actual match decision tree logic

        let matched_type = ctx
            .function_mut()
            .values()
            .semantic_for_id(&self.scrutinee)
            .unwrap();
        let _matched_type_definition = ctx
            .symtab()
            .types
            .get_type_definition(&matched_type)
            .unwrap();

        ctx.function().values().diag(ctx.symtab());
        let mut match_arm_ctx = MatchArmContext {
            ctx,
            scrutinee: self.scrutinee,
        };

        let walked_patterns: Vec<LoweredPattern> = self
            .arms
            .into_iter()
            .map(|arm| lower_pattern(arm.pattern, &mut match_arm_ctx))
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
