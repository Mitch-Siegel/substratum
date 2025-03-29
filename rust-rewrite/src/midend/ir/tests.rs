use crate::{
    frontend::sourceloc::SourceLoc,
    midend::ir::{BinaryOperations, JumpCondition, OperandName, Operations},
};

use super::{DualSourceOperands, IrLine, JumpOperation, Operand, SourceDestOperands};

#[test]
fn ir_line_new_assignment() {
    let assignment = IrLine::new_assignment(
        SourceLoc::new(123, 456),
        Operand::new_as_variable("a".into()),
        Operand::new_as_unsigned_decimal_constant(99),
    );

    assert_eq!(assignment.loc, SourceLoc::new(123, 456));
    assert!(matches!(assignment.operation, Operations::Assignment(_)));
}

#[test]
fn ir_line_new_binary_op() {
    let binary_operation = IrLine::new_binary_op(
        SourceLoc::new(123, 456),
        BinaryOperations::new_add(
            Operand::new_as_variable("result".into()),
            Operand::new_as_variable("a".into()),
            Operand::new_as_variable("b".into()),
        ),
    );

    assert_eq!(binary_operation.loc, SourceLoc::new(123, 456));
    assert!(matches!(
        binary_operation.operation,
        Operations::BinaryOperation(_)
    ));
}

#[test]
fn ir_line_new_jump() {
    let jump = IrLine::new_jump(SourceLoc::new(123, 456), 8, JumpCondition::Unconditional);

    assert_eq!(jump.loc, SourceLoc::new(123, 456));
    assert!(matches!(jump.operation, Operations::Jump(_)));
}

fn operand_from_string(name: &str) -> Operand {
    Operand::new_as_variable(name.into())
}

fn line_from_op(operation: Operations) -> IrLine {
    IrLine::new(SourceLoc::new(0, 0), operation)
}

fn operand_name_from_string(name: &str) -> OperandName {
    OperandName::new_basic(name.into())
}

#[test]
fn read_operand_names() {
    // assignment op
    let mut op = line_from_op(Operations::Assignment(SourceDestOperands {
        destination: operand_from_string("assignment_destination"),
        source: operand_from_string("assignment_source"),
    }));
    assert_eq!(
        op.write_operand_names(),
        vec![&operand_name_from_string("assignment_destination")]
    );
    assert_eq!(
        op.read_operand_names(),
        vec![&operand_name_from_string("assignment_source")]
    );

    // binary operation
    op = line_from_op(Operations::BinaryOperation(BinaryOperations::new_subtract(
        operand_from_string("binary_op_destination"),
        operand_from_string("binary_op_source_a"),
        operand_from_string("binary_op_source_b"),
    )));
    assert_eq!(
        op.write_operand_names(),
        vec![&operand_name_from_string("binary_op_destination")]
    );
    assert_eq!(
        op.read_operand_names(),
        vec![
            &operand_name_from_string("binary_op_source_a"),
            &operand_name_from_string("binary_op_source_b")
        ]
    );

    // jeq
    op = line_from_op(Operations::Jump(JumpOperation::new(
        1,
        JumpCondition::Eq(DualSourceOperands::new(
            operand_from_string("eq_a"),
            operand_from_string("eq_b"),
        )),
    )));
    assert_eq!(op.write_operand_names(), Vec::<&OperandName>::new());
    assert_eq!(
        op.read_operand_names(),
        vec![
            &operand_name_from_string("eq_a"),
            &operand_name_from_string("eq_b")
        ]
    );

    // jne
    op = line_from_op(Operations::Jump(JumpOperation::new(
        1,
        JumpCondition::NE(DualSourceOperands::new(
            operand_from_string("ne_a"),
            operand_from_string("ne_b"),
        )),
    )));
    assert_eq!(op.write_operand_names(), Vec::<&OperandName>::new());
    assert_eq!(
        op.read_operand_names(),
        vec![
            &operand_name_from_string("ne_a"),
            &operand_name_from_string("ne_b")
        ]
    );

    // jg
    op = line_from_op(Operations::Jump(JumpOperation::new(
        1,
        JumpCondition::GT(DualSourceOperands::new(
            operand_from_string("gt_a"),
            operand_from_string("gt_b"),
        )),
    )));
    assert_eq!(op.write_operand_names(), Vec::<&OperandName>::new());
    assert_eq!(
        op.read_operand_names(),
        vec![
            &operand_name_from_string("gt_a"),
            &operand_name_from_string("gt_b")
        ]
    );

    // jl
    op = line_from_op(Operations::Jump(JumpOperation::new(
        1,
        JumpCondition::LT(DualSourceOperands::new(
            operand_from_string("lt_a"),
            operand_from_string("lt_b"),
        )),
    )));
    assert_eq!(op.write_operand_names(), Vec::<&OperandName>::new());
    assert_eq!(
        op.read_operand_names(),
        vec![
            &operand_name_from_string("lt_a"),
            &operand_name_from_string("lt_b")
        ]
    );

    // ge
    op = line_from_op(Operations::Jump(JumpOperation::new(
        1,
        JumpCondition::GE(DualSourceOperands::new(
            operand_from_string("ge_a"),
            operand_from_string("ge_b"),
        )),
    )));
    assert_eq!(op.write_operand_names(), Vec::<&OperandName>::new());
    assert_eq!(
        op.read_operand_names(),
        vec![
            &operand_name_from_string("ge_a"),
            &operand_name_from_string("ge_b")
        ]
    );

    // le
    op = line_from_op(Operations::Jump(JumpOperation::new(
        1,
        JumpCondition::LE(DualSourceOperands::new(
            operand_from_string("le_a"),
            operand_from_string("le_b"),
        )),
    )));
    assert_eq!(op.write_operand_names(), Vec::<&OperandName>::new());
    assert_eq!(
        op.read_operand_names(),
        vec![
            &operand_name_from_string("le_a"),
            &operand_name_from_string("le_b")
        ]
    );
}

