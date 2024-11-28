pub mod ir;
mod symtab;
mod types;

use crate::{ast::*, lexer::SourceLoc};
use ir::*;
pub use symtab::*;
use types::*;

struct WalkContext<'a> {
    control_flow: &'a mut ControlFlow,
    current_block: usize,
    control_convergence_block: usize,
}

impl WalkContext<'_> {
    pub fn new(control_flow: &mut ControlFlow) -> WalkContext {
        let current_block = control_flow.new_block();
        let control_convergence_block = control_flow.new_block();
        WalkContext {
            control_flow,
            current_block,
            control_convergence_block,
        }
    }

    pub fn append_ir(&mut self, statement: IR) {
        self.control_flow
            .append_to_block(self.current_block, statement);
    }

    pub fn converge_control(&mut self) {
        let end_block_jump = IR::new_jump(
            SourceLoc::none(),
            self.control_convergence_block,
            JumpCondition::Unconditional,
        );
        self.append_ir(end_block_jump);
        self.current_block = self.control_convergence_block;
        self.control_convergence_block = self.control_flow.new_block();
    }

    pub fn current_block_label(&self) -> usize {
        self.current_block
    }

    pub fn branch_from_current_with_identical_convergence(&mut self) -> WalkContext<'_> {
        let branched_block_label = self.control_flow.new_block();
        WalkContext {
            control_flow: self.control_flow,
            current_block: branched_block_label,
            control_convergence_block: self.control_convergence_block,
        }
    }
}

pub trait TableWalk {
    fn walk(self, symbol_table: &mut SymbolTable);
}

trait ScopeWalk {
    fn walk(&self, scope: &mut Scope);
}

impl TableWalk for TranslationUnitTree {
    fn walk(self, symbol_table: &mut SymbolTable) {
        match self.contents {
            TranslationUnit::FunctionDeclaration(tree) => {
                let declared_function = tree.walk();
                symbol_table.InsertFunction(declared_function);
            }
            TranslationUnit::FunctionDefinition(tree) => {
                let mut declared_function = tree.prototype.walk();
                let mut context = WalkContext::new(declared_function.control_flow());
                let definition_scope = tree.body.walk(&mut context);
                declared_function.add_definition(definition_scope);
                symbol_table.InsertFunction(declared_function);
            }
        }
    }
}

impl TypenameTree {
    fn walk(&self) -> Type {
        match self.name.as_str() {
            "u8" => Type::new_u8(0),
            "u16" => Type::new_u16(0),
            "u32" => Type::new_u32(0),
            "u64" => Type::new_u64(0),
            _ => {
                panic!("impossible type!");
            }
        }
    }
}

impl VariableDeclarationTree {
    fn walk(&self) -> Variable {
        Variable::new(self.name.clone(), self.typename.walk())
    }
}

impl FunctionDeclarationTree {
    fn walk(self) -> Function {
        Function::new(
            self.name,
            self.arguments.into_iter().map(|x| x.walk()).collect(),
            None,
        )
    }
}

impl ArithmeticOperationTree {
    pub fn walk(self, loc: SourceLoc, context: &WalkContext) -> IROperand {
        let temp_dest = IROperand::new_as_temporary(String::from("TEMP")); // TODO: temps/scoping/etc...?
        let ir = IR::new_binary_op(
            loc,
            match self {
                ArithmeticOperationTree::Add(operands) => BinaryOperations::new_add(
                    temp_dest.clone(),
                    operands.e1.walk(context),
                    operands.e2.walk(context),
                ),
                ArithmeticOperationTree::Subtract(operands) => BinaryOperations::new_subtract(
                    temp_dest.clone(),
                    operands.e1.walk(context),
                    operands.e2.walk(context),
                ),
                ArithmeticOperationTree::Multiply(operands) => BinaryOperations::new_multiply(
                    temp_dest.clone(),
                    operands.e1.walk(context),
                    operands.e2.walk(context),
                ),
                ArithmeticOperationTree::Divide(operands) => BinaryOperations::new_divide(
                    temp_dest.clone(),
                    operands.e1.walk(context),
                    operands.e2.walk(context),
                ),
            },
        );
        temp_dest
    }
}

impl ExpressionTree {
    pub fn walk(self, context: &WalkContext) -> IROperand {
        match self.expression {
            Expression::Identifier(ident) => IROperand::new_as_variable(ident),
            Expression::UnsignedDecimalConstant(constant) => {
                IROperand::new_as_unsigned_decimal_constant(constant)
            }
            Expression::Arithmetic(arithmetic_operation) => {
                arithmetic_operation.walk(self.loc, context)
            }
        }
    }
}

impl AssignmentTree {
    pub fn walk(self, context: &mut WalkContext) {
        let assignment_ir = IR::new_assignment(
            self.loc,
            IROperand::new_as_variable(self.identifier),
            self.value.walk(context),
        );
        context.append_ir(assignment_ir);
    }
}

impl StatementTree {
    fn walk(self, scope: &mut Scope, context: &mut WalkContext) {
        match self.statement {
            Statement::VariableDeclaration(tree) => scope.insert_variable(tree.walk()),
            Statement::Assignment(tree) => tree.walk(context),
        }
    }
}

impl CompoundStatementTree {
    fn walk(self, context: &mut WalkContext) -> Scope {
        let mut compound_scope = Scope::new();
        for statement in self.statements {
            statement.walk(&mut compound_scope, context);
        }
        compound_scope
    }
}
