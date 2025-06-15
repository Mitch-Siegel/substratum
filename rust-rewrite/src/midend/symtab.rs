use crate::{
    midend::{ir, types::Type},
    trace,
};
pub use errors::*;
pub use function::*;
pub use scope::Scope;
pub use type_definitions::*;
pub use variable::*;

mod errors;
mod function;
pub mod intrinsics;
mod module;
mod scope;
pub mod symtab_walker;
mod type_definitions;
mod variable;

pub use module::Module;
pub use scope::ScopePath;
pub use TypeRepr;

/// Traits for lookup and insertion based on ownership of various symbol types
pub trait ScopeOwner {
    fn subscopes(&self) -> impl Iterator<Item = &Scope>;
}
pub trait MutScopeOwner: ScopeOwner {
    fn insert_scope(&mut self, scope: Scope);
}

pub trait BasicBlockOwner {
    fn basic_blocks(&self) -> impl Iterator<Item = &ir::BasicBlock>;
    fn lookup_basic_block(&self, label: usize) -> Option<&ir::BasicBlock>;
}
pub trait MutBasicBlockOwner: BasicBlockOwner {
    fn insert_basic_block(&mut self, block: ir::BasicBlock);
    fn lookup_basic_block_mut(&mut self, label: usize) -> Option<&mut ir::BasicBlock>;
}

pub trait VariableOwner {
    fn variables(&self) -> impl Iterator<Item = &Variable>;
    fn lookup_variable_by_name(&self, name: &str) -> Result<&Variable, UndefinedSymbol>;
}
pub trait MutVariableOwner: VariableOwner {
    fn insert_variable(&mut self, variable: Variable) -> Result<(), DefinedSymbol>;
}

pub trait TypeOwner {
    fn types(&self) -> impl Iterator<Item = &TypeDefinition>;
    fn lookup_type(&self, type_: &Type) -> Result<&TypeDefinition, UndefinedSymbol>;
    fn lookup_struct(&self, name: &str) -> Result<&StructRepr, UndefinedSymbol>;
}
pub trait MutTypeOwner: TypeOwner {
    fn insert_type(&mut self, type_: TypeDefinition) -> Result<(), DefinedSymbol>;
    fn lookup_type_mut(&mut self, type_: &Type) -> Result<&mut TypeDefinition, UndefinedSymbol>;
}

pub trait FunctionOwner {
    fn functions(&self) -> impl Iterator<Item = &Function>;
    fn lookup_function(&self, name: &str) -> Result<&Function, UndefinedSymbol>;
    fn lookup_function_prototype(&self, name: &str) -> Result<&FunctionPrototype, UndefinedSymbol> {
        Ok(&self.lookup_function(name)?.prototype)
    }
}
pub trait MutFunctionOwner: FunctionOwner {
    fn insert_function(&mut self, function: Function) -> Result<(), DefinedSymbol>;
}

pub trait AssociatedOwner {
    fn associated_functions(&self) -> impl Iterator<Item = &Function>;
    // TODO: "maybe you meant..." for associated/method mismatch
    fn lookup_associated(&self, name: &str) -> Result<&Function, UndefinedSymbol>;
}
pub trait MutAssociatedOwner: AssociatedOwner {
    fn insert_associated(&mut self, associated: Function) -> Result<(), DefinedSymbol>;
}

pub trait MethodOwner {
    fn methods(&self) -> impl Iterator<Item = &Function>;
    // TODO: "maybe you meant..." for associated/method mismatch
    fn lookup_method(&self, name: &str) -> Result<&Function, UndefinedSymbol>;
}
pub trait MutMethodOwner: MethodOwner {
    fn insert_method(&mut self, method: Function) -> Result<(), DefinedSymbol>;
}

pub trait ModuleOwner {
    fn submodules(&self) -> impl Iterator<Item = &Module>;
    fn lookup_submodule(&self, name: &str) -> Result<&Module, UndefinedSymbol>;
}
pub trait MutModuleOwner: ModuleOwner {
    fn insert_module(&mut self, module: Module) -> Result<(), DefinedSymbol>;
}

