use crate::{frontend::ast::*, midend::symtab::Path};

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

impl midend::treewalk::Linearize for StructFieldTree {
    type Data = (String, midend::types::Syntactic);
    fn linearize(
        self,
        ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let (maybe_field_type, ctx) = self.type_.linearize_in_place(ctx)?;

        let field_type = maybe_field_type.expect("struct field types may not be '_'");

        let (name, ctx) = self.name.linearize(ctx)?;
        ctx.into_result((name, field_type))
    }
}

impl Display for StructFieldTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {}", self.name, self.type_)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct StructDefinitionTree {
    pub struct_keyword_loc: sourceloc::SourceSpan,
    pub name: IdentifierTree,
    pub generic_params: generics::OptionalGenericParamsListTree,
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

impl midend::treewalk::Collect for StructDefinitionTree {
    fn collect_symbols(
        &self,
        mut ctx: midend::treewalk::CollectCtx,
    ) -> midend::treewalk::CollectResult {
        let struct_path = ctx.declare_type(self.name.value.clone())?;

        self.generic_params.collect_symbols(
            ctx.with_segment(struct_path.last().clone())
                .expect("invalid path for struct"),
        )
    }
}

impl midend::treewalk::Linearize for StructDefinitionTree {
    type Data = (
        midend::symtab::types::StructRepr,
        <generics::OptionalGenericParamsListTree as midend::treewalk::Linearize>::Data,
    );
    #[tracing::instrument(skip(self, ctx), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(
        self,
        mut ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let struct_name;
        (struct_name, ctx) = self.name.linearize_in_place(ctx)?;

        ctx = ctx
            .with_segment(midend::symtab::PathSegment::Type(struct_name.clone()))
            .unwrap();

        let mut fields = Vec::new();
        for field in self.fields {
            let linearized_field;
            (linearized_field, ctx) = field.linearize_in_place(ctx)?;
            fields.push(linearized_field);
        }

        // TODO: struct duplicate field error
        let struct_repr = midend::symtab::types::StructRepr::new(struct_name, fields).unwrap();

        let (params, ctx) = self.generic_params.linearize(ctx)?;

        ctx.into_result((struct_repr, params))
    }
}
