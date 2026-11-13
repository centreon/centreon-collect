//! Minimal HTTP/1.1 client of the `http-post:` service actions: one bodiless
//! `POST` per URL, of which only the status line is read. Plain text only, no
//! redirect followed. Written on `tokio::net` to keep an HTTP client crate
//! out of the agent.

use std::time::Duration;

use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;
use tonic::transport::Uri;

/// Bytes read at most while looking for the end of the status line.
const MAX_STATUS_LINE: usize = 8 * 1024;
/// Characters of a malformed status line quoted in an error.
const QUOTED_CHARS: usize = 120;

/// Destination of one `POST`, checked when the configuration is loaded.
#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) struct HttpTarget {
    /// URL as the administrator wrote it, for the reports.
    url: String,
    /// `host:port` to connect to.
    address: String,
    /// Value of the `Host` header.
    host: String,
    /// Path and query of the request line.
    request_target: String,
}

impl HttpTarget {
    /// Reads one `http://host[:port][/path]` URL; the error says what is wrong.
    pub(crate) fn parse(url: &str) -> Result<Self, String> {
        if url.chars().any(|c| c.is_control() || c.is_whitespace()) {
            return Err("URL holds a control character or a space".to_owned());
        }
        let scheme = url.split_once("://").map(|(scheme, _)| scheme);
        if scheme.is_some_and(|scheme| scheme.eq_ignore_ascii_case("https")) {
            return Err(format!(
                "{url}: TLS is out of scope for this action, only http:// URLs are called"
            ));
        }
        let expected = || format!("{url}: expected http://host[:port][/path]");
        if url.contains('#') {
            return Err(format!("{url}: a URL fragment is never sent"));
        }
        let uri = url.parse::<Uri>().map_err(|_| expected())?;
        let authority = uri.authority().ok_or_else(expected)?;
        if uri.scheme_str() != Some("http") || authority.host().is_empty() {
            return Err(expected());
        }
        if authority.as_str().contains('@') {
            return Err(format!("{url}: credentials in the URL are not supported"));
        }
        // Read here: the URL parser lets an empty or oversized port through.
        let port = match authority.as_str().strip_prefix(authority.host()) {
            Some("") => 80,
            Some(port) => match port.strip_prefix(':').map(str::parse::<u16>) {
                Some(Ok(port)) if port != 0 => port,
                _ => return Err(expected()),
            },
            None => return Err(expected()),
        };
        let path = match uri.path() {
            "" => "/",
            path => path,
        };
        let request_target = match uri.query() {
            Some(query) => format!("{path}?{query}"),
            None => path.to_owned(),
        };
        Ok(Self {
            url: url.to_owned(),
            // An IPv6 host keeps its brackets, as the resolver expects.
            address: format!("{}:{port}", authority.host()),
            host: authority.as_str().to_owned(),
            request_target,
        })
    }

    /// Reads a list of URLs separated by white space; an empty list is an error.
    pub(crate) fn parse_list(list: &str) -> Result<Vec<Self>, String> {
        let targets = list
            .split_whitespace()
            .map(Self::parse)
            .collect::<Result<Vec<_>, _>>()?;
        if targets.is_empty() {
            return Err("at least one http:// URL is expected".to_owned());
        }
        Ok(targets)
    }

    /// URL as written in the configuration.
    pub(crate) fn url(&self) -> &str {
        &self.url
    }
}

/// First line of an HTTP answer.
#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) struct StatusLine {
    /// The line as received, without its line break.
    pub line: String,
    /// Its status code.
    pub code: u16,
}

impl StatusLine {
    /// True for a 2xx code. A redirect is a refusal: it is never followed.
    pub(crate) fn accepted(&self) -> bool {
        (200..300).contains(&self.code)
    }

    fn parse(raw: &[u8]) -> Result<Self, String> {
        let malformed = || {
            let text = String::from_utf8_lossy(raw);
            let quoted: String = text.chars().take(QUOTED_CHARS).collect();
            format!("malformed status line {quoted:?}")
        };
        let line = std::str::from_utf8(raw).map_err(|_| malformed())?;
        let line = line.strip_suffix('\r').unwrap_or(line);
        // The line ends up in a job event: nothing but printable text.
        if line.chars().any(char::is_control) {
            return Err(malformed());
        }
        let mut parts = line.splitn(3, ' ');
        let version = parts.next().unwrap_or_default();
        let code = parts.next().unwrap_or_default();
        let minor = version.strip_prefix("HTTP/1.").ok_or_else(malformed)?;
        let known_version = minor.len() == 1 && minor.bytes().all(|b| b.is_ascii_digit());
        if !known_version || code.len() != 3 || !code.bytes().all(|b| b.is_ascii_digit()) {
            return Err(malformed());
        }
        Ok(Self {
            line: line.to_owned(),
            code: code.parse().map_err(|_| malformed())?,
        })
    }
}

