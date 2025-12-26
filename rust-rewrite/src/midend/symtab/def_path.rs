#[derive(Clone, PartialEq, Eq)]
pub enum PathError {
    CantOwn(PathSegment, PathSegment),
    PopEmpty,
    WithoutLastSingleSegment(DefPath),
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
            Self::PopEmpty => write!(f, "pop from empty defpath"),
            Self::WithoutLastSingleSegment(p) => {
                write!(f, ".without_last() call would leave path {} empty", p)
            }
        }
    }
}

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct TypeSegment(String);

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ValueSegment(String);

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct MacroSegment(String);

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

impl Into<String> for PathSegment {
    fn into(self) -> String {
        match self {
            Self::Type(s) | Self::Value(s) | Self::Macro(s) => s,
        }
    }
}

impl From<TypeSegment> for PathSegment {
    fn from(value: TypeSegment) -> Self {
        Self::Type(value.0)
    }
}
impl From<ValueSegment> for PathSegment {
    fn from(value: ValueSegment) -> Self {
        Self::Value(value.0)
    }
}
impl From<MacroSegment> for PathSegment {
    fn from(value: MacroSegment) -> Self {
        Self::Macro(value.0)
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

pub trait Path:
    std::ops::Index<usize, Output = PathSegment> + IntoIterator<Item = PathSegment> + Sized
{
    fn len(&self) -> usize;

    fn join(self, other: Self) -> Result<Self, PathError>;

    fn without_last(self) -> Result<(Self, PathSegment), PathError>;

    fn _first(&self) -> &PathSegment {
        &self[0]
    }

    fn last(&self) -> &PathSegment {
        &self[self.len() - 1]
    }

    fn _is_prefix_of(&self, other: Self) -> bool {
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
}

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct DefPath {
    prefix_segments: Vec<PathSegment>,
    last: PathSegment,
}

impl DefPath {
    pub fn new(prefix_segments: Vec<PathSegment>, last: PathSegment) -> Self {
        Self {
            prefix_segments,
            last,
        }
    }

    pub fn new_type(prefix_segments: Vec<PathSegment>, name: String) -> Self {
        Self {
            prefix_segments,
            last: PathSegment::Type(name),
        }
    }

    pub fn new_value(prefix_segments: Vec<PathSegment>, name: String) -> Self {
        Self {
            prefix_segments,
            last: PathSegment::Value(name),
        }
    }

    pub fn new_macro(prefix_segments: Vec<PathSegment>, name: String) -> Self {
        Self {
            prefix_segments,
            last: PathSegment::Macro(name),
        }
    }

    pub fn with_segment(mut self, segment: PathSegment) -> Result<Self, PathError> {
        if self.last.can_own(&segment) {
            let old_last = std::mem::replace(&mut self.last, segment);
            self.prefix_segments.push(old_last);
            Ok(self)
        } else {
            Err(PathError::CantOwn(self.last.clone(), segment))
        }
    }

    pub fn is_type(&self) -> bool {
        match self.last {
            PathSegment::Type(_) => true,
            _ => false,
        }
    }

    pub fn is_value(&self) -> bool {
        match self.last {
            PathSegment::Value(_) => true,
            _ => false,
        }
    }

    pub fn is_macro(&self) -> bool {
        match self.last {
            PathSegment::Macro(_) => true,
            _ => false,
        }
    }
}

impl Path for DefPath {
    fn len(&self) -> usize {
        self.prefix_segments.len() + 1
    }

    fn join(mut self, other: Self) -> Result<Self, PathError> {
        for segment in other.into_iter() {
            let prev_last: PathSegment = std::mem::replace(&mut self.last, segment.clone());
            if prev_last.can_own(&segment) {
                self.prefix_segments.push(prev_last);
            } else {
                return Err(PathError::CantOwn(self.last.into(), segment));
            }
        }
        Ok(self)
    }

    fn without_last(mut self) -> Result<(DefPath, PathSegment), PathError> {
        if self.prefix_segments.len() < 1 {
            return Err(PathError::WithoutLastSingleSegment(self));
        }

        let last = std::mem::replace(&mut self.last, self.prefix_segments.pop().unwrap().into());
        Ok((self, last.into()))
    }
}

impl std::ops::Index<usize> for DefPath {
    type Output = PathSegment;
    fn index(&self, index: usize) -> &Self::Output {
        let prefix_len = self.prefix_segments.len();
        if index > (prefix_len + 1) {
            panic!("out-of-bounds index on defpath");
        } else if index == prefix_len {
            &self.last
        } else {
            &self.prefix_segments[index]
        }
    }
}

impl IntoIterator for DefPath {
    type Item = PathSegment;
    type IntoIter = std::iter::Chain<std::vec::IntoIter<Self::Item>, std::iter::Once<Self::Item>>;
    fn into_iter(self) -> Self::IntoIter {
        self.prefix_segments
            .into_iter()
            .chain(std::iter::once(self.last.into()))
    }
}

impl std::fmt::Display for DefPath {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for (index, component) in self.prefix_segments.iter().enumerate() {
            write!(f, "{}", component)?;
            if index < (self.prefix_segments.len()) {
                write!(f, "::")?;
            }
        }
        write!(f, "{}", self.last)
    }
}

impl std::fmt::Debug for DefPath {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for (index, component) in self.prefix_segments.iter().enumerate() {
            write!(f, "{:?}", component)?;
            if index < (self.prefix_segments.len()) {
                write!(f, "::")?;
            }
        }
        write!(f, "{:?}", self.last)
    }
}
