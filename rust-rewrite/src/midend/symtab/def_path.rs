#[derive(Clone, PartialEq, Eq)]
pub enum PathError {
    CantOwn(PathSegment, PathSegment),
    PopEmpty,
    WithoutLastSingleSegment(RawPath),
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

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Debug)]
pub struct ImplId(pub u32);

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum PathSegment {
    Type(String),
    Value(String),
    Macro(String),
    Impl(ImplId),
}

impl PathSegment {
    pub fn can_own(&self, other: &Self) -> bool {
        matches!(
            (self, other),
            (Self::Type(_), _) | (Self::Value(_), Self::Value(_)) | (Self::Value(_), Self::Type(_))
        )
    }

    pub fn raw(&self) -> &str {
        match self {
            Self::Type(name) | Self::Value(name) | Self::Macro(name) => name,
            Self::Impl(_) => panic!("no raw name for Impl segments"),
        }
    }
}

impl From<PathSegment> for String {
    fn from(value: PathSegment) -> String {
        match value {
            PathSegment::Type(s) | PathSegment::Value(s) | PathSegment::Macro(s) => s,
            PathSegment::Impl(_) => panic!("no raw name for Impl segments"),
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

impl std::fmt::Display for PathSegment {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.raw())
    }
}

impl std::fmt::Debug for PathSegment {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}(", self.raw())?;
        match self {
            Self::Type(name) => write!(f, "T:{}", name),
            Self::Value(name) => write!(f, "V:{}", name),
            Self::Macro(name) => write!(f, "M:{}", name),
            Self::Impl(id) => write!(f, "I:Impl({})", id.0),
        }?;
        write!(f, ")")
    }
}

