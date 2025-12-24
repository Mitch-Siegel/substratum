use crate::midend::symtab::*;

#[derive(Clone, PartialEq, Eq)]
pub enum PathError {
    CantOwn(PathSegment, PathSegment),
}

impl std::fmt::Display for PathError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{:?}", self)
    }
}

impl std::fmt::Debug for PathError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::CantOwn(owner, owned) => write!(
                f,
                "def path component {:?} can't own component {:?}",
                owner, owned
            ),
        }
    }
}

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum PathSegment {
    Type(String),
    Value(String),
    Macro(String),
}

impl PathSegment {
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

impl<'a> std::fmt::Display for PathSegment {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.raw())
    }
}

impl<'a> std::fmt::Debug for PathSegment {
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

pub trait Path: std::ops::Index<usize, Output = PathSegment> {
    fn len(&self) -> usize;

    fn first(&self) -> &PathSegment {
        &self[0]
    }

    fn last(&self) -> &PathSegment {
        &self[self.len() - 1]
    }

    fn is_type(&self) -> bool {
        match self.last() {
            PathSegment::Type(_) => true,
            _ => false,
        }
    }

    fn is_value(&self) -> bool {
        match self.last() {
            PathSegment::Value(_) => true,
            _ => false,
        }
    }

    fn is_macro(&self) -> bool {
        match self.last() {
            PathSegment::Macro(_) => true,
            _ => false,
        }
    }
}

#[derive(Clone, PartialEq, Eq, Hash)]
pub struct RawPath {
    components: Vec<PathSegment>,
}

impl RawPath {
    pub fn empty() -> Self {
        Self {
            components: Vec::new(),
        }
    }

    pub fn is_prefix_of(&self, other: &DefPath) -> bool {
        if self.len() >= other.len() {
            return false;
        }

        for idx in 0..self.len() {
            if self[idx] != other[idx] {
                return false;
            }
        }
        true
    }

    pub fn pop(&mut self) -> Option<PathSegment> {
        self.components.pop()
    }

    pub fn push(&mut self, component: PathSegment) -> Result<(), PathError> {
        if self.can_own(&component) {
            self.components.push(component);
            Ok(())
        } else {
            Err(PathError::CantOwn(self.last().clone(), component))
        }
    }

    pub fn join(mut self, other: RawPath) -> Result<Self, PathError> {
        for component in other.components.into_iter().rev() {
            self.push(component)?;
        }
        Ok(self)
    }

    pub fn can_own(&self, component: &PathSegment) -> bool {
        match self.components.last() {
            Some(last_component) => last_component.can_own(&component),
            None => true,
        }
    }
}

impl Path for RawPath {
    fn len(&self) -> usize {
        self.components.len()
    }
}

impl std::fmt::Display for RawPath {
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

impl std::fmt::Debug for RawPath {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for (index, component) in self.components.iter().enumerate() {
            write!(f, "{:?}", component)?;
            if index < (self.components.len() - 1) {
                write!(f, "::")?;
            }
        }
        Ok(())
    }
}

impl std::ops::Index<usize> for RawPath {
    type Output = PathSegment;
    fn index(&self, index: usize) -> &Self::Output {
        &self.components[index]
    }
}

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct DefPath {
    prefix_components: Vec<PathSegment>,
    last: PathSegment,
}

impl Path for DefPath {
    fn len(&self) -> usize {
        self.prefix_components.len() + 1
    }
}

impl std::ops::Index<usize> for DefPath {
    type Output = PathSegment;
    fn index(&self, index: usize) -> &Self::Output {
        let prefix_len = self.prefix_components.len();
        if index > (prefix_len + 1) {
            panic!("out-of-bounds index on defpath");
        } else if index == prefix_len {
            &self.last
        } else {
            &self.prefix_components[index]
        }
    }
}

impl std::fmt::Display for DefPath {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for (index, component) in self.prefix_components.iter().enumerate() {
            write!(f, "{}", component)?;
            if index < (self.prefix_components.len()) {
                write!(f, "::")?;
            }
        }
        write!(f, "{}", self.last);
        Ok(())
    }
}

impl std::fmt::Debug for DefPath {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for (index, component) in self.prefix_components.iter().enumerate() {
            write!(f, "{:?}", component)?;
            if index < (self.prefix_components.len()) {
                write!(f, "::")?;
            }
        }
        write!(f, "{:?}", self.last);
        Ok(())
    }
}
