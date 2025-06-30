#![allow(unused_imports, unused_macros)]

use tracing;
pub use tracing::Level;
pub use tracing_print::Print;

// TODO: maybe namespace this differently to avoid confusion over shadowing the tracing crate
// macros directly

pub struct ExitOnDropSpan {
    entered_span: Option<tracing::span::EnteredSpan>,
}

macro_rules! span_auto {
    ($lvl:expr, $name:expr, $($fields:tt)*) => {
        $crate::trace::ExitOnDropSpan::from(tracing::span!($lvl, $name, $($fields)*).entered())
    };
    ($lvl:expr, $name:expr) => {
        $crate::trace::ExitOnDropSpan::from(tracing::span!($lvl, $name).entered())
    };
}

macro_rules! span_auto_trace {
    ($name:expr, $($fields:tt)*) => {
        $crate::trace::ExitOnDropSpan::from(tracing::span!($crate::trace::Level::TRACE, $name, $($fields)*).entered())
    };
    ($name:expr) => {
        $crate::trace::ExitOnDropSpan::from(tracing::span!($crate::trace::Level::TRACE, $name).entered())
    };
}

macro_rules! span_auto_debug {
    ($name:expr, $($fields:tt)*) => {
        $crate::trace::ExitOnDropSpan::from(tracing::span!($crate::trace::Level::DEBUG, $name, $($fields)*).entered())
    };
    ($name:expr) => {
        $crate::trace::ExitOnDropSpan::from(tracing::span!($crate::trace::Level::DEBUG, $name).entered())
    };
}

pub(crate) use span_auto;
pub(crate) use span_auto_debug;
pub(crate) use span_auto_trace;

macro_rules! trace {
    ($name:expr, $($arg:tt)*) => (tracing::trace!($name, $($arg)*));
    ($name:expr) => (tracing::trace!($name))
}
pub(crate) use trace;

macro_rules! debug {
    ($name:expr, $($arg:tt)*) => (tracing::debug!($name, $($arg)*));
    ($name:expr) => (tracing::debug!($name))
}
pub(crate) use debug;

macro_rules! event {
    ($name:expr, $($arg:tt)*) => (tracing::event!($name, $($arg)*));
    ($name:expr) => (tracing::event!($name))
}
pub(crate) use event;

macro_rules! warning {
    ($name:expr, $($arg:tt)*) => (tracing::warn!($name, $($arg)*));
    ($name:expr) => (tracing::warn!($name))
}
pub(crate) use warning;

macro_rules! error {
    ($name:expr, $($arg:tt)*) => (tracing::error!($name, $($arg)*));
    ($name:expr) => (tracing::error!($name))
}
pub(crate) use error;

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