pub trait Path:
    Clone
    + std::fmt::Debug
    + std::fmt::Display
    + std::ops::Index<usize, Output = PathSegment>
    + IntoIterator<Item = PathSegment>
    + Sized
    + Into<RawPath>
{
    fn len(&self) -> usize;

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
pub struct RawPath {
    prefix_segments: Vec<PathSegment>,
    last: PathSegment,
}

impl RawPath {
    pub fn new(prefix_segments: Vec<PathSegment>, last: PathSegment) -> Self {
        Self {
            prefix_segments,
            last,
        }
    }

    pub fn is_type(&self) -> bool {
        matches!(self.last, PathSegment::Type(_))
    }

    pub fn is_value(&self) -> bool {
        matches!(self.last, PathSegment::Value(_))
    }

    pub fn is_macro(&self) -> bool {
        matches!(self.last, PathSegment::Macro(_))
    }

    pub fn is_impl(&self) -> bool {
        matches!(self.last, PathSegment::Impl(_))
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

    pub fn without_last(mut self) -> Result<(RawPath, PathSegment), PathError> {
        if self.prefix_segments.is_empty() {
            return Err(PathError::WithoutLastSingleSegment(self));
        }

        let last = std::mem::replace(&mut self.last, self.prefix_segments.pop().unwrap());
        Ok((self, last))
    }
}

impl Path for RawPath {
    fn len(&self) -> usize {
        self.prefix_segments.len() + 1
    }
}

impl std::ops::Index<usize> for RawPath {
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

impl IntoIterator for RawPath {
    type Item = PathSegment;
    type IntoIter = std::iter::Chain<std::vec::IntoIter<Self::Item>, std::iter::Once<Self::Item>>;
    fn into_iter(self) -> Self::IntoIter {
        self.prefix_segments
            .into_iter()
            .chain(std::iter::once(self.last))
    }
}

impl std::fmt::Display for RawPath {
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

impl std::fmt::Debug for RawPath {
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

#[allow(unused)]
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TypePath(pub(in crate::midend::symtab) RawPath);
#[allow(unused)]
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ValuePath(pub(in crate::midend::symtab) RawPath);
#[allow(unused)]
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MacroPath(pub(in crate::midend::symtab) RawPath);
#[allow(unused)]
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ImplPath(pub(in crate::midend::symtab) RawPath);

impl TypePath {
    pub fn new(parent: Option<impl Path>, name: String) -> Self {
        match parent {
            Some(parent) => Self(parent.into().with_segment(PathSegment::Type(name)).unwrap()),
            None => Self(RawPath::new(Vec::new(), PathSegment::Type(name))),
        }
    }

    #[allow(unused)]
    pub fn with_child_type(self, name: String) -> Self {
        Self(self.0.with_segment(PathSegment::Type(name)).unwrap())
    }

    #[allow(unused)]
    pub fn with_child_value(self, name: String) -> ValuePath {
        ValuePath(self.0.with_segment(PathSegment::Value(name)).unwrap())
    }

    #[allow(unused)]
    pub fn with_child_macro(self, name: String) -> MacroPath {
        MacroPath(self.0.with_segment(PathSegment::Macro(name)).unwrap())
    }

    #[allow(unused)]
    pub fn with_child_impl(self, id: ImplId) -> ImplPath {
        ImplPath(self.0.with_segment(PathSegment::Impl(id)).unwrap())
    }
}

impl ValuePath {
    #[allow(unused)]
    pub fn with_child_value(self, name: String) -> Self {
        Self(self.0.with_segment(PathSegment::Value(name)).unwrap())
    }

    #[allow(unused)]
    pub fn with_child_type(self, name: String) -> TypePath {
        TypePath(self.0.with_segment(PathSegment::Type(name)).unwrap())
    }

    #[allow(unused)]
    pub fn with_child_macro(self, name: String) -> MacroPath {
        MacroPath(self.0.with_segment(PathSegment::Macro(name)).unwrap())
    }

    #[allow(unused)]
    pub fn with_child_impl(self, id: ImplId) -> ImplPath {
        ImplPath(self.0.with_segment(PathSegment::Impl(id)).unwrap())
    }
}

impl ImplPath {
    #[allow(unused)]
    pub fn with_child_value(self, name: String) -> ValuePath {
        ValuePath(self.0.with_segment(PathSegment::Value(name)).unwrap())
    }
}

impl From<RawPath> for TypePath {
    fn from(value: RawPath) -> Self {
        assert!(value.is_type());
        Self(value)
    }
}

impl From<RawPath> for ValuePath {
    fn from(value: RawPath) -> Self {
        assert!(value.is_value());
        Self(value)
    }
}

impl From<RawPath> for MacroPath {
    fn from(value: RawPath) -> Self {
        assert!(value.is_macro());
        Self(value)
    }
}

impl From<RawPath> for ImplPath {
    fn from(value: RawPath) -> Self {
        assert!(value.is_impl());
        Self(value)
    }
}

impl std::ops::Index<usize> for TypePath {
    type Output = PathSegment;

    fn index(&self, index: usize) -> &Self::Output {
        &self.0[index]
    }
}

impl Path for TypePath {
    fn len(&self) -> usize {
        self.0.len()
    }
}

impl From<TypePath> for RawPath {
    fn from(value: TypePath) -> Self {
        value.0
    }
}

impl IntoIterator for TypePath {
    type Item = PathSegment;
    type IntoIter = std::vec::IntoIter<Self::Item>;

    fn into_iter(self) -> Self::IntoIter {
        self.0.into_iter().collect::<Vec<_>>().into_iter()
    }
}

impl std::ops::Index<usize> for ValuePath {
    type Output = PathSegment;

    fn index(&self, index: usize) -> &Self::Output {
        &self.0[index]
    }
}

impl Path for ValuePath {
    fn len(&self) -> usize {
        self.0.len()
    }
}

impl From<ValuePath> for RawPath {
    fn from(value: ValuePath) -> Self {
        value.0
    }
}

impl IntoIterator for ValuePath {
    type Item = PathSegment;
    type IntoIter = std::vec::IntoIter<Self::Item>;

    fn into_iter(self) -> Self::IntoIter {
        self.0.into_iter().collect::<Vec<_>>().into_iter()
    }
}

impl std::fmt::Display for TypePath {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Type {}", self.0)
    }
}

impl std::fmt::Display for ValuePath {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Value {}", self.0)
    }
}
