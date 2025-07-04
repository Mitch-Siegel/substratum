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

pub trait Symbol: Sized
where
    for<'a> &'a Self: From<DefResolver<'a>>,
    for<'a> &'a mut Self: From<MutDefResolver<'a>>,
    for<'a> DefGenerator<'a, Self>: Into<SymbolDef>,
{
    type SymbolKey: Clone + Into<DefPathComponent>;

    fn symbol_key(&self) -> &Self::SymbolKey;
}

pub struct DefResolver<'a> {
    pub to_resolve: &'a SymbolDef,
    pub type_interner: &'a TypeInterner,
}
impl<'a> DefResolver<'a> {
    pub fn new(type_interner: &'a TypeInterner, to_resolve: &'a SymbolDef) -> Self {
        Self {
            type_interner,
            to_resolve,
        }
    }
}

pub struct MutDefResolver<'a> {
    pub to_resolve: &'a mut SymbolDef,
    pub type_interner: &'a mut TypeInterner,
}
impl<'a, 'b> MutDefResolver<'a> {
    pub fn new(to_resolve: &'a mut SymbolDef, type_interner: &'a mut TypeInterner) -> Self {
        Self {
            type_interner,
            to_resolve,
        }
    }
}

pub struct DefGenerator<'a, S>
where
    S: Symbol,
    for<'b> &'b S: From<DefResolver<'b>>,
    for<'b> &'b mut S: From<MutDefResolver<'b>>,
    for<'b> DefGenerator<'b, S>: Into<SymbolDef>,
{
    pub def_path: DefPath,
    pub type_interner: &'a mut TypeInterner,
    pub to_generate_def_for: S,
}

impl<'a, S> DefGenerator<'a, S>
where
    S: Symbol,
    for<'b> &'b S: From<DefResolver<'b>>,
    for<'b> &'b mut S: From<MutDefResolver<'b>>,
    for<'b> DefGenerator<'b, S>: Into<SymbolDef>,
{
    pub fn new(
        def_path: DefPath,
        type_interner: &'a mut TypeInterner,
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
