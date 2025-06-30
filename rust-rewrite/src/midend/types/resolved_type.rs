use crate::midend::types::*;

impl Into<ResolvedType> for PrimitiveType {
    fn into(self) -> ResolvedType {
        ResolvedType::Primitive(self)
    }
}

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Debug, Hash)]
pub enum ResolvedType {
    Primitive(PrimitiveType),
    UserDefined(std::rc::Rc<symtab::UnresolvedTypeRepr>),
    Reference(Mutability, Box<ResolvedType>),
    Pointer(Mutability, Box<ResolvedType>),
}

impl ResolvedType {
    pub fn is_integral<C>(&self) -> bool {
        match self {
            ResolvedType::Primitive(_)
            | ResolvedType::Reference(_, _)
            | ResolvedType::Pointer(_, _) => true,
            ResolvedType::UserDefined(_) => false,
        }
    }

    // size of the type in bytes
    pub fn size<Target>(&self) -> usize
    where
        Target: backend::arch::TargetArchitecture,
    {
        match self {
            ResolvedType::Primitive(primitive) => primitive.size(),
            ResolvedType::UserDefined(udt) => udt.size::<Target>(),
            ResolvedType::Reference(_, _) => Target::word_size(),
            ResolvedType::Pointer(_, _) => Target::word_size(),
        }
    }

    pub fn align_size_power_of_two(size: usize) -> usize {
        if size == 0 {
            0
        } else if size == 1 {
            1
        } else {
            size.next_power_of_two()
        }
    }

    pub fn alignment<Target>(&self) -> usize
    where
        Target: backend::arch::TargetArchitecture,
    {
        match self {
            ResolvedType::Primitive(_)
            | ResolvedType::Reference(_, _)
            | ResolvedType::Pointer(_, _) => Self::align_size_power_of_two(self.size::<Target>()),
            ResolvedType::UserDefined(repr) => repr.alignment::<Target>(),
        }
    }
}

impl Display for ResolvedType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Primitive(primitive) => write!(f, "{}", primitive),
            Self::UserDefined(repr) => write!(f, "user-defined type {}", repr.name()),
            Self::Reference(mutability, to) => write!(f, "&{} {}", mutability, to),
            Self::Pointer(mutability, to) => write!(f, "*{} {}", mutability, to),
        }
    }
}
