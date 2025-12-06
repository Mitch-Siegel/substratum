use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct StructFieldTree {
    pub name: IdentifierTree,
    pub type_: TypeTree,
}

impl Ast for StructFieldTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.name.loc().merge(&self.type_.loc()).unwrap()
    }
}

impl Display for StructFieldTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {}", self.name, self.type_)
    }
}

impl treewalk::Linearize<(String, midend::types::Syntactic)> for StructFieldTree {
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> (String, midend::types::Syntactic) {
        let field_type = self
            .type_
            .linearize(ctx)
            .expect("struct field types may not be '_'");
        (self.name.linearize(ctx), field_type)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct StructDefinitionTree {
    pub struct_keyword_loc: sourceloc::SourceSpan,
    pub name: IdentifierTree,
    pub generic_params: Option<generics::GenericParamsListTree>,
    pub fields: Vec<StructFieldTree>,
    pub close_brace_loc: sourceloc::SourceSpan,
}

impl Ast for StructDefinitionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.struct_keyword_loc
            .clone()
            .merge(&self.close_brace_loc)
            .unwrap()
    }
}

impl Display for StructDefinitionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut fields = String::new();
        for field in &self.fields {
            fields += &field.to_string();
            fields += " ";
        }

        write!(f, "Struct Definition: {}: {}", self.name, fields)
    }
}

impl treewalk::CollectSymbols for StructDefinitionTree {
    fn collect_symbols(&self, ctx: &mut treewalk::CollectCtx) {
        ctx.declare(midend::symtab::DefPathComponent::Type(
            midend::types::Syntactic::Named(self.name.value.clone()),
        ))
        .unwrap();
    }
}

impl treewalk::Linearize<midend::symtab::StructRepr> for StructDefinitionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> midend::symtab::StructRepr {
        let generic_params: midend::types::GenericParamsList = match self.generic_params {
            Some(params) => params.linearize(ctx),
            None => midend::types::GenericParamsList::new(),
        };

        let struct_name = self.name.linearize(ctx);

        let type_def_path_component = midend::symtab::DefPathComponent::Type(
            midend::types::Syntactic::Named(struct_name.clone()),
        );
        ctx.push_def_path(type_def_path_component.clone(), &generic_params);

        let fields = self
            .fields
            .into_iter()
            .map(|field| field.linearize(ctx))
            .collect::<Vec<_>>();

        ctx.pop_def_path(type_def_path_component).unwrap();
        midend::symtab::StructRepr::new(struct_name, generic_params, fields).unwrap()
    }
}
