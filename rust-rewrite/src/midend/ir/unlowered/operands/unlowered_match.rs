use crate::midend::{
    ir::unlowered::*,
    linearizer::{CustomReturnWalk, ValueWalk},
    *,
};

struct MatchArmContext<'a> {
    pub ctx: &'a mut linearizer::FunctionWalkContext,
    pub scrutinee: ValueId,
}

struct MatchArmResult<'a> {
    pub arm_label: usize, // the label of the arm to which we should jump if matched
    pub result_value: ValueId,
    pub comparison_value: ValueId,
    pub ctx: &'a mut linearizer::FunctionWalkContext,
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct MatchArm {
    pub pattern: frontend::ast::expressions::match_expression::PatternTree,
    pub arm_label: usize,
    pub result_value: ValueId,
}

fn match_enum_destructure(
    ctx: &mut linearizer::FunctionWalkContext,
    scrutinee: ValueId,
    destructured_enum: &symtab::type_definition::EnumRepr,
    variant_name: String,
    nested_patterns: Vec<frontend::ast::expressions::match_expression::PatternTree>,
) {
    let variant = destructured_enum
        .get_variant(&variant_name)
        .expect(&format!(
            "Enum {} has no variant {}",
            destructured_enum.name, variant_name
        ));

    match variant.data() {
        symtab::EnumVariantRepr::Unit => panic!(
            "{}::{} has no variant data",
            destructured_enum.name, variant_name
        ),
        symtab::EnumVariantRepr::Tuple(members) => {
            if members.len() != nested_patterns.len() {
                panic!(
                    "{}::{} has {} tuple members, but saw {} in match",
                    destructured_enum.name,
                    variant_name,
                    members.len(),
                    nested_patterns.len()
                );
            }

            let mut tuple_byte: usize = 0;

            for (pattern_tree, member) in nested_patterns.into_iter().zip(members.iter()) {
                let pattern_tree = pattern_tree.walk(ctx);
                let tuple_member_value = ctx.values_mut().next_temp();

                let (member_type, type_def_path) = ctx
                    .lookup_with_path::<symtab::TypeDefinition>(member)
                    .unwrap();

                //tuple_byte += ctx.symtab().types.

                //let tuple_member = ir::IrLine::new_compute_field_address(pattern_tree.loc, tuple_member_value
                //let truth_value = match_pattern_to_truth_value(pattern_tree.pattern,
            }
        }
    }
}

fn match_pattern_to_truth_value(
    pattern: frontend::ast::expressions::match_expression::Pattern,
    scrutinee: ValueId,
    ctx: &mut linearizer::FunctionWalkContext,
) -> ValueId {
    use frontend::ast::expressions::match_expression::Pattern;
    match pattern {
        Pattern::Literal(literal) => literal.walk(ctx),
        Pattern::Identifier(name) => {
            let (_, def_path) = ctx.lookup_with_path::<symtab::Variable>(&name).unwrap();
            ctx.values_mut().id_for_variable(def_path)
        }
        Pattern::TupleStruct(name, nested_patterns) => {
            let scrutinee_value = ctx.values().value_for_id(&scrutinee).unwrap();
            let scrutinee_type = ctx
                .symtab()
                .types
                .get_definition(&scrutinee_value.ty().unwrap())
                .unwrap();

            match scrutinee_type.repr.clone() {
                symtab::type_definition::TypeRepr::Enum(destructured_enum) => {
                    match_enum_destructure(
                        ctx,
                        scrutinee,
                        &destructured_enum,
                        name,
                        nested_patterns,
                    )
                }
                other => panic!("Invalid type {} in tuple struct match", other.name()),
            }

            unimplemented!("tuple struct pattern match not implemented yet");
        }
    }
}

impl<'a> linearizer::CustomReturnWalk<MatchArmContext<'a>, MatchArmResult<'a>> for MatchArm {
    fn walk(self, arm_context: MatchArmContext<'a>) -> MatchArmResult<'a> {
        let MatchArmContext::<'a> { ctx, scrutinee } = arm_context;

        let (loc, truth_value) = (
            self.pattern.loc,
            match_pattern_to_truth_value(self.pattern.pattern, scrutinee, ctx),
        );

        let comparison_jump = ir::IrLine::new_jump(
            loc,
            self.arm_label,
            lowered::JumpCondition::Conditional(
                ir::lowered::operands::BinaryComparisonOperands::new(
                    scrutinee,
                    truth_value,
                    lowered::operands::BinaryComparisonKind::EQ,
                ),
            ),
        );
        ctx.append_jump_to_current_block(comparison_jump).unwrap();

        MatchArmResult {
            arm_label: self.arm_label,
            result_value: self.result_value,
            comparison_value: truth_value,
            ctx,
        }
    }
}

fn match_enum(
    mut context: &mut linearizer::FunctionWalkContext,
    match_loc: SourceLoc,
    match_operands: MatchOperands,
) {
    // TODO: architecture based discriminant size based on word size?
    let discriminant_type = context
        .semantic_type_for_syntactic(&types::Syntactic::U64)
        .unwrap();
    let discriminant = context.values_mut().next_temp_with_type(discriminant_type);

    let discriminant_line = ir::IrLine::new_discriminant(
        match_loc,
        context.def_path(),
        discriminant,
        match_operands.scrutinee,
    );

    for arm in match_operands.arms {
        let result = arm.walk(MatchArmContext {
            ctx: context,
            scrutinee: match_operands.scrutinee,
        });

        context = result.ctx;
    }
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct MatchOperands {
    pub scrutinee: ValueId,
    pub arms: Vec<MatchArm>,
}

impl Lowerable for MatchOperands {
    fn lower<'a>(self, context: &'a mut linearizer::FunctionWalkContext, loc: SourceLoc) {
        let matched_type = context.values().semantic_for_id(&self.scrutinee).unwrap();
        let matched_type_definition = context
            .symtab()
            .types
            .get_definition(&matched_type)
            .unwrap();

        match matched_type_definition.repr {
            symtab::TypeRepr::Enum(_) => match_enum(context, loc, self),
            _ => panic!(
                "Type {:?} not supported for matching",
                matched_type_definition
            ),
        }
    }
}

impl OperandTypePropagation for MatchOperands {
    fn propagate_types(&self, ctx: &TypePropagationContext) -> bool {
        true
    }
}
