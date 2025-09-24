use crate::midend::{ir::*, linearizer::DefContext};

pub mod operands;
use operands::*;

#[enum_delegate::register]
pub trait Lowerable {
    fn lower(self, context: &mut linearizer::FunctionWalkContext, loc: SourceLoc);
}

#[derive(PartialEq, Eq, Clone)]
pub struct Operation {
    pub def_path: symtab::DefPath,
    pub ty: OperationType,
}

impl Operation {
    pub fn lower(
        self,
        symtab: Box<symtab::SymbolTable>,
        manager: BlockManager,
        current_block: usize,
        loc: SourceLoc,
    ) -> (Box<symtab::SymbolTable>, BlockManager) {
        let parent_function_def_path = self.def_path.clone().parent_function().unwrap();

        let mut ctx = linearizer::FunctionWalkContext::from_existing(
            symtab,
            parent_function_def_path.clone(),
            linearizer::GenericParamsContext::new(),
            manager,
            current_block,
        );

        let mut to_push_components = Vec::new();
        let mut dupe_def_path = self.def_path.clone();
        for _ in 0..(self.def_path.len() - parent_function_def_path.len()) {
            to_push_components.push(dupe_def_path.pop().unwrap());
        }
        to_push_components.reverse();

        for to_push in to_push_components {
            ctx.push_def_path(to_push, &Vec::new());
        }

        self.ty.lower(&mut ctx, loc);

        while ctx.def_path().len() > parent_function_def_path.len() {
            ctx.pop_def_path(ctx.def_path().last().clone()).unwrap();
        }

        let (symtab, _, _, manager) = ctx.take().unwrap();
        (symtab, manager)
    }
}

impl OperandTypePropagation for Operation {
    fn propagate_types(&self, ctx: &TypePropagationContext) -> bool {
        self.ty.propagate_types(ctx)
    }
}

impl std::fmt::Display for Operation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(f, "{}", self.ty)
    }
}

impl std::fmt::Debug for Operation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(f, "{:?}: {:?}", self.def_path, self.ty)
    }
}

#[derive(Debug, PartialEq, Eq, Clone)]
#[enum_delegate::implement(Lowerable)]
pub enum OperationType {
    Match(MatchOperands),
    Discriminant(DiscriminantOperands),
    GetFieldPointer(FieldPointerOperands),
}

impl OperandTypePropagation for OperationType {
    fn propagate_types(&self, ctx: &TypePropagationContext) -> bool {
        match self {
            Self::Match(_) => true,
            Self::Discriminant(d) => d.propagate_types(ctx),
            Self::GetFieldPointer(f) => f.propagate_types(ctx),
        }
    }
}

impl Operation {
    pub fn read_value_ids(&self) -> Vec<ValueId> {
        vec![]
    }

    pub fn write_value_ids(&self) -> Vec<ValueId> {
        vec![]
    }
}

impl std::fmt::Display for OperationType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Match(_m) => write!(f, "match"),
            Self::Discriminant(_d) => write!(f, "discriminant"),
            Self::GetFieldPointer(_) => write!(f, "getfieldptr"),
        }
    }
}

pub fn new_match(def_path: symtab::DefPath, scrutinee: ValueId, arms: Vec<MatchArm>) -> Operation {
    Operation {
        def_path,
        ty: OperationType::Match(MatchOperands { scrutinee, arms }),
    }
}

pub fn new_discriminant(
    def_path: symtab::DefPath,
    destination: ValueId,
    enum_receiver: ValueId,
) -> Operation {
    Operation {
        def_path,
        ty: OperationType::Discriminant(DiscriminantOperands {
            destination,
            enum_receiver,
        }),
    }
}

pub fn new_get_field_pointer(
    def_path: symtab::DefPath,
    receiver: ValueId,
    field_name: String,
    destination: ValueId,
) -> Operation {
    Operation {
        def_path,
        ty: OperationType::GetFieldPointer(FieldPointerOperands {
            receiver,
            field_name,
            destination,
        }),
    }
}
