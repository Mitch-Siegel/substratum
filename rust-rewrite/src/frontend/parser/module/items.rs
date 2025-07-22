use crate::frontend::parser::*;

mod enum_definition;
mod function;
mod implementation;
mod module_item;
mod struct_definition;

pub fn parse_implementation_item<'a>(parser: &mut Parser<'a>) -> Result<Item, ParseError> {
    Ok(Item::Implementation(parser.parse_implementation()?))
}

pub fn parse_enum_definition_item<'a>(parser: &mut Parser<'a>) -> Result<Item, ParseError> {
    Ok(Item::EnumDefinition(parser.parse_enum_definition()?))
}

pub fn parse_struct_definition_item<'a>(parser: &mut Parser<'a>) -> Result<Item, ParseError> {
    Ok(Item::StructDefinition(parser.parse_struct_definition()?))
}

pub fn parse_function_definition_item<'a>(
    parser: &mut Parser<'a>,
    allow_self_param: bool,
) -> Result<Item, ParseError> {
    let prototype = parser.parse_function_prototype(allow_self_param)?;
    Ok(Item::FunctionDefinition(
        parser.parse_function_definition(prototype)?,
    ))
}
