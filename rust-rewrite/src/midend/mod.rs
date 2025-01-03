mod control_flow;
pub mod ir;
mod symtab;
mod types;

use crate::{ast::*, lexer::SourceLoc};
use control_flow::ControlFlow;
use ir::*;
pub use symtab::*;
use types::*;

struct WalkContext {
    control_flow: Option<ControlFlow>,
    scopes: Vec<Scope>,
}

impl WalkContext {
    fn new(control_flow: ControlFlow) -> WalkContext {
        WalkContext {
            control_flow: Some(control_flow),
            scopes: Vec::new(),
        }
    }

    pub fn append_ir(&mut self, statement: IR) {
        self.control_flow
            .as_mut()
            .expect("WalkContext::append_ir expects valid control flow")
            .append_statement_to_current_block(statement);
    }

    pub fn converge_control(&mut self) {
        self.control_flow
            .take()
            .expect("WalkContext::converge_control expects valid control flow")
            .converge_control();
    }

    pub fn next_temp(&mut self, type_: Type) -> IROperand {
        let temp_name = self
            .control_flow
            .as_mut()
            .expect("WalkContext::next_temp expects valid control flow")
            .next_temp();
        self.scope()
            .insert_variable(Variable::new(temp_name.clone(), type_));
        IROperand::new_as_temporary(temp_name)
    }

    pub fn push_scope(&mut self, scope: Scope) {
        self.scopes.push(scope)
    }

    pub fn pop_scope_to_subscope_of_next(&mut self) {
        let popped = self
            .scopes
            .pop()
            .expect("WalkContext::pop_scope_to_subscope_of_next expects valid scope");
        self.scope().insert_subscope(popped);
    }

    pub fn pop_last_scope(&mut self) -> Scope {
        if self.scopes.len() > 1 {
            panic!(
                "WalkContext::pop_last_scope() called with {} parent scopes",
                self.scopes.len()
            );
        }

        self.scopes
            .pop()
            .expect("WalkContext::pop_last_scope() called with no scopese")
    }

    pub fn take_control_flow(&mut self) -> ControlFlow {
        self.control_flow
            .take()
            .expect("WalkContext::take_control_flow expects valid control flow")
    }

    pub fn scope(&mut self) -> &mut Scope {
        self.scopes
            .last_mut()
            .expect("WalkContext::scope() expects valid scope")
    }

    pub fn lookup_variable_by_name(&self, name: &str) -> Option<&Variable> {
        for scope in (&self.scopes).into_iter().rev().by_ref() {
            match scope.lookup_variable_by_name(name) {
                Some(variable) => return Some(variable),
                None => {}
            }
        }
        None
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
                symbol_table.insert_function_prototype(declared_function);
            }
            TranslationUnit::FunctionDefinition(tree) => {
                let mut declared_prototype = tree.prototype.walk();
                let mut context = WalkContext::new(ControlFlow::new());
                context.push_scope(declared_prototype.create_argument_scope());

                tree.body.walk(&mut context);
                let argument_scope = context.pop_last_scope();

                symbol_table.insert_function(Function::new(
                    declared_prototype,
                    argument_scope,
                    context.take_control_flow(),
                ));
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
    fn walk(self) -> FunctionPrototype {
        FunctionPrototype::new(
            self.name,
            self.arguments.into_iter().map(|x| x.walk()).collect(),
            match self.return_type {
                Some(typename) => Some(typename.walk()),
                None => None,
            },
        )
    }
}

impl ArithmeticOperationTree {
    pub fn walk(self, loc: SourceLoc, context: &mut WalkContext) -> IROperand {
        let (temp_dest, op) = match self {
            ArithmeticOperationTree::Add(operands) => {
                let lhs = operands.e1.walk(context);
                let rhs = operands.e2.walk(context);
                let dest = context.next_temp(lhs.type_(context));
                (dest.clone(), BinaryOperations::new_add(dest, lhs, rhs))
            }
            ArithmeticOperationTree::Subtract(operands) => {
                let lhs = operands.e1.walk(context);
                let rhs = operands.e2.walk(context);
                let dest = context.next_temp(lhs.type_(context));
                (dest.clone(), BinaryOperations::new_divide(dest, lhs, rhs))
            }
            ArithmeticOperationTree::Multiply(operands) => {
                let lhs = operands.e1.walk(context);
                let rhs = operands.e2.walk(context);
                let dest = context.next_temp(lhs.type_(context));
                (dest.clone(), BinaryOperations::new_multiply(dest, lhs, rhs))
            }
            ArithmeticOperationTree::Divide(operands) => {
                let lhs = operands.e1.walk(context);
                let rhs = operands.e2.walk(context);
                let dest = context.next_temp(lhs.type_(context));
                (dest.clone(), BinaryOperations::new_divide(dest, lhs, rhs))
            }
        };

        let operation = IR::new_binary_op(loc, op);
        context.append_ir(operation);
        temp_dest
    }
}

impl ExpressionTree {
    pub fn walk(self, context: &mut WalkContext) -> IROperand {
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
    fn walk(self, context: &mut WalkContext) {
        match self.statement {
            Statement::VariableDeclaration(tree) => context.scope().insert_variable(tree.walk()),
            Statement::Assignment(tree) => tree.walk(context),
        }
    }
}

impl CompoundStatementTree {
    fn walk(self, context: &mut WalkContext) {
        context.push_scope(Scope::new());
        for statement in self.statements {
            statement.walk(context);
        }
        context.pop_scope_to_subscope_of_next();
    }
}
