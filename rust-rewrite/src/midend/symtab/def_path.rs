use crate::midend::types;

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash, Debug)]
pub enum DefPathComponent {
    Module(String),
    Type(types::Type),
    Function(String),
    Scope(usize),
    Variable(String),
    BasicBlock(usize),
}

impl DefPathComponent {
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
}

impl std::fmt::Display for DefPathComponent {
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

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct DefPath {
    components: Vec<DefPathComponent>,
}

impl DefPath {
    pub fn new() -> Self {
        Self {
            components: Vec::new(),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.components.is_empty()
    }

    pub fn last(&self) -> Option<&DefPathComponent> {
        self.components.last()
    }

    pub fn pop(&mut self) -> Option<DefPathComponent> {
        self.components.pop()
    }

    pub fn push(&mut self, component: DefPathComponent) {
        assert!(self.can_own(&component));

        self.components.push(component);
    }

    pub fn join(&mut self, other: DefPath) {
        for other_component in other.components.into_iter().rev() {
            self.push(other_component);
        }
    }

    pub fn can_own(&self, component: &DefPathComponent) -> bool {
        match self.components.last() {
            Some(last_component) => last_component.can_own(&component),
            None => matches!(component, DefPathComponent::Module(_)),
        }
    }

    pub fn parent(&self) -> Self {
        let mut parent = self.clone();
        parent.pop();
        parent
    }

    pub fn clone_with_new_last(&self, last: DefPathComponent) -> Self {
        let mut new_path = self.clone();
        new_path.push(last);
        new_path
    }

    pub fn clone_with_join(&self, other: DefPath) -> Self {
        let mut new_path = self.clone();
        new_path.join(other);
        new_path
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
