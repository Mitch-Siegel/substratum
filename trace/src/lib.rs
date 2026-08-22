#![allow(unused_imports, unused_macros)]

pub use tracing::Level;
pub(crate) use tracing_print::Print;

pub use tracing;
pub use tracing::event;
pub use tracing::instrument;
pub use tracing_subscriber as subscriber;

// TODO: maybe namespace this differently to avoid confusion over shadowing the tracing crate
// macros directly

pub struct ExitOnDropSpan {
    entered_span: Option<tracing::span::EnteredSpan>,
}

#[macro_export]
macro_rules! span_auto {
    ($lvl:expr, $name:expr, $($fields:tt)*) => {
        $crate::ExitOnDropSpan::from($crate::tracing::span!($lvl, $name, $($fields)*).entered())
    };
    ($lvl:expr, $name:expr) => {
        $crate::ExitOnDropSpan::from($crate::tracing::span!($lvl, $name).entered())
    };
}

#[macro_export]
macro_rules! span_auto_trace {
    ($name:expr, $($fields:tt)*) => {
        $crate::ExitOnDropSpan::from(tracing::span!($crate::Level::TRACE, $name, $($fields)*).entered())
    };
    ($name:expr) => {
        $crate::ExitOnDropSpan::from(tracing::span!($crate::Level::TRACE, $name).entered())
    };
}

#[macro_export]
macro_rules! span_auto_debug {
    ($name:expr, $($fields:tt)*) => {
        $crate::ExitOnDropSpan::from(tracing::span!($crate::Level::DEBUG, $name, $($fields)*).entered())
    };
    ($name:expr) => {
        $crate::ExitOnDropSpan::from(tracing::span!($crate::Level::DEBUG, $name).entered())
    };
}

pub use tracing::debug;
pub use tracing::error;
pub use tracing::info;
pub use tracing::trace;
pub use tracing::warn;

impl From<tracing::span::EnteredSpan> for ExitOnDropSpan {
    fn from(entered_span: tracing::span::EnteredSpan) -> Self {
        Self {
            entered_span: Some(entered_span),
        }
    }
}
impl From<tracing::span::Span> for ExitOnDropSpan {
    fn from(span: tracing::span::Span) -> Self {
        Self {
            entered_span: Some(span.entered()),
        }
    }
}
impl Drop for ExitOnDropSpan {
    fn drop(&mut self) {
        self.entered_span.take().unwrap().exit();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
}
