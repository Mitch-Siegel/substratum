use tracing::{self, debug, error, event, span, trace, warn, Level};

// TODO: maybe namespace this differently to avoid confusion over shadowing the tracing crate
// macros directly

pub struct ExitOnDropSpan {
    entered_span: Option<tracing::span::EnteredSpan>,
}

#[macro_export]
macro_rules! span_auto {
    (name: $name:expr, $($arg:tt)+) => ( ExitOnDropSpan::from(tracing::span!($name, $($arg)+).entered())
    )
}

#[macro_export]
macro_rules! trace {
    (name: $name:expr, $($arg:tt)+) => (tracing::trace!($name, $($arg)+))
}

#[macro_export]
macro_rules! debug {
    (name: $name:expr, $($arg:tt)+) => (tracing::debug!($name, $($arg)+))
}

#[macro_export]
macro_rules! event {
    (name: $name:expr, $($arg:tt)+) => (tracing::event!($name, $($arg)+))
}

#[macro_export]
macro_rules! warn {
    (name: $name:expr, $($arg:tt)+) => (tracing::warn!($name, $($arg)+))
}

#[macro_export]
macro_rules! error {
    (name: $name:expr, $($arg:tt)+) => (tracing::error!($name, $($arg)+))
}

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
