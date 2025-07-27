use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct FieldExpressionTree {
    pub loc: SourceLocWithMod,
    pub receiver: ExpressionTree,
    pub field: String,
}

impl FieldExpressionTree {
    pub fn new(loc: SourceLocWithMod, receiver: ExpressionTree, field: String) -> Self {
        Self {
            loc,
            receiver,
            field,
        }
    }
}

impl Display for FieldExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}.{}", self.receiver, self.field)
    }
}

// returns (receiver, field_info)
// receiver is the value id for the receiver of the field access
// field_info is a value id for the field being accessed
impl<'a> ReturnFunctionWalk<'a, (midend::ir::ValueId, &'a midend::symtab::StructField)>
    for FieldExpressionTree
{
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(
        self,
        context: &'a mut FunctionWalkContext,
    ) -> (midend::ir::ValueId, &'a midend::symtab::StructField) {
        let receiver = self.receiver.walk(context);

        let struct_name = match context.value_for_id(&receiver).unwrap().type_ {
            Some(type_id) => {
                let struct_type = context.type_for_id(&type_id).unwrap();
                match &struct_type.repr {
                    midend::symtab::TypeRepr::Struct(struct_repr) => &struct_repr.name,
                    other_repr => panic!(
                        "Field expression receiver must be of struct type (got {})",
                        other_repr.name()
                    ),
                }
            }
            _ => panic!("unknown type of field receiver",),
        };

        let receiver_type = context.resolve_type_name(struct_name).expect(&format!(
            "Error handling for failed lookups is unimplemented: {}.{}",
            struct_name, self.field
        ));

        // TODO: handle generic params
        let receiver_definition = context
            .lookup::<midend::symtab::TypeDefinition>(&receiver_type)
            .unwrap();

        let struct_receiver = match &receiver_definition.repr {
            midend::symtab::TypeRepr::Struct(struct_definition) => struct_definition,
            non_struct_repr => panic!("Named type {} is not a struct", non_struct_repr.name()),
        };

        let accessed_field = struct_receiver.lookup_field(&self.field).unwrap();
        (receiver, accessed_field)
    }
}
