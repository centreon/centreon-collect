//! Error type of the server library.

/// Errors returned by `gorgone-server`.
#[derive(Debug, thiserror::Error)]
pub enum Error {
    /// Configuration could not be loaded or is inconsistent.
    #[error("configuration: {0}")]
    Config(String),
    /// Database failure.
    #[error("store: {0}")]
    Store(#[from] sqlx::Error),
    /// I/O failure, with what was being done.
    #[error("{context}: {source}")]
    Io {
        /// Operation or path involved.
        context: String,
        /// Underlying error.
        source: std::io::Error,
    },
    /// TLS material could not be loaded.
    #[error("tls: {0}")]
    Tls(String),
    /// gRPC transport failure.
    #[error("grpc transport: {0}")]
    Transport(#[from] tonic::transport::Error),
    /// Request or setting refused.
    #[error("invalid: {0}")]
    Invalid(String),
    /// The node is not enrolled.
    #[error("unknown node: {0}")]
    UnknownNode(String),
    /// A stored action could not be decoded.
    #[error("corrupt stored action: {0}")]
    Decode(#[from] prost::DecodeError),
}

impl Error {
    /// Builds an [`Error::Io`] closure carrying `context`.
    pub(crate) fn io(context: impl Into<String>) -> impl FnOnce(std::io::Error) -> Self {
        let context = context.into();
        move |source| Self::Io { context, source }
    }
}