pub trait SelfTypeOwner {
    fn self_type(&self) -> &Type;
}

pub trait ScopeOwnerships: BasicBlockOwner + VariableOwner + TypeOwner {}
pub trait MutScopeOwnerships: MutBasicBlockOwner + MutVariableOwner + MutTypeOwner {}

pub trait ModuleOwnerships: TypeOwner {}
pub trait MutModuleOwnerships: MutTypeOwner {}

pub struct SymbolTable {
    pub global_module: Module,
}

impl SymbolTable {
    pub fn new(global_module: Module) -> Self {
        SymbolTable { global_module }
    }

    pub fn collapse_scopes(&mut self) {
        let _ = trace::debug!("SymbolTable::collapse_scopes");
    }
}

#[cfg(test)]
pub mod tests {
    use crate::midend::{symtab::*, types};
    pub fn test_scope_owner<T>(owner: &mut T)
    where
        T: MutScopeOwner,
    {
        owner.insert_scope(Scope::new());
    }

    pub fn test_basic_block_owner<T>(owner: &T)
    where
        T: BasicBlockOwner,
    {
        let block_order = [0, 4, 1, 3, 2, 7, 8, 6, 5, 9];

        for label in block_order.iter().rev() {
            assert_eq!(owner.lookup_basic_block(*label), None);
        }
    }

    pub fn test_mut_basic_block_owner<T>(owner: &mut T)
    where
        T: MutBasicBlockOwner,
    {
        let block_order = [0, 4, 1, 3, 2, 7, 8, 6, 5, 9];

        for label in block_order.iter().rev() {
            assert_eq!(owner.lookup_basic_block(*label), None);
            assert_eq!(owner.lookup_basic_block_mut(*label), None);
        }

        for label in &block_order {
            owner.insert_basic_block(ir::BasicBlock::new(*label));
        }

        for label in block_order.iter().rev() {
            assert_eq!(owner.lookup_basic_block(*label).unwrap().label, *label);
            assert_eq!(owner.lookup_basic_block_mut(*label).unwrap().label, *label);
        }
    }

    pub fn test_variable_owner<T>(owner: &T)
    where
        T: VariableOwner,
    {
        let example_variable = Variable::new("test_variable".into(), Some(Type::U64));

        // make sure our example variable doesn't exist to start
        assert_eq!(
            owner.lookup_variable_by_name("test_variable"),
            Err(UndefinedSymbol::variable("test_variable".into()))
        );
    }

    pub fn test_mut_variable_owner<T>(owner: &mut T)
    where
        T: MutVariableOwner,
    {
        let example_variable = Variable::new("test_variable".into(), Some(Type::U64));

        // make sure our example variable doesn't exist to start
        assert_eq!(
            owner.lookup_variable_by_name("test_variable"),
            Err(UndefinedSymbol::variable("test_variable".into()))
        );

        // insert a variable
        assert_eq!(owner.insert_variable(example_variable.clone()), Ok(()));

        // trying to insert it again should error, it's already defined
        assert_eq!(
            owner.insert_variable(example_variable.clone()),
            Err(DefinedSymbol::variable(example_variable.clone()))
        );

        // now, looking it up should return ok
        assert_eq!(
            owner.lookup_variable_by_name("test_variable"),
            Ok(&example_variable)
        );
    }

    pub fn test_type_owner<T>(owner: &T)
    where
        T: TypeOwner + types::TypeSizingContext,
    {
        let example_struct_repr = StructRepr::new(
            "TestStruct".into(),
            vec![("first_field".into(), Type::U8)],
            owner,
        )
        .unwrap();

        let mut example_type = TypeDefinition::new(
            Type::UDT("TestStruct".into()),
            TypeRepr::Struct(example_struct_repr.clone()),
        );

        assert_eq!(
            owner.lookup_type(example_type.type_()),
            Err(UndefinedSymbol::type_(example_type.type_().clone()))
        );
        assert_eq!(
            owner.lookup_struct("TestStruct"),
            Err(UndefinedSymbol::struct_("TestStruct".into()))
        );
    }

