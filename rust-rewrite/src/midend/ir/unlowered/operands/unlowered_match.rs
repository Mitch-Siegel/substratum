use crate::midend::{ir::unlowered::*, treewalk::Linearize};

struct _MatchArmContext {
    pub ctx: treewalk::LinearizeCtx,
    pub scrutinee: ValueId,
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct MatchArm {
    pub pattern: frontend::ast::expressions::match_expression::PatternTree,
    pub arm_label: usize,
    pub result_value: ValueId,
}

fn _lower_pattern(
    pattern: frontend::ast::expressions::match_expression::PatternTree,
    mut arm_ctx: _MatchArmContext,
) -> Result<(LoweredPattern, treewalk::UnpathedLinearizeCtx), treewalk::LinearizeError> {
    use frontend::ast::expressions::match_expression::PatternTree;
    let lowered_pattern;
    let ctx;
    (lowered_pattern, ctx) = match pattern {
        PatternTree::Literal(expr) => {
            let (_expr_value, ctx) = expr.linearize(arm_ctx.ctx)?;
            let pattern = LoweredPattern::_Constructor(
                PatternConstructor::_Constant(
                    123, /*arm_ctx
                        .ctx
                        .function()
                        .values()
                        .value_for_constant(value)
                        .unwrap(),*/
                ),
                Vec::new(),
            );
            (pattern, ctx)
        }
        PatternTree::Identifier(name) => {
            let (name, ctx) = name.linearize(arm_ctx.ctx)?;
            (LoweredPattern::_Identifier(name), ctx)
        }
        PatternTree::TupleStruct(tuple_struct) => {
            let _scrutinee_type = arm_ctx
                .ctx
                .function_mut()
                .values()
                .semantic_for_id(&arm_ctx.scrutinee)
                .expect("Scrutinee type not known!");
            let _scrutinee_variable_def_path = match arm_ctx
                .ctx
                .function_mut()
                .values()
                .def_path_for_id(&arm_ctx.scrutinee)
            {
                Ok(opt) => opt.cloned(),
                Err(_e) => None,
            };

            let _variant = tuple_struct.name.linearize(arm_ctx.ctx);

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
    };

    Ok((lowered_pattern, ctx))
}

#[allow(unused)]
#[derive(Debug)]
pub enum PatternConstructor {
    _EnumVariant {
        ty: types::Semantic,
        variant: String,
    },
    /*Struct {
        ty: types::Semantic,
    },*/
    _Constant(usize),
}

impl std::fmt::Display for PatternConstructor {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::_EnumVariant { ty, variant } => write!(f, "enum variant {}::{}", ty, variant),
            //Self::Struct{ty} => write!(f, "struct {}", ty),
            Self::_Constant(constant) => write!(f, "constant {}", constant),
        }
    }
}

#[allow(unused)]
#[derive(Debug)]
pub enum LoweredPattern {
    _Constructor(PatternConstructor, Vec<LoweredPattern>), // fields = subpatterns
    _Identifier(String),
    //Wildcard,
}

impl std::fmt::Display for LoweredPattern {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::_Constructor(pat, subpats) => {
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
            Self::_Identifier(ident) => write!(f, "{}", ident),
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
    fn lower(self, _ctx: &mut treewalk::LinearizeCtx, _loc: SourceLoc) {
        // TODO: implement actual match decision tree logic
        unimplemented!();

        /*
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
        */
    }
}

impl OperandTypeInference for MatchOperands {
    fn infer_types(&mut self, _ctx: &TypeInferenceContext) -> bool {
        unimplemented!();
    }
}