#[test]
fn read_operand_names_mut() {
    // assignment op
    let mut op = line_from_op(Operations::Assignment(SourceDestOperands {
        destination: operand_from_string("assignment_destination"),
        source: operand_from_string("assignment_source"),
    }));
    assert_eq!(
        op.write_operand_names_mut(),
        vec![&operand_name_from_string("assignment_destination")]
    );
    assert_eq!(
        op.read_operand_names_mut(),
        vec![&operand_name_from_string("assignment_source")]
    );

    // binary operation
    op = line_from_op(Operations::BinaryOperation(BinaryOperations::new_subtract(
        operand_from_string("binary_op_destination"),
        operand_from_string("binary_op_source_a"),
        operand_from_string("binary_op_source_b"),
    )));
    assert_eq!(
        op.write_operand_names_mut(),
        vec![&operand_name_from_string("binary_op_destination")]
    );
    assert_eq!(
        op.read_operand_names_mut(),
        vec![
            &operand_name_from_string("binary_op_source_a"),
            &operand_name_from_string("binary_op_source_b")
        ]
    );

    // jeq
    op = line_from_op(Operations::Jump(JumpOperation::new(
        1,
        JumpCondition::Eq(DualSourceOperands::new(
            operand_from_string("eq_a"),
            operand_from_string("eq_b"),
        )),
    )));
    assert_eq!(op.write_operand_names_mut(), Vec::<&OperandName>::new());
    assert_eq!(
        op.read_operand_names_mut(),
        vec![
            &operand_name_from_string("eq_a"),
            &operand_name_from_string("eq_b")
        ]
    );

    // jne
    op = line_from_op(Operations::Jump(JumpOperation::new(
        1,
        JumpCondition::NE(DualSourceOperands::new(
            operand_from_string("ne_a"),
            operand_from_string("ne_b"),
        )),
    )));
    assert_eq!(op.write_operand_names_mut(), Vec::<&OperandName>::new());
    assert_eq!(
        op.read_operand_names_mut(),
        vec![
            &operand_name_from_string("ne_a"),
            &operand_name_from_string("ne_b")
        ]
    );

    // jg
    op = line_from_op(Operations::Jump(JumpOperation::new(
        1,
        JumpCondition::GT(DualSourceOperands::new(
            operand_from_string("gt_a"),
            operand_from_string("gt_b"),
        )),
    )));
    assert_eq!(op.write_operand_names_mut(), Vec::<&OperandName>::new());
    assert_eq!(
        op.read_operand_names_mut(),
        vec![
            &operand_name_from_string("gt_a"),
            &operand_name_from_string("gt_b")
        ]
    );

    // jl
    op = line_from_op(Operations::Jump(JumpOperation::new(
        1,
        JumpCondition::LT(DualSourceOperands::new(
            operand_from_string("lt_a"),
            operand_from_string("lt_b"),
        )),
    )));
    assert_eq!(op.write_operand_names_mut(), Vec::<&OperandName>::new());
    assert_eq!(
        op.read_operand_names_mut(),
        vec![
            &operand_name_from_string("lt_a"),
            &operand_name_from_string("lt_b")
        ]
    );

    // ge
    op = line_from_op(Operations::Jump(JumpOperation::new(
        1,
        JumpCondition::GE(DualSourceOperands::new(
            operand_from_string("ge_a"),
            operand_from_string("ge_b"),
        )),
    )));
    assert_eq!(op.write_operand_names_mut(), Vec::<&OperandName>::new());
    assert_eq!(
        op.read_operand_names_mut(),
        vec![
            &operand_name_from_string("ge_a"),
            &operand_name_from_string("ge_b")
        ]
    );

    // le
    op = line_from_op(Operations::Jump(JumpOperation::new(
        1,
        JumpCondition::LE(DualSourceOperands::new(
            operand_from_string("le_a"),
            operand_from_string("le_b"),
        )),
    )));
    assert_eq!(op.write_operand_names_mut(), Vec::<&OperandName>::new());
    assert_eq!(
        op.read_operand_names_mut(),
        vec![
            &operand_name_from_string("le_a"),
            &operand_name_from_string("le_b")
        ]
    );
}