    fn test_mut_type_owner<T>(owner: &mut T)
    where
        T: MutTypeOwner + types::TypeSizingContext,
    {
        let example_struct_repr = StructRepr::new(
            "TestStruct".into(),
            vec![("first_field".into(), Type::U8)],
            owner,
        )
        .unwrap();

        let mut example_type = TypeDefinition::new(
            Type::UDT("TestStruct".into()),
            TypeRepr::Struct(example_struct_repr.clone()),
        );

        assert_eq!(
            owner.lookup_type(example_type.type_()),
            Err(UndefinedSymbol::type_(example_type.type_().clone()))
        );
        assert_eq!(
            owner.lookup_struct("TestStruct"),
            Err(UndefinedSymbol::struct_("TestStruct".into()))
        );

        assert_eq!(
            owner.lookup_type_mut(example_type.type_()),
            Err(UndefinedSymbol::type_(example_type.type_().clone()))
        );

        assert_eq!(owner.insert_type(example_type.clone()), Ok(()));
        assert_eq!(
            owner.insert_type(example_type.clone()),
            Err(DefinedSymbol::type_(example_type.repr.clone()))
        );

        assert_eq!(owner.lookup_type(example_type.type_()), Ok(&example_type));

        assert_eq!(
            owner.lookup_type_mut(example_type.type_()),
            Ok(&mut example_type)
        );

        assert_eq!(owner.lookup_struct("TestStruct"), Ok(&example_struct_repr));
    }

    pub fn test_function_owner<T>(owner: &T)
    where
        T: FunctionOwner,
    {
        let example_function = Function::new(
            FunctionPrototype::new(
                "example_function".into(),
                vec![Variable::new("argument_1".into(), Some(Type::I32))],
                Type::I64,
            ),
            Scope::new(),
            ir::ControlFlow::new().0,
        );

        assert_eq!(
            owner.lookup_function("example_function"),
            Err(UndefinedSymbol::Function("example_function".into()))
        );
    }

    pub fn test_mut_function_owner<T>(owner: &mut T)
    where
        T: MutFunctionOwner,
    {
        let example_function = Function::new(
            FunctionPrototype::new(
                "example_function".into(),
                vec![Variable::new("argument_1".into(), Some(Type::I32))],
                Type::I64,
            ),
            Scope::new(),
            ir::ControlFlow::new().0,
        );

        assert_eq!(
            owner.lookup_function("example_function"),
            Err(UndefinedSymbol::Function("example_function".into()))
        );

        assert_eq!(owner.insert_function(example_function.clone()), Ok(()));
        assert_eq!(
            owner.insert_function(example_function.clone()),
            Err(DefinedSymbol::function(example_function.prototype.clone()))
        );

        assert_eq!(
            owner.lookup_function("example_function"),
            Ok(&example_function)
        );
    }

    pub fn test_module_owner<T>(owner: &T)
    where
        T: ModuleOwner,
    {
        let example_module = Module::new("A".into());

        assert_eq!(
            owner.lookup_submodule("A"),
            Err(UndefinedSymbol::module("A".into()))
        );
    }

    pub fn test_mut_module_owner<T>(owner: &mut T)
    where
        T: MutModuleOwner,
    {
        let example_module = Module::new("A".into());

        assert_eq!(
            owner.lookup_submodule("A"),
            Err(UndefinedSymbol::module("A".into()))
        );

        assert_eq!(owner.insert_module(example_module.clone()), Ok(()));
        assert_eq!(
            owner.insert_module(example_module.clone()),
            Err(DefinedSymbol::module("A".into())),
        );

        assert_eq!(owner.lookup_submodule("A"), Ok(&example_module));
    }
}
