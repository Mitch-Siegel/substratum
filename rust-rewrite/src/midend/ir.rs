use std::collections::HashSet;

enum IROperand {
    Variable(String),
    Temporary(String),
    UnsignedDecimalConstant(usize),
}

pub enum BinaryOperations {
    Add,
    Subtract,
    Multiply,
    Divide,
}

pub struct DualSourceOperands {
    pub a: IROperand,
    pub b: IROperand,
}

pub struct BinaryArithmeticOperands {
    pub destination: IROperand,
    pub sources: DualSourceOperands,
}

pub enum JumpCondition {
    Unconditional,
    Eq(DualSourceOperands),
    NE(DualSourceOperands),
    G(DualSourceOperands),
    L(DualSourceOperands),
    GE(DualSourceOperands),
    LE(DualSourceOperands),
}

pub struct JumpOperands {
    pub destination_block: usize,
    pub condition: JumpCondition,
}

enum IR {
    BinaryArithmetic(BinaryArithmeticOperands),
    Jump(JumpOperands),
}

impl IR {
    pub fn new_three_op_arith(
        destination: IROperand,
        source_a: IROperand,
        source_b: IROperand,
    ) -> Self {
        IR::BinaryArithmetic(BinaryArithmeticOperands {
            destination,
            sources: DualSourceOperands{a: source_a, b: source_b}
        })
    }
}

struct BasicBlock {
    statements: Vec<IR>,
    targets: HashSet<usize>,
}

impl BasicBlock {
    pub fn new() -> Self {
        BasicBlock{statements: Vec::new(), targets: HashSet::new()}
    }

    pub fn append_statement(&mut self, statement: IR) {
        match &statement {
            IR::Jump(operands) => {
                self.targets.insert(operands.destination_block);
            }
            _ => {}
        }
        
        self.statements.push(statement);
    }
}
