use crate::midend::symtab::*;

pub mod function;
pub mod module;
pub mod scope;
pub mod type_definition;
pub mod variable;

pub use function::*;
pub use module::*;
pub use scope::*;
pub use type_definition::*;
pub use variable::*;

pub trait Symbol<'a>: Sized + 'a
where
    &'a Self: From<DefResolver<'a>>,
    DefGenerator<'a, Self>: Into<SymbolDef>,
{
    type SymbolKey: Clone + Into<DefPathComponent<'a>>;

    fn symbol_key(&self) -> &Self::SymbolKey;
}

pub struct DefResolver<'a> {
    pub type_interner: &'a TypeInterner<'a>,
    pub to_resolve: &'a SymbolDef,
}
impl<'a> DefResolver<'a> {
    pub fn new(type_interner: &'a TypeInterner<'a>, to_resolve: &'a SymbolDef) -> Self {
        Self {
            type_interner,
            to_resolve,
        }
    }
}

pub struct DefGenerator<'a, S>
where
    S: Symbol<'a>,
    &'a S: From<DefResolver<'a>>,
    DefGenerator<'a, S>: Into<SymbolDef>,
{
    pub def_path: DefPath<'a>,
    pub type_interner: &'a mut TypeInterner<'a>,
    pub to_generate_def_for: S,
}

impl<'a, S> DefGenerator<'a, S>
where
    S: Symbol<'a>,
    &'a S: From<DefResolver<'a>>,
    DefGenerator<'a, S>: Into<SymbolDef>,
{
    pub fn new(
        def_path: DefPath<'a>,
        type_interner: &'a mut TypeInterner<'a>,
        to_generate_def_for: S,
    ) -> Self {
        Self {
            def_path,
            type_interner,
            to_generate_def_for,
        }
    }
}

#[derive(Debug)]
pub enum SymbolDef {
    Module(Module),
    Type(TypeId),
    Function(Function),
    Scope(Scope),
    Variable(Variable),
    BasicBlock(ir::BasicBlock),
}

impl std::fmt::Display for SymbolDef {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Module(module) => write!(f, "{}", module),
            Self::Type(id) => write!(f, "{}", id),
            Self::Function(function) => write!(f, "{}", function.name()),
            Self::Scope(scope) => write!(f, "scope{}", scope),
            Self::Variable(variable) => write!(f, "{}", variable.name),
            Self::BasicBlock(block) => write!(f, "{}", block.label),
        }
    }
}
