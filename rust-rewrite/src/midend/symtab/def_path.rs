use crate::midend::{symtab::*, types};

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum DefPathComponent<'a> {
    Module(<Module as Symbol<'a>>::SymbolKey),
    Type(<TypeDefinition as Symbol<'a>>::SymbolKey),
    Function(<Function as Symbol<'a>>::SymbolKey),
    Scope(<Scope as Symbol<'a>>::SymbolKey),
    Variable(<Variable as Symbol<'a>>::SymbolKey),
    BasicBlock(<ir::BasicBlock as Symbol<'a>>::SymbolKey),
}

impl<'a> DefPathComponent<'a> {
    pub fn can_own(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::Module(_), Self::Module(_)) => true,
            (Self::Module(_), Self::Type(_)) => true,
            (Self::Module(_), Self::Function(_)) => true,
            (Self::Module(_), Self::Variable(_)) => true,
            (Self::Module(_), _) => false,
            (Self::Type(_), Self::Function(_)) => true,
            (Self::Type(_), _) => false,
            (Self::Function(_), Self::Scope(_)) => true,
            (Self::Function(_), Self::Variable(_)) => true,
            (Self::Function(_), Self::BasicBlock(_)) => true,
            (_, _) => false,
        }
    }

    pub fn name(&self) -> &str {
        match self {
            Self::Module(_) => "module",
            Self::Type(_) => "type",
            Self::Function(_) => "function",
            Self::Scope(_) => "scope",
            Self::Variable(_) => "variable",
            Self::BasicBlock(_) => "basic block",
        }
    }
}

impl<'a> From<<Module as Symbol<'a>>::SymbolKey> for DefPathComponent<'a> {
    fn from(module_key: <Module as Symbol>::SymbolKey) -> Self {
        Self::Module(module_key)
    }
}
impl<'a> From<<TypeDefinition as Symbol<'a>>::SymbolKey> for DefPathComponent<'a> {
    fn from(type_definition_key: <TypeDefinition as Symbol>::SymbolKey) -> Self {
        Self::Type(type_definition_key)
    }
}
impl<'a> From<<Function as Symbol<'a>>::SymbolKey> for DefPathComponent<'a> {
    fn from(function_key: <Function as Symbol>::SymbolKey) -> Self {
        Self::Function(function_key)
    }
}
impl<'a> From<<Scope as Symbol<'a>>::SymbolKey> for DefPathComponent<'a> {
    fn from(scope_key: <Scope as Symbol>::SymbolKey) -> Self {
        Self::Scope(scope_key)
    }
}
impl<'a> From<<Variable as Symbol<'a>>::SymbolKey> for DefPathComponent<'a> {
    fn from(variable_key: <Variable as Symbol>::SymbolKey) -> Self {
        Self::Variable(variable_key)
    }
}
impl<'a> From<<ir::BasicBlock as Symbol<'a>>::SymbolKey> for DefPathComponent<'a> {
    fn from(block_key: <ir::BasicBlock as Symbol>::SymbolKey) -> Self {
        Self::BasicBlock(block_key)
    }
}

impl<'a> std::fmt::Display for DefPathComponent<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Module(module) => write!(f, "{}", module),
            Self::Type(type_) => write!(f, "{}", type_),
            Self::Function(function) => write!(f, "{}", function),
            Self::Scope(scope) => write!(f, "{}", scope),
            Self::Variable(variable) => write!(f, "{}", variable),
            Self::BasicBlock(block) => write!(f, "{}", block),
        }
    }
}

impl<'a> std::fmt::Debug for DefPathComponent<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}(", self.name())?;
        match self {
            Self::Module(module) => write!(f, "{}", module),
            Self::Type(type_) => write!(f, "{}", type_),
            Self::Function(function) => write!(f, "{}", function),
            Self::Scope(scope) => write!(f, "{}", scope),
            Self::Variable(variable) => write!(f, "{}", variable),
            Self::BasicBlock(block) => write!(f, "{}", block),
        }?;
        write!(f, ")")
    }
}

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct DefPath<'a> {
    components: Vec<DefPathComponent<'a>>,
}

impl<'a> DefPath<'a> {
    pub fn new() -> Self {
        Self {
            components: Vec::new(),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.components.is_empty()
    }

    pub fn last(&self) -> Option<&DefPathComponent<'a>> {
        self.components.last()
    }

    pub fn pop(&mut self) -> Option<DefPathComponent<'a>> {
        self.components.pop()
    }

    pub fn push(&mut self, component: DefPathComponent<'a>) {
        assert!(self.can_own(&component));

        self.components.push(component);
    }

    pub fn can_own(&self, component: &DefPathComponent<'a>) -> bool {
        match self.components.last() {
            Some(last_component) => last_component.can_own(&component),
            None => matches!(component, DefPathComponent::Module(_)),
        }
    }

    pub fn is_type(&self) -> bool {
        match self.last() {
            Some(DefPathComponent::Type(_)) => true,
            _ => false,
        }
    }

    pub fn with_component(mut self, component: DefPathComponent<'a>) -> Self {
        self.push(component);
        self
    }
}

impl<'a> std::fmt::Display for DefPath<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut first = true;
        for component in &self.components {
            write!(f, "{}", component)?;
            if !first {
                write!(f, "::")?;
            }
            first = false;
        }
        Ok(())
    }
}

impl<'a> std::fmt::Debug for DefPath<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut first = true;
        for component in &self.components {
            write!(f, "{:?}", component)?;
            if !first {
                write!(f, "::")?;
            }
            first = false;
        }
        Ok(())
    }
}
