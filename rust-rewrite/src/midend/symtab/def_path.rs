use crate::midend::symtab::*;

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum DefPathComponent {
    Type(String),
    Value(String),
    Macro(String),
}

impl DefPathComponent {
    pub fn can_own(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::Type(_), _) => true,
            (_, _) => false,
        }
    }

    pub fn raw(&self) -> &str {
        match self {
            Self::Type(name) | Self::Value(name) | Self::Macro(name) => &name,
        }
    }
}

impl<'a> std::fmt::Display for DefPathComponent {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.raw())
    }
}

impl<'a> std::fmt::Debug for DefPathComponent {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}(", self.raw())?;
        match self {
            Self::Type(name) => write!(f, "T:{}", name),
            Self::Value(name) => write!(f, "V:{}", name),
            Self::Macro(name) => write!(f, "M:{}", name),
        }?;
        write!(f, ")")
    }
}

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct DefPath {
    components: Vec<DefPathComponent>,
}

impl DefPath {
    pub fn empty() -> Self {
        Self {
            components: Vec::new(),
        }
    }

    pub fn len(&self) -> usize {
        self.components.len()
    }

    pub fn is_empty(&self) -> bool {
        self.components.is_empty()
    }

    fn idx(&self, idx: usize) -> &DefPathComponent {
        &self.components[idx]
    }

    pub fn is_prefix_of(&self, other: &DefPath) -> bool {
        if self.len() >= other.len() {
            return false;
        }

        for idx in 0..self.len() {
            if self.idx(idx) != other.idx(idx) {
                return false;
            }
        }
        true
    }

    pub fn first(&self) -> Option<&DefPathComponent> {
        self.components.first()
    }

    pub fn last(&self) -> Option<&DefPathComponent> {
        self.components.last()
    }

    pub fn pop(&mut self) -> Option<DefPathComponent> {
        self.components.pop()
    }

    pub fn push(&mut self, component: DefPathComponent) -> Result<(), SymbolError> {
        if self.can_own(&component) {
            self.components.push(component);
            Ok(())
        } else {
            Err(SymbolError::CantOwn(self.clone(), component))
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
            None => DefPathComponent::Empty.can_own(&component),
        }
    }

    pub fn is_type(&self) -> bool {
        match self.last() {
            Some(DefPathComponent::Type(_)) => true,
            _ => false,
        }
    }

    pub fn is_value(&self) -> bool {
        match self.last() {
            Some(DefPathComponent::Value(_)) => true,
            _ => false,
        }
    }

    pub fn is_macro(&self) -> bool {
        match self.last() {
            Some(DefPathComponent::Macro(_)) => true,
            _ => false,
        }
    }

    pub fn with_component(mut self, component: DefPathComponent) -> Result<Self, SymbolError> {
        self.push(component)?;
        Ok(self)
    }

    pub fn parent(mut self) -> Option<Self> {
        match self.pop() {
            Some(_) => Some(self),
            None => None,
        }
    }

    pub fn split_last(mut self) -> (Self, Option<DefPathComponent>) {
        let last = self.pop();
        (self, last)
    }
}

impl std::fmt::Display for DefPath {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for (index, component) in self.components.iter().enumerate() {
            write!(f, "{}", component)?;
            if index < (self.components.len() - 1) {
                write!(f, "::")?;
            }
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
