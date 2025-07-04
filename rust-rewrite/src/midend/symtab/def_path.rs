use crate::midend::{symtab::*, types};

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum DefPathComponent {
    Empty,
    Module(<Module as Symbol>::SymbolKey),
    Type(<TypeDefinition as Symbol>::SymbolKey),
    Function(<Function as Symbol>::SymbolKey),
    Scope(<Scope as Symbol>::SymbolKey),
    Variable(<Variable as Symbol>::SymbolKey),
    BasicBlock(<ir::BasicBlock as Symbol>::SymbolKey),
}

impl DefPathComponent {
    pub fn can_own(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::Empty, Self::Module(_)) => true,
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
            Self::Empty => "empty",
            Self::Module(_) => "module",
            Self::Type(_) => "type",
            Self::Function(_) => "function",
            Self::Scope(_) => "scope",
            Self::Variable(_) => "variable",
            Self::BasicBlock(_) => "basic block",
        }
    }
}

impl<'a> From<<Module as Symbol>::SymbolKey> for DefPathComponent {
    fn from(module_key: <Module as Symbol>::SymbolKey) -> Self {
        Self::Module(module_key)
    }
}
impl<'a> From<<TypeDefinition as Symbol>::SymbolKey> for DefPathComponent {
    fn from(type_definition_key: <TypeDefinition as Symbol>::SymbolKey) -> Self {
        Self::Type(type_definition_key)
    }
}
impl<'a> From<<Function as Symbol>::SymbolKey> for DefPathComponent {
    fn from(function_key: <Function as Symbol>::SymbolKey) -> Self {
        Self::Function(function_key)
    }
}
impl<'a> From<<Scope as Symbol>::SymbolKey> for DefPathComponent {
    fn from(scope_key: <Scope as Symbol>::SymbolKey) -> Self {
        Self::Scope(scope_key)
    }
}
impl<'a> From<<Variable as Symbol>::SymbolKey> for DefPathComponent {
    fn from(variable_key: <Variable as Symbol>::SymbolKey) -> Self {
        Self::Variable(variable_key)
    }
}
impl<'a> From<<ir::BasicBlock as Symbol>::SymbolKey> for DefPathComponent {
    fn from(block_key: <ir::BasicBlock as Symbol>::SymbolKey) -> Self {
        Self::BasicBlock(block_key)
    }
}

impl<'a> std::fmt::Display for DefPathComponent {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Empty => write!(f, "empty"),
            Self::Module(module) => write!(f, "{}", module),
            Self::Type(type_) => write!(f, "{}", type_),
            Self::Function(function) => write!(f, "{}", function),
            Self::Scope(scope) => write!(f, "{}", scope),
            Self::Variable(variable) => write!(f, "{}", variable),
            Self::BasicBlock(block) => write!(f, "{}", block),
        }
    }
}

impl<'a> std::fmt::Debug for DefPathComponent {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}(", self.name())?;
        match self {
            Self::Empty => write!(f, "empty"),
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
pub struct DefPath {
    parent_component: DefPathComponent,
    components: Vec<DefPathComponent>,
}

impl DefPath {
    pub fn new(parent_component: DefPathComponent) -> Self {
        Self {
            parent_component,
            components: Vec::new(),
        }
    }

    pub fn empty() -> Self {
        Self {
            parent_component: DefPathComponent::Empty,
            components: Vec::new(),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.components.is_empty()
    }

    pub fn last(&self) -> &DefPathComponent {
        self.components.last().unwrap_or(&DefPathComponent::Empty)
    }

    pub fn pop(&mut self) -> Option<DefPathComponent> {
        self.components.pop()
    }

    pub fn push(&mut self, component: DefPathComponent) -> Result<(), SymbolError> {
        if self.can_own(&component) {
            self.components.push(component);
            Ok(())
        } else {
            Err(SymbolError::CantOwn(self.last().clone(), component))
        }
    }

    pub fn join(mut self, other: DefPath) -> Result<Self, SymbolError> {
        for component in other.components.into_iter().rev() {
            self.push(component)?;
        }
        Ok(self)
    }

    pub fn can_own(&self, component: &DefPathComponent) -> bool {
        match self.components.last() {
            Some(last_component) => last_component.can_own(&component),
            None => matches!(component, DefPathComponent::Module(_)),
        }
    }

    pub fn is_type(&self) -> bool {
        match self.last() {
            DefPathComponent::Type(_) => true,
            _ => false,
        }
    }

    pub fn with_component(mut self, component: DefPathComponent) -> Result<Self, SymbolError> {
        self.push(component)?;
        Ok(self)
    }
}

impl std::fmt::Display for DefPath {
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

impl std::fmt::Debug for DefPath {
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
