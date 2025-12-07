use crate::frontend::{
    ast::{
        expressions::{arithmetic::*, *},
        types::{
            PrimitiveTypePathTree, TypeItemPathTree, TypeNoBoundsTree, TypePath,
            TypePathSegmentTree,
        },
        Expression, *,
    },
    sourceloc::{SourcePoint, SourceSpan},
};

use super::types::ReferenceTypeTree;

pub fn test_span(start_line: u32, start_col: u32, end_line: u32, end_col: u32) -> SourceSpan {
    SourceSpan::new(
        String::from(""),
        SourcePoint::new(start_line, start_col),
        SourcePoint::new(end_line, end_col),
    )
}

pub fn id(line: u32, col: u32, name: &str) -> Expression {
    Expression::PathInExpression(PathInExpressionTree {
        segments: vec![PathExprSegmentTree {
            ident: PathIdentSegment::Ident(IdentifierTree {
                loc: test_span(
                    line,
                    col,
                    line,
                    col + TryInto::<u32>::try_into(name.len()).unwrap(),
                ),
                value: name.into(),
            }),
            generic_args: None,
        }],
    })
}

pub fn unsigned_decimal_constant(line: u32, col: u32, value: usize) -> Expression {
    Expression::UnsignedDecimalConstant(
        test_span(
            line,
            col,
            line,
            col + TryInto::<u32>::try_into(format!("{}", value).len()).unwrap(),
        ),
        value,
    )
}

pub fn add(lhs: Expression, rhs: Expression) -> Expression {
    Expression::Arithmetic(ArithmeticExpressionTree::Add(ArithmeticDualOperands::new(
        lhs, rhs,
    )))
}

pub fn mul(lhs: Expression, rhs: Expression) -> Expression {
    Expression::Arithmetic(ArithmeticExpressionTree::Multiply(
        ArithmeticDualOperands::new(lhs, rhs),
    ))
}

pub fn primitive_type(
    start_line: u32,
    start_col: u32,
    type_: midend::types::Syntactic,
) -> TypeNoBoundsTree {
    TypeNoBoundsTree::TypePath(TypePath::Primitive(PrimitiveTypePathTree {
        loc: test_span(
            start_line,
            start_col,
            start_line,
            start_col + TryInto::<u32>::try_into(format!("{}", type_).len()).unwrap(),
        ),
        type_: type_,
    }))
}

pub fn named_type(start_line: u32, start_col: u32, name: &str) -> TypeTree {
    TypeTree::TypeNoBounds(TypeNoBoundsTree::TypePath(TypePath::ItemPath(
        TypeItemPathTree {
            starts_global: None,
            segments: vec![TypePathSegmentTree {
                ident_segment: PathIdentSegment::Ident(IdentifierTree {
                    loc: test_span(
                        start_line,
                        start_col,
                        start_line,
                        start_col + TryInto::<u32>::try_into(name.len()).unwrap(),
                    ),
                    value: name.into(),
                }),
                generic_args: None,
            }],
        },
    )))
}

pub fn reference_of_type(
    line: u32,
    col: u32,
    mutability: midend::types::Mutability,
    inner_type: TypeNoBoundsTree,
) -> ReferenceTypeTree {
    ReferenceTypeTree {
        reference_token_loc: test_span(line, col, line, col + 1),
        mutability,
        type_: Box::new(inner_type),
    }
}