/// Why a call got no status line.
#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) enum PostError {
    /// Nothing usable within the time given, connection included.
    Timeout(Duration),
    /// Connection, transfer or protocol error, ready to report.
    Failed(String),
}

impl std::fmt::Display for PostError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Timeout(limit) => write!(f, "no answer within {} ms", limit.as_millis()),
            Self::Failed(reason) => f.write_str(reason),
        }
    }
}

/// Sends `POST` without a body to `target` and returns the status line of
/// the answer, whatever its code. `timeout` bounds the whole exchange.
pub(crate) async fn post(target: &HttpTarget, timeout: Duration) -> Result<StatusLine, PostError> {
    match tokio::time::timeout(timeout, exchange(target)).await {
        Ok(answer) => answer.map_err(PostError::Failed),
        Err(_) => Err(PostError::Timeout(timeout)),
    }
}

async fn exchange(target: &HttpTarget) -> Result<StatusLine, String> {
    let mut stream = TcpStream::connect(target.address.as_str())
        .await
        .map_err(|error| format!("connection to {}: {error}", target.address))?;
    let request = format!(
        "POST {} HTTP/1.1\r\nHost: {}\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
        target.request_target, target.host
    );
    stream
        .write_all(request.as_bytes())
        .await
        .map_err(|error| format!("sending the request: {error}"))?;

    let mut received = Vec::with_capacity(256);
    let mut chunk = [0_u8; 512];
    loop {
        if let Some(end) = received.iter().position(|byte| *byte == b'\n') {
            // Headers and body are of no use: the code alone decides.
            return StatusLine::parse(&received[..end]);
        }
        let room = MAX_STATUS_LINE.saturating_sub(received.len());
        if room == 0 {
            return Err(format!(
                "no status line in the first {MAX_STATUS_LINE} bytes of the answer"
            ));
        }
        let wanted = room.min(chunk.len());
        let read = stream
            .read(&mut chunk[..wanted])
            .await
            .map_err(|error| format!("reading the answer: {error}"))?;
        if read == 0 {
            return if received.is_empty() {
                Err("connection closed without an answer".to_owned())
            } else {
                StatusLine::parse(&received)
            };
        }
        received.extend_from_slice(&chunk[..read]);
    }
}

#[cfg(test)]
pub(crate) mod testing {
    use std::net::SocketAddr;
    use std::sync::{Arc, Mutex};

    use tokio::io::{AsyncReadExt, AsyncWriteExt};
    use tokio::net::TcpListener;

    /// HTTP server answering every connection with the same bytes, or with
    /// nothing at all while keeping the connection open.
    pub(crate) struct FakeHttp {
        pub addr: SocketAddr,
        requests: Arc<Mutex<Vec<String>>>,
        task: tokio::task::JoinHandle<()>,
    }

    impl FakeHttp {
        /// `answer` of `None`: the server reads the request and stays silent.
        pub(crate) async fn start(answer: Option<&'static [u8]>) -> Self {
            let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
            let addr = listener.local_addr().unwrap();
            let requests = Arc::new(Mutex::new(Vec::new()));
            let seen = requests.clone();
            let task = tokio::spawn(async move {
                let mut silent = Vec::new();
                loop {
                    let (mut stream, _) = listener.accept().await.unwrap();
                    let mut request = Vec::new();
                    let mut chunk = [0_u8; 1024];
                    while !request.ends_with(b"\r\n\r\n") {
                        let read = stream.read(&mut chunk).await.unwrap();
                        if read == 0 {
                            break;
                        }
                        request.extend_from_slice(&chunk[..read]);
                    }
                    seen.lock()
                        .unwrap()
                        .push(String::from_utf8(request).unwrap());
                    match answer {
                        Some(answer) => {
                            // The client may already be gone.
                            let _ = stream.write_all(answer).await;
                        }
                        None => silent.push(stream),
                    }
                }
            });
            Self {
                addr,
                requests,
                task,
            }
        }

        pub(crate) fn url(&self, path: &str) -> String {
            format!("http://{}{path}", self.addr)
        }

