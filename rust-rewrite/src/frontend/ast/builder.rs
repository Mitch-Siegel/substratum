use crate::frontend::{
    ast::{
        expressions::{arithmetic::*, PathInExpressionTree},
        path::{IdentSegment, PathSegmentTree, PathTree},
        types::{PrimitiveTypePathTree, TypeItemPathTree, TypeNoBoundsTree, TypePath},
        Expression, *,
    },
    sourceloc::{SourceLoc, SourcePoint, SourceSpan},
};

use super::types::ReferenceTypeTree;

pub(crate) fn test_loc(line: u32, col: u32) -> SourceLoc {
    SourceLoc::new(String::from(""), SourcePoint::new(line, col))
}

pub(crate) fn test_span(start_line: u32, start_col: u32, end_line: u32, end_col: u32) -> SourceSpan {
    SourceSpan::new(
        String::from(""),
        SourcePoint::new(start_line, start_col),
        SourcePoint::new(end_line, end_col),
    )
}

fn expand_span(span: SourceSpan, cols: u32) -> SourceSpan {
    let mut end = span.clone().end();
    end.point.col += cols;
    span.merge(&end.into()).unwrap()
}

pub(crate) fn identifier(value: &str, start_loc: SourceLoc) -> IdentifierTree {
    IdentifierTree {
        value: value.into(),
        loc: expand_span(
            start_loc.into(),
            TryInto::<u32>::try_into(value.len()).unwrap(),
        ),
    }
}

pub(crate) fn path_ident_segment<T>(
    value: &str,
    start_point: SourceLoc,
    data: Option<T>,
) -> PathSegmentTree<T>
where
    T: Ast,
{
    PathSegmentTree {
        ident: IdentSegment::Ident(identifier(value, start_point)),
        data,
    }
}

pub(crate) fn path_super_segment<T>(start_loc: SourceLoc, data: Option<T>) -> PathSegmentTree<T>
where
    T: Ast,
{
    PathSegmentTree {
        ident: IdentSegment::Super(expand_span(start_loc.into(), 5)),
        data,
    }
}

pub(crate) fn path_self_lower_segment<T>(start_loc: SourceLoc, data: Option<T>) -> PathSegmentTree<T>
where
    T: Ast,
{
    PathSegmentTree {
        ident: IdentSegment::SelfLower(expand_span(start_loc.into(), 4)),
        data,
    }
}

pub(crate) fn path_self_upper_segment<T>(start_loc: SourceLoc, data: Option<T>) -> PathSegmentTree<T>
where
    T: Ast,
{
    PathSegmentTree {
        ident: IdentSegment::SelfUpper(expand_span(start_loc.into(), 4)),
        data,
    }
}

pub(crate) fn path<T>(segments: Vec<PathSegmentTree<T>>, starts_global: Option<SourceSpan>) -> PathTree<T>
where
    T: Ast,
{
    PathTree {
        segments,
        starts_global,
    }
}

pub(crate) fn id(line: u32, col: u32, name: &str) -> Expression {
    Expression::PathIn(PathInExpressionTree {
        underlying_path: PathTree {
            segments: vec![PathSegmentTree {
                ident: IdentSegment::Ident(IdentifierTree {
                    loc: test_span(
                        line,
                        col,
                        line,
                        col + TryInto::<u32>::try_into(name.len()).unwrap(),
                    ),
                    value: name.into(),
                }),
                data: None,
            }],
            starts_global: None,
        },
    })
}

pub(crate) fn unsigned_decimal_constant(line: u32, col: u32, value: usize) -> Expression {
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

pub(crate) fn add(lhs: Expression, rhs: Expression) -> Expression {
    Expression::Arithmetic(ArithmeticExpressionTree::Add(ArithmeticDualOperands::new(
        lhs, rhs,
    )))
}

pub(crate) fn mul(lhs: Expression, rhs: Expression) -> Expression {
    Expression::Arithmetic(ArithmeticExpressionTree::Multiply(
        ArithmeticDualOperands::new(lhs, rhs),
    ))
}

pub(crate) fn primitive_type(
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

pub(crate) fn named_type(start_line: u32, start_col: u32, name: &str) -> TypeTree {
    TypeTree::TypeNoBounds(TypeNoBoundsTree::TypePath(TypePath::ItemPath(
        TypeItemPathTree {
            underlying_path: PathTree {
                starts_global: None,
                segments: vec![PathSegmentTree {
                    ident: IdentSegment::Ident(IdentifierTree {
                        loc: test_span(
                            start_line,
                            start_col,
                            start_line,
                            start_col + TryInto::<u32>::try_into(name.len()).unwrap(),
                        ),
                        value: name.into(),
                    }),
                    data: None,
                }],
            },
        },
    )))
}

pub(crate) fn reference_of_type(
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
