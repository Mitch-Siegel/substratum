mod symtab;
mod types;

use crate::ast::*;
pub use symtab::*;
use types::*;

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
                declared_function.add_definition(tree.body.walk());
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

impl StatementTree {
    fn walk(self, scope: &mut Scope) {
        match self.statement {
            Statement::VariableDeclaration(tree) => scope.insert_variable(tree.walk()),
            Statement::Assignment(tree) => {}
        }
    }
}

impl CompoundStatementTree {
    fn walk(self) -> Scope {
        let mut compound_scope = Scope::new();
        for statement in self.statements {
            statement.walk(&mut compound_scope);
        }
        compound_scope
    }
}