        /// Requests received so far, head only.
        pub(crate) fn requests(&self) -> Vec<String> {
            self.requests.lock().unwrap().clone()
        }
    }

    impl Drop for FakeHttp {
        fn drop(&mut self) {
            self.task.abort();
        }
    }

    /// URL of a local port nothing listens on.
    pub(crate) async fn refused_url(path: &str) -> String {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let addr = listener.local_addr().unwrap();
        drop(listener);
        format!("http://{addr}{path}")
    }
}

#[cfg(test)]
mod tests {
    use super::testing::{FakeHttp, refused_url};
    use super::*;

    const LIMIT: Duration = Duration::from_secs(5);

    async fn post_to(url: &str, timeout: Duration) -> Result<StatusLine, PostError> {
        post(&HttpTarget::parse(url).unwrap(), timeout).await
    }

    fn failure(answer: Result<StatusLine, PostError>) -> String {
        match answer {
            Err(PostError::Failed(reason)) => reason,
            other => panic!("unexpected {other:?}"),
        }
    }

    #[test]
    fn urls_are_split_into_what_the_request_needs() {
        for (url, address, host, request_target) in [
            (
                "http://broker:8080/reload",
                "broker:8080",
                "broker:8080",
                "/reload",
            ),
            ("http://broker", "broker:80", "broker", "/"),
            ("http://broker/", "broker:80", "broker", "/"),
            ("HTTP://Broker:8080", "Broker:8080", "Broker:8080", "/"),
            (
                "http://10.0.0.7:8080/a/b?x=1",
                "10.0.0.7:8080",
                "10.0.0.7:8080",
                "/a/b?x=1",
            ),
            (
                "http://[::1]:8080/reload",
                "[::1]:8080",
                "[::1]:8080",
                "/reload",
            ),
            ("http://[::1]/reload", "[::1]:80", "[::1]", "/reload"),
        ] {
            let target = HttpTarget::parse(url).unwrap();
            assert_eq!(target.url(), url);
            assert_eq!(target.address, address, "{url}");
            assert_eq!(target.host, host, "{url}");
            assert_eq!(target.request_target, request_target, "{url}");
        }
    }

    #[test]
    fn bad_urls_are_refused() {
        for (url, needle) in [
            ("https://broker:8080/reload", "TLS is out of scope"),
            ("HTTPS://broker/reload", "TLS is out of scope"),
            ("broker:8080/reload", "expected http://host"),
            ("/reload", "expected http://host"),
            ("ftp://broker/reload", "expected http://host"),
            ("http://", "expected http://host"),
            ("http:///reload", "expected http://host"),
            ("http://:8080/reload", "expected http://host"),
            ("http://broker:/reload", "expected http://host"),
            ("http://broker:0/reload", "expected http://host"),
            ("http://broker:99999/reload", "expected http://host"),
            ("http://broker:port/reload", "expected http://host"),
            ("http://user:secret@broker/reload", "credentials"),
            ("http://user@broker/reload", "credentials"),
            ("http://broker/reload#now", "fragment"),
            ("http://broker/re\rload", "control character"),
            ("http://broker/re\nload", "control character"),
            ("http://broker/re load", "control character"),
            ("http://broker/\u{0}", "control character"),
            ("", "expected http://host"),
        ] {
            let error = HttpTarget::parse(url).unwrap_err();
            assert!(error.contains(needle), "{url:?}: {error}");
        }
    }

    #[test]
    fn url_lists_are_split_on_white_space_and_never_empty() {
        let targets = HttpTarget::parse_list(" http://a:8080/reload\thttp://b/reload ").unwrap();
        let urls: Vec<&str> = targets.iter().map(HttpTarget::url).collect();
        assert_eq!(urls, ["http://a:8080/reload", "http://b/reload"]);
        for (list, needle) in [
            ("", "at least one"),
            ("  ", "at least one"),
            ("http://a/reload https://b/reload", "TLS is out of scope"),
            (
                "http://a/reload http://b:99999/reload",
                "expected http://host",
            ),
        ] {
            let error = HttpTarget::parse_list(list).unwrap_err();
            assert!(error.contains(needle), "{list:?}: {error}");
        }
    }

