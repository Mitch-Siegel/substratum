#[derive(Clone, PartialEq, Eq)]
pub(crate) enum PathError {
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
pub(crate) struct TypeSegment(pub String);

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct ValueSegment(pub String);

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct MacroSegment(pub String);

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct ImplSegment(pub ImplId);

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Debug)]
pub(crate) struct ImplId(pub usize);

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct ScopeSegment(pub ScopeId);

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Debug)]
pub(crate) struct ScopeId(pub usize);

impl TryFrom<PathSegment> for TypeSegment {
    type Error = ();
    fn try_from(value: PathSegment) -> Result<Self, Self::Error> {
        match value {
            PathSegment::Type(data) => Ok(Self(data)),
            _ => Err(()),
        }
    }
}

impl TryFrom<PathSegment> for ValueSegment {
    type Error = ();
    fn try_from(value: PathSegment) -> Result<Self, Self::Error> {
        match value {
            PathSegment::Value(data) => Ok(Self(data)),
            _ => Err(()),
        }
    }
}

impl TryFrom<PathSegment> for MacroSegment {
    type Error = ();
    fn try_from(value: PathSegment) -> Result<Self, Self::Error> {
        match value {
            PathSegment::Macro(data) => Ok(Self(data)),
            _ => Err(()),
        }
    }
}

impl TryFrom<PathSegment> for ImplSegment {
    type Error = ();
    fn try_from(value: PathSegment) -> Result<Self, Self::Error> {
        match value {
            PathSegment::Impl(data) => Ok(Self(data)),
            _ => Err(()),
        }
    }
}

impl TryFrom<PathSegment> for ScopeSegment {
    type Error = ();
    fn try_from(value: PathSegment) -> Result<Self, Self::Error> {
        match value {
            PathSegment::Scope(data) => Ok(Self(data)),
            _ => Err(()),
        }
    }
}

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) enum PathSegment {
    Type(String),
    Value(String),
    Macro(String),
    Impl(ImplId),
    Scope(ScopeId),
}

impl PathSegment {
    pub(crate) fn can_own(&self, other: &Self) -> bool {
        matches!(
            (self, other),
            (Self::Type(_), _)
                | (Self::Value(_), Self::Value(_))
                | (Self::Value(_), Self::Type(_))
                | (Self::Value(_), Self::Scope(_))
                | (Self::Scope(_), Self::Scope(_))
                | (Self::Scope(_), Self::Value(_))
        )
    }

    pub(crate) fn raw(&self) -> &str {
        match self {
            Self::Type(name) | Self::Value(name) | Self::Macro(name) => name,
            Self::Impl(_) => panic!("no raw name for Impl segments"),
            Self::Scope(_) => panic!("no raw name for Scope segments"),
        }
    }
}

impl From<PathSegment> for String {
    fn from(value: PathSegment) -> String {
        String::from(value.raw())
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
            Self::Type(name) => write!(f, " Type:{}", name),
            Self::Value(name) => write!(f, "Value:{}", name),
            Self::Macro(name) => write!(f, "Macro:{}", name),
            Self::Impl(id) => write!(f, " Impl:({})", id.0),
            Self::Scope(id) => write!(f, "Scope:({})", id.0),
        }?;
        write!(f, ")")
    }
}

pub(crate) trait Path:
    Clone
    + std::fmt::Debug
    + std::fmt::Display
    + std::ops::Index<usize, Output = PathSegment>
    + IntoIterator<Item = PathSegment>
    + Sized
    + Ord
    + Into<RawPath>
{
    fn len(&self) -> usize;

    fn _first(&self) -> &PathSegment {
        &self[0]
    }

    fn last(&self) -> &PathSegment {
        &self[self.len() - 1]
    }

    /// TRUE if self is a prefix of other OR self == other
    fn is_prefix_of(&self, other: &impl Path) -> bool {
        if self.len() > other.len() {
            return false;
        }

        for idx in 0..self.len() {
            if self[idx] != other[idx] {
                return false;
            }
        }
        true
    }

    fn split_last(self) -> (Option<RawPath>, PathSegment);
}

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct RawPath {
    prefix_segments: Vec<PathSegment>,
    last: PathSegment,
}

impl RawPath {
    pub(crate) fn new(prefix_segments: Vec<PathSegment>, last: PathSegment) -> Self {
        Self {
            prefix_segments,
            last,
        }
    }

    pub(crate) fn is_type(&self) -> bool {
        matches!(self.last, PathSegment::Type(_))
    }

    pub(crate) fn is_value(&self) -> bool {
        matches!(self.last, PathSegment::Value(_))
    }

    pub(crate) fn is_macro(&self) -> bool {
        matches!(self.last, PathSegment::Macro(_))
    }

    pub(crate) fn is_impl(&self) -> bool {
        matches!(self.last, PathSegment::Impl(_))
    }

    pub(crate) fn is_scope(&self) -> bool {
        matches!(self.last, PathSegment::Scope(_))
    }

