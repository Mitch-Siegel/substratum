pub use tracing::{self, span, trace, Level};

pub struct ExitOnDropSpan {
    entered_span: Option<tracing::span::EnteredSpan>,
}
impl From<tracing::span::EnteredSpan> for ExitOnDropSpan {
    fn from(entered_span: tracing::span::EnteredSpan) -> Self {
        Self {
            entered_span: Some(entered_span),
        }
    }
}
impl Drop for ExitOnDropSpan {
    fn drop(&mut self) {
        self.entered_span.take().unwrap().exit();
    }
}