    #[test]
    fn status_lines_are_read_strictly() {
        for (raw, code) in [
            ("HTTP/1.1 200 OK\r", 200),
            ("HTTP/1.1 204 No Content", 204),
            ("HTTP/1.0 200", 200),
            ("HTTP/1.1 503 Service Unavailable\r", 503),
        ] {
            let status = StatusLine::parse(raw.as_bytes()).unwrap();
            assert_eq!(status.code, code);
            assert_eq!(status.line, raw.trim_end_matches('\r'));
        }
        for raw in [
            &b""[..],
            b"\r",
            b"200 OK",
            b"HTTP/2 200",
            b"HTTP/1.1 20 OK",
            b"HTTP/1.1 2000 OK",
            b"HTTP/1.1 2x0 OK",
            b"HTTP/1.1  200 OK",
            b"HTTP/1.x 200 OK",
            b"HTTP/1.1 200 OK\x1b[31m",
            b"HTTP/1.1 200 \xff",
            b"SSH-2.0-OpenSSH_9.6",
        ] {
            let error = StatusLine::parse(raw).unwrap_err();
            assert!(error.starts_with("malformed status line"), "{error}");
            assert!(!error.contains('\u{1b}'), "{error:?}");
        }
        let long = StatusLine::parse("x".repeat(4000).as_bytes()).unwrap_err();
        assert!(long.len() < 200, "{}", long.len());
    }

    #[tokio::test]
    async fn posts_without_a_body_and_reads_the_status_line() {
        let server = FakeHttp::start(Some(b"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok")).await;
        let status = post_to(&server.url("/reload?now=1"), LIMIT).await.unwrap();
        assert!(status.accepted());
        assert_eq!(status.line, "HTTP/1.1 200 OK");
        assert_eq!(
            server.requests(),
            [format!(
                "POST /reload?now=1 HTTP/1.1\r\nHost: {}\r\nContent-Length: 0\r\n\
                 Connection: close\r\n\r\n",
                server.addr
            )]
        );
    }

    #[tokio::test]
    async fn other_codes_are_answers_that_are_not_accepted() {
        for (answer, code) in [
            (&b"HTTP/1.1 204 No Content\r\n\r\n"[..], 204),
            (
                b"HTTP/1.1 302 Found\r\nLocation: http://elsewhere/\r\n\r\n",
                302,
            ),
            (b"HTTP/1.1 404 Not Found\r\n\r\n", 404),
            (b"HTTP/1.1 500 Internal Server Error\n\n", 500),
        ] {
            let server = FakeHttp::start(Some(answer)).await;
            let status = post_to(&server.url("/reload"), LIMIT).await.unwrap();
            assert_eq!(status.code, code);
            assert_eq!(status.accepted(), code == 204);
            // A redirect is not followed.
            assert_eq!(server.requests().len(), 1);
        }
    }

    #[tokio::test]
    async fn connection_refused_is_reported() {
        let url = refused_url("/reload").await;
        let reason = failure(post_to(&url, LIMIT).await);
        assert!(reason.starts_with("connection to 127.0.0.1:"), "{reason}");
    }

    #[tokio::test]
    async fn silent_server_times_out() {
        let server = FakeHttp::start(None).await;
        let limit = Duration::from_millis(200);
        let started = std::time::Instant::now();
        let answer = post_to(&server.url("/reload"), limit).await;
        assert_eq!(answer, Err(PostError::Timeout(limit)));
        assert_eq!(answer.unwrap_err().to_string(), "no answer within 200 ms");
        assert!(started.elapsed() < LIMIT);
        assert_eq!(server.requests().len(), 1);
    }

    #[tokio::test]
    async fn malformed_answers_are_reported() {
        for (answer, needle) in [
            (&b"SSH-2.0-OpenSSH_9.6\r\n"[..], "malformed status line"),
            (b"HTTP/1.1 OK\r\n\r\n", "malformed status line"),
            (b"HTTP/1.1 200", "HTTP/1.1 200"),
            (b"", "closed without an answer"),
        ] {
            let server = FakeHttp::start(Some(answer)).await;
            match post_to(&server.url("/reload"), LIMIT).await {
                // A complete status line cut before its line break still counts.
                Ok(status) => assert_eq!(status.line, needle),
                Err(error) => assert!(error.to_string().contains(needle), "{error}"),
            }
        }
    }

    #[tokio::test]
    async fn status_line_search_is_bounded() {
        static ENDLESS: [u8; 64 * 1024] = [b'x'; 64 * 1024];
        let server = FakeHttp::start(Some(&ENDLESS)).await;
        let reason = failure(post_to(&server.url("/reload"), LIMIT).await);
        assert!(reason.contains("first 8192 bytes"), "{reason}");
    }
}
