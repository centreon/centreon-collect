//! The `cbmod` configuration file centreon-web generates for a poller
//! (`/etc/centreon-broker/<poller>-module.json`): the identity of the poller
//! and the address of the central broker, read as Broker reads them
//! (`broker/core/src/config/parser.cc`).

use std::path::Path;

use serde_json::Value;

/// What the scheduler takes from the `cbmod` file.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct BrokerConf {
    /// `uid` when set, else `poller_id`.
    pub poller_id: u64,
    /// `poller_name`.
    pub poller_name: String,
    /// `broker_name`.
    pub broker_name: String,
    /// `host:port` of the first TCP output, if any.
    pub address: Option<String>,
}

fn string_of(value: &Value) -> Option<String> {
    match value {
        Value::String(text) => Some(text.clone()),
        Value::Number(number) => Some(number.to_string()),
        _ => None,
    }
}

/// Parses the text of a `cbmod` file.
pub fn parse(text: &str) -> Result<BrokerConf, String> {
    let document: Value =
        serde_json::from_str(text).map_err(|error| format!("invalid JSON: {error}"))?;
    let root = document
        .get("centreonBroker")
        .ok_or("no centreonBroker object")?;
    let id = |key: &str| root.get(key).and_then(Value::as_u64);
    let poller_id = id("uid")
        .or_else(|| id("poller_id"))
        .filter(|id| *id > 0)
        .ok_or("no poller_id")?;
    let text_of = |key: &str| root.get(key).and_then(string_of).unwrap_or_default();
    let outputs: Vec<&Value> = match root.get("output") {
        Some(Value::Array(list)) => list.iter().collect(),
        // PHP writes an object when the keys are not 0..n.
        Some(Value::Object(map)) => map.values().collect(),
        Some(other) => vec![other],
        None => Vec::new(),
    };
    let address = outputs.iter().find_map(|output| {
        let kind = output.get("type").and_then(string_of)?;
        if !matches!(kind.as_str(), "ipv4" | "ipv6" | "tcp") {
            return None;
        }
        let host = output.get("host").and_then(string_of)?;
        let port = output.get("port").and_then(string_of)?;
        if host.is_empty() || port.is_empty() {
            return None;
        }
        Some(if host.contains(':') && !host.starts_with('[') {
            format!("[{host}]:{port}")
        } else {
            format!("{host}:{port}")
        })
    });
    Ok(BrokerConf {
        poller_id,
        poller_name: text_of("poller_name"),
        broker_name: text_of("broker_name"),
        address,
    })
}

/// Reads and parses the `cbmod` file at `path`.
pub fn read(path: &Path) -> Result<BrokerConf, String> {
    let text =
        std::fs::read_to_string(path).map_err(|error| format!("{}: {error}", path.display()))?;
    parse(&text).map_err(|error| format!("{}: {error}", path.display()))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn reads_what_web_generates() {
        let conf = parse(
            r#"{"centreonBroker":{"broker_id":3,"broker_name":"rust-poller-module",
            "poller_id":4,"poller_name":"rust-poller","bbdo_version":"3.0.1",
            "output":[{"name":"rrd","type":"lua","path":"x"},
                      {"name":"to-central","type":"ipv4","host":"broker","port":"5669",
                       "protocol":"bbdo","tls":"no","compression":"no"}]}}"#,
        )
        .unwrap();
        assert_eq!(
            conf,
            BrokerConf {
                poller_id: 4,
                poller_name: "rust-poller".into(),
                broker_name: "rust-poller-module".into(),
                address: Some("broker:5669".into()),
            }
        );
    }

    #[test]
    fn uid_wins_and_objects_are_lists() {
        let conf = parse(
            r#"{"centreonBroker":{"uid":77,"poller_id":4,
            "output":{"1":{"type":"tcp","host":"::1","port":5669}}}}"#,
        )
        .unwrap();
        assert_eq!(conf.poller_id, 77);
        assert_eq!(conf.address.as_deref(), Some("[::1]:5669"));
        assert!(parse(r#"{"centreonBroker":{"output":[]}}"#).is_err());
        assert!(parse("{").is_err());
        let grpc_only = parse(
            r#"{"centreonBroker":{"poller_id":2,"output":[{"type":"grpc","host":"b","port":"1"}]}}"#,
        )
        .unwrap();
        assert_eq!(grpc_only.address, None);
    }
}