    pub(crate) fn with_segment(mut self, segment: PathSegment) -> Result<Self, PathError> {
        if self.last.can_own(&segment) {
            let old_last = std::mem::replace(&mut self.last, segment);
            self.prefix_segments.push(old_last);
            Ok(self)
        } else {
            Err(PathError::CantOwn(self.last.clone(), segment))
        }
    }
}

impl Path for RawPath {
    fn len(&self) -> usize {
        self.prefix_segments.len() + 1
    }

    fn split_last(mut self) -> (Option<RawPath>, PathSegment) {
        let prefix_path = if !self.prefix_segments.is_empty() {
            let new_last = self.prefix_segments.pop().unwrap();
            Some(RawPath::new(self.prefix_segments, new_last))
        } else {
            None
        };

        (prefix_path, self.last)
    }
}

pub(crate) trait TypeOwner: Path {
    #[allow(unused)]
    fn with_child_type(self, name: String) -> TypePath {
        self.into()
            .with_segment(PathSegment::Type(name))
            .unwrap()
            .into()
    }
}
pub(crate) trait ValueOwner: Path {
    fn with_child_value(self, name: String) -> ValuePath {
        self.into()
            .with_segment(PathSegment::Value(name))
            .unwrap()
            .into()
    }

    fn with_scope(self, id: ScopeId) -> ScopePath {
        self.into()
            .with_segment(PathSegment::Scope(id))
            .unwrap()
            .into()
    }
}
pub(crate) trait MacroOwner: Path {
    #[allow(unused)]
    fn with_child_macro(self, name: String) -> MacroPath {
        self.into()
            .with_segment(PathSegment::Macro(name))
            .unwrap()
            .into()
    }
}

pub(crate) trait ImplOwner: Path {
    #[allow(unused)]
    fn with_child_impl(self, id: ImplId) -> ImplPath {
        self.into()
            .with_segment(PathSegment::Impl(id))
            .unwrap()
            .into()
    }
}

pub(crate) trait ScopeOwner: Path {
    fn with_child_scope(self, id: ScopeId) -> ScopePath {
        self.into()
            .with_segment(PathSegment::Scope(id))
            .unwrap()
            .into()
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
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct TypePath(pub(in crate::midend::symtab) RawPath);
#[allow(unused)]
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct ValuePath(pub(in crate::midend::symtab) RawPath);
#[allow(unused)]
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) struct MacroPath(pub(in crate::midend::symtab) RawPath);
#[allow(unused)]
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) struct ImplPath(pub(in crate::midend::symtab) RawPath);

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) struct ScopePath(pub(in crate::midend::symtab) RawPath);

impl TypeOwner for TypePath {}
impl ValueOwner for TypePath {}

impl ValueOwner for ValuePath {}
impl ScopeOwner for ValuePath {}

impl ValueOwner for ImplPath {}

impl ValueOwner for ScopePath {}
impl ScopeOwner for ScopePath {}

impl TypePath {
    pub(crate) fn new(parent: Option<impl Path>, name: String) -> Self {
        match parent {
            Some(parent) => Self(parent.into().with_segment(PathSegment::Type(name)).unwrap()),
            None => Self(RawPath::new(Vec::new(), PathSegment::Type(name))),
        }
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

impl From<RawPath> for ScopePath {
    fn from(value: RawPath) -> Self {
        assert!(value.is_scope());
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

    fn split_last(self) -> (Option<RawPath>, PathSegment) {
        self.0.split_last()
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

    fn split_last(self) -> (Option<RawPath>, PathSegment) {
        self.0.split_last()
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

impl std::ops::Index<usize> for ImplPath {
    type Output = PathSegment;

    fn index(&self, index: usize) -> &Self::Output {
        &self.0[index]
    }
}

impl Path for ImplPath {
    fn len(&self) -> usize {
        self.0.len()
    }

    fn split_last(self) -> (Option<RawPath>, PathSegment) {
        self.0.split_last()
    }
}

impl From<ImplPath> for RawPath {
    fn from(value: ImplPath) -> Self {
        value.0
    }
}

impl IntoIterator for ImplPath {
    type Item = PathSegment;
    type IntoIter = std::vec::IntoIter<Self::Item>;

    fn into_iter(self) -> Self::IntoIter {
        self.0.into_iter().collect::<Vec<_>>().into_iter()
    }
}

impl std::ops::Index<usize> for ScopePath {
    type Output = PathSegment;

    fn index(&self, index: usize) -> &Self::Output {
        &self.0[index]
    }
}

impl Path for ScopePath {
    fn len(&self) -> usize {
        self.0.len()
    }

    fn split_last(self) -> (Option<RawPath>, PathSegment) {
        self.0.split_last()
    }
}

impl From<ScopePath> for RawPath {
    fn from(value: ScopePath) -> Self {
        value.0
    }
}

impl IntoIterator for ScopePath {
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

impl std::fmt::Display for ImplPath {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Impl {}", self.0)
    }
}

impl std::fmt::Display for ScopePath {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Scope {}", self.0)
    }
}
