use crate::midend::{types::*, *};

pub enum SemanticFunctionError {
    UnresolvableType(Syntactic),
    NonFunction(Syntactic),
}

impl std::fmt::Display for SemanticFunctionError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::UnresolvableType(t) => write!(
                f,
                "syntactic type {} ({:?}) cannot be resolved to a semantic type",
                t, t
            ),
            Self::NonFunction(t) => write!(
                f,
                "semantic function cannot be generated from non-function syntactic type of {} ({:?})",
                t, t
            ),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct SemanticFunction {
    syntactic: Syntactic,
    pub arguments: Vec<Semantic>,
    pub return_value: Semantic,
}

impl SemanticFunction {
    pub fn new(syntactic: Syntactic, arguments: Vec<Semantic>, return_value: Semantic) -> Self {
        Self {
            syntactic,
            arguments,
            return_value,
        }
    }
}

impl
    TryFrom<(
        Syntactic,
        ParamSubstMap,
        &symtab::DefPath,
        &symtab::SymbolTable,
    )> for SemanticFunction
{
    type Error = SemanticFunctionError;
    fn try_from(
        value: (
            Syntactic,
            ParamSubstMap,
            &symtab::DefPath,
            &symtab::SymbolTable,
        ),
    ) -> Result<Self, Self::Error> {
        let (syntactic, generic_params, def_path, symtab) = value;

        match &syntactic {
            Syntactic::Function(args, return_type) => {
                let mut semantic_args = Vec::<Semantic>::new();
                for arg in args.iter() {
                    match symtab.semantic_type_for_syntactic(def_path, generic_params.clone(), arg)
                    {
                        Ok(ty_) => semantic_args.push(ty_),
                        _ => Err(SemanticFunctionError::UnresolvableType(arg.clone()))?,
                    }
                }

                let semantic_return_type =
                    match symtab.semantic_type_for_syntactic(def_path, generic_params, return_type)
                    {
                        Ok(ty_) => ty_,
                        _ => Err(SemanticFunctionError::UnresolvableType(
                            *return_type.clone(),
                        ))?,
                    };

                Ok(Self::new(syntactic, semantic_args, semantic_return_type))
            }
            _ => Err(SemanticFunctionError::NonFunction(syntactic)),
        }
    }
}
