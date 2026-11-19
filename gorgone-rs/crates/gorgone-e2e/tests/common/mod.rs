//! Minimal HTTP/1.1 client for the tests: one request per connection, plain
//! TCP on loopback. Enough for the REST API, and no dependency.

// Each test binary uses its own part of this module.
#![allow(dead_code)]

use std::net::SocketAddr;

use serde_json::Value;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;

#[derive(Clone, Copy)]
pub struct Http {
    pub addr: SocketAddr,
}

impl Http {
    /// Sends one request and returns the status and the body.
    pub async fn request(
        &self,
        method: &str,
        path: &str,
        content_type: &str,
        body: &[u8],
    ) -> (u16, Vec<u8>) {
        let mut stream = TcpStream::connect(self.addr).await.unwrap();
        let head = format!(
            "{method} {path} HTTP/1.1\r\nhost: {}\r\nconnection: close\r\n\
             content-type: {content_type}\r\ncontent-length: {}\r\n\r\n",
            self.addr,
            body.len()
        );
        stream.set_nodelay(true).unwrap();
        // One write: two small ones would wait on the peer's delayed ACK.
        let mut request = head.into_bytes();
        request.extend_from_slice(body);
        stream.write_all(&request).await.unwrap();
        let mut raw = Vec::new();
        stream.read_to_end(&mut raw).await.unwrap();

        let split = raw
            .windows(4)
            .position(|w| w == b"\r\n\r\n")
            .expect("response head");
        let head = String::from_utf8_lossy(&raw[..split]).to_ascii_lowercase();
        let status = head
            .split_whitespace()
            .nth(1)
            .and_then(|code| code.parse().ok())
            .expect("status code");
        let payload = &raw[split + 4..];
        let body = if head.contains("transfer-encoding: chunked") {
            dechunk(payload)
        } else {
            payload.to_vec()
        };
        (status, body)
    }

    /// Sends a JSON request (or none) and decodes a JSON answer.
    pub async fn json(&self, method: &str, path: &str, body: Option<&Value>) -> Value {
        let bytes = body.map(|b| b.to_string().into_bytes()).unwrap_or_default();
        let (status, answer) = self.request(method, path, "application/json", &bytes).await;
        serde_json::from_slice(&answer).unwrap_or_else(|_| {
            panic!(
                "{method} {path}: status {status}, body {}",
                String::from_utf8_lossy(&answer)
            )
        })
    }
}

/// Persistent connection, for the benchmark: answers must carry a
/// `content-length`, which every JSON route does.
pub struct Conn {
    stream: TcpStream,
    addr: SocketAddr,
}

impl Conn {
    pub async fn open(addr: SocketAddr) -> Self {
        let stream = TcpStream::connect(addr).await.unwrap();
        stream.set_nodelay(true).unwrap();
        Self { stream, addr }
    }

    pub async fn json(&mut self, method: &str, path: &str, body: Option<&Value>) -> Value {
        let body = body.map(|b| b.to_string().into_bytes()).unwrap_or_default();
        let mut request = format!(
            "{method} {path} HTTP/1.1\r\nhost: {}\r\ncontent-type: application/json\r\n\
             content-length: {}\r\n\r\n",
            self.addr,
            body.len()
        )
        .into_bytes();
        request.extend_from_slice(&body);
        self.stream.write_all(&request).await.unwrap();

        let mut raw = Vec::new();
        let split = loop {
            let mut chunk = [0u8; 4096];
            let read = self.stream.read(&mut chunk).await.unwrap();
            assert!(read > 0, "connection closed");
            raw.extend_from_slice(&chunk[..read]);
            if let Some(split) = raw.windows(4).position(|w| w == b"\r\n\r\n") {
                break split;
            }
        };
        let head = String::from_utf8_lossy(&raw[..split]).to_ascii_lowercase();
        let length: usize = head
            .lines()
            .find_map(|line| line.strip_prefix("content-length:"))
            .and_then(|value| value.trim().parse().ok())
            .expect("content-length");
        let mut answer = raw.split_off(split + 4);
        while answer.len() < length {
            let mut chunk = [0u8; 4096];
            let read = self.stream.read(&mut chunk).await.unwrap();
            assert!(read > 0, "connection closed");
            answer.extend_from_slice(&chunk[..read]);
        }
        serde_json::from_slice(&answer).expect("JSON answer")
    }
}

fn dechunk(mut data: &[u8]) -> Vec<u8> {
    let mut out = Vec::new();
    while let Some(end) = data.windows(2).position(|w| w == b"\r\n") {
        let size = usize::from_str_radix(String::from_utf8_lossy(&data[..end]).trim(), 16)
            .expect("chunk size");
        if size == 0 {
            break;
        }
        let start = end + 2;
        out.extend_from_slice(&data[start..start + size]);
        data = &data[start + size + 2..];
    }
    out
}
