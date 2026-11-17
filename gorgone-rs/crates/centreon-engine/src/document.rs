//! The structured configuration document, version 1
//! (`gorgone-rs/schemas/engine-configuration.v1.json`): what centreon-web
//! sends instead of the `.cfg` files. Its objects become the raw objects of
//! the file reader, so that templates are resolved and objects checked by
//! the same code ([`cfg::build`]): both inputs give the same model.
//!
//! Every object refuses a field it does not know: a field the engine would
//! ignore is a behaviour it would silently not have (a check period, a
//! downtime...), so the whole document is refused and the configuration in
//! force stays.

use std::collections::{BTreeMap, HashMap};

use serde::Deserialize;

use crate::cfg::{self, Kind, MonitoringConfig, RawObject, Report, Settings};

/// Version of the document format this engine reads.
pub const VERSION: u32 = 1;

/// The whole document.
#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct Document {
    pub version: u32,
    pub poller: Poller,
    pub broker: Broker,
    #[serde(default)]
    pub settings: SettingsDoc,
    #[serde(default)]
    pub resource_macros: BTreeMap<String, String>,
    #[serde(default)]
    pub commands: Vec<CommandDoc>,
    #[serde(default)]
    pub host_templates: Vec<HostDoc>,
    #[serde(default)]
    pub hosts: Vec<HostDoc>,
    #[serde(default)]
    pub service_templates: Vec<ServiceDoc>,
    #[serde(default)]
    pub services: Vec<ServiceDoc>,
}

/// Identity of the poller: the BBDO instance.
#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct Poller {
    pub id: u64,
    pub name: String,
}

/// The central broker input the poller sends its events to.
#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct Broker {
    pub address: String,
    #[serde(default)]
    pub name: String,
}

/// Global settings; Engine's default for each one absent.
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct SettingsDoc {
    pub interval_length: Option<u32>,
    pub service_check_timeout: Option<u32>,
    pub host_check_timeout: Option<u32>,
    pub max_service_check_spread: Option<u32>,
    pub max_host_check_spread: Option<u32>,
    pub status_update_interval: Option<u32>,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct CommandDoc {
    pub command_name: String,
    pub command_line: String,
    pub connector: Option<String>,
}

/// Fields shared by hosts and services that schedule a check.
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct HostDoc {
    pub name: Option<String>,
    #[serde(default, rename = "use")]
    pub uses: Vec<String>,
    pub host_name: Option<String>,
    pub host_id: Option<u64>,
    pub alias: Option<String>,
    pub address: Option<String>,
    pub check_command: Option<String>,
    pub check_interval: Option<u32>,
    pub retry_interval: Option<u32>,
    pub max_check_attempts: Option<u32>,
    pub active_checks_enabled: Option<bool>,
    pub passive_checks_enabled: Option<bool>,
    #[serde(default)]
    pub macros: BTreeMap<String, String>,
}

#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct ServiceDoc {
    pub name: Option<String>,
    #[serde(default, rename = "use")]
    pub uses: Vec<String>,
    pub host_name: Option<String>,
    pub service_description: Option<String>,
    pub service_id: Option<u64>,
    pub check_command: Option<String>,
    pub check_interval: Option<u32>,
    pub retry_interval: Option<u32>,
    pub max_check_attempts: Option<u32>,
    pub active_checks_enabled: Option<bool>,
    pub passive_checks_enabled: Option<bool>,
    #[serde(default)]
    pub macros: BTreeMap<String, String>,
}

/// Only the version, read first so that a document of another version is
/// refused for that reason rather than for its unknown fields.
#[derive(Deserialize)]
struct Probe {
    version: Option<serde_json::Value>,
}

/// A document turned into the engine model.
#[derive(Debug, Clone)]
pub(crate) struct Parsed {
    pub config: MonitoringConfig,
    pub poller: Poller,
    pub broker: Broker,
}

/// Raw-object fields, named as the file reader names them.
#[derive(Default)]
struct Fields(BTreeMap<String, String>);

impl Fields {
    fn text(&mut self, key: &str, value: Option<&String>) {
        if let Some(value) = value {
            self.0.insert(key.to_owned(), value.clone());
        }
    }

    fn number(&mut self, key: &str, value: Option<impl ToString>) {
        if let Some(value) = value {
            self.0.insert(key.to_owned(), value.to_string());
        }
    }

    fn flag(&mut self, key: &str, value: Option<bool>) {
        if let Some(value) = value {
            self.0
                .insert(key.to_owned(), if value { "1" } else { "0" }.to_owned());
        }
    }
}

fn macros(map: &BTreeMap<String, String>) -> BTreeMap<String, String> {
    map.iter()
        .map(|(name, value)| (name.to_ascii_uppercase(), value.clone()))
        .collect()
}

fn host_object(host: &HostDoc, register: bool, origin: String) -> RawObject {
    let mut fields = Fields::default();
    fields.text("host_name", host.host_name.as_ref());
    fields.number("host_id", host.host_id);
    fields.text("alias", host.alias.as_ref());
    fields.text("address", host.address.as_ref());
    fields.text("check_command", host.check_command.as_ref());
    fields.number("check_interval", host.check_interval);
    fields.number("retry_interval", host.retry_interval);
    fields.number("max_check_attempts", host.max_check_attempts);
    fields.flag("checks_active", host.active_checks_enabled);
    fields.flag("checks_passive", host.passive_checks_enabled);
    RawObject {
        fields: fields.0,
        custom: macros(&host.macros),
        uses: host.uses.clone(),
        name: host.name.clone(),
        register,
        origin,
    }
}

fn service_object(service: &ServiceDoc, register: bool, origin: String) -> RawObject {
    let mut fields = Fields::default();
    fields.text("host_name", service.host_name.as_ref());
    fields.text("service_description", service.service_description.as_ref());
    fields.number("service_id", service.service_id);
    fields.text("check_command", service.check_command.as_ref());
    fields.number("check_interval", service.check_interval);
    fields.number("retry_interval", service.retry_interval);
    fields.number("max_check_attempts", service.max_check_attempts);
    fields.flag("checks_active", service.active_checks_enabled);
    fields.flag("checks_passive", service.passive_checks_enabled);
    RawObject {
        fields: fields.0,
        custom: macros(&service.macros),
        uses: service.uses.clone(),
        name: service.name.clone(),
        register,
        origin,
    }
}

/// Origin of an object in messages: its list, index and name.
fn origin(list: &str, index: usize, name: Option<&String>) -> String {
    match name {
        Some(name) => format!("{list}[{index}] '{name}'"),
        None => format!("{list}[{index}]"),
    }
}

/// True for `host:port` with a port from 1 to 65535.
fn valid_address(address: &str) -> bool {
    address.rsplit_once(':').is_some_and(|(host, port)| {
        !host.is_empty() && port.parse::<u16>().is_ok_and(|port| port > 0)
    })
}

/// Reads a document and checks it as the files are checked. `Err` holds the
/// report (`Total Errors:` last), refused as a whole.
pub(crate) fn parse(bytes: &[u8]) -> Result<(Parsed, Report), String> {
    let refuse = |error: String| {
        Err(Report {
            errors: vec![error],
            warnings: Vec::new(),
        }
        .render(None))
    };
    let probe: Probe = match serde_json::from_slice(bytes) {
        Ok(probe) => probe,
        Err(error) => return refuse(format!("invalid configuration document: {error}")),
    };
    match probe.version {
        Some(serde_json::Value::Number(number)) if number.as_u64() == Some(u64::from(VERSION)) => {}
        Some(other) => {
            return refuse(format!(
                "configuration document version {other} is not supported: this engine reads version {VERSION}"
            ));
        }
        None => return refuse("configuration document without version".to_owned()),
    }
    let document: Document = match serde_json::from_slice(bytes) {
        Ok(document) => document,
        Err(error) => return refuse(format!("invalid configuration document: {error}")),
    };
    if document.version != VERSION {
        return refuse(format!(
            "configuration document version {} is not supported",
            document.version
        ));
    }
    let (config, report) = model(&document);
    let mut report = report;
    if document.poller.id == 0 || document.poller.name.is_empty() {
        report
            .errors
            .push("poller: id must be at least 1 and name not empty".to_owned());
    }
    if !valid_address(&document.broker.address) {
        report.errors.push(format!(
            "broker.address {:?}: expected host:port",
            document.broker.address
        ));
    }
    let counts = (
        config.commands.len(),
        config.hosts.len(),
        config.services.len(),
    );
    if !report.errors.is_empty() {
        return Err(report.render(Some(counts)));
    }
    Ok((
        Parsed {
            config,
            poller: document.poller,
            broker: document.broker,
        },
        report,
    ))
}

/// The engine model of a document: raw objects, then the file reader's
/// template resolution and checks.
fn model(document: &Document) -> (MonitoringConfig, Report) {
    let mut report = Report::default();
    let defaults = Settings::default();
    let given = &document.settings;
    let settings = Settings {
        interval_length: given.interval_length.unwrap_or(defaults.interval_length),
        service_check_timeout: given
            .service_check_timeout
            .unwrap_or(defaults.service_check_timeout),
        host_check_timeout: given
            .host_check_timeout
            .unwrap_or(defaults.host_check_timeout),
        max_service_check_spread: given
            .max_service_check_spread
            .unwrap_or(defaults.max_service_check_spread),
        max_host_check_spread: given
            .max_host_check_spread
            .unwrap_or(defaults.max_host_check_spread),
        status_update_interval: given
            .status_update_interval
            .unwrap_or(defaults.status_update_interval),
        broker_module_cfg_file: None,
    };
    let mut users = HashMap::new();
    for (name, value) in &document.resource_macros {
        if name.is_empty() || name.contains('$') {
            report
                .errors
                .push(format!("resource_macros: invalid name '{name}'"));
            continue;
        }
        users.insert(name.clone(), value.clone());
    }
    let mut objects = Vec::new();
    for (index, command) in document.commands.iter().enumerate() {
        let mut fields = Fields::default();
        fields.text("command_name", Some(&command.command_name));
        fields.text("command_line", Some(&command.command_line));
        fields.text("connector", command.connector.as_ref());
        objects.push((
            Kind::Command,
            RawObject {
                fields: fields.0,
                register: true,
                origin: origin("commands", index, Some(&command.command_name)),
                ..RawObject::default()
            },
        ));
    }
    for (index, host) in document.host_templates.iter().enumerate() {
        if host.name.as_deref().is_none_or(str::is_empty) {
            report
                .errors
                .push(format!("host_templates[{index}]: a template needs a name"));
        }
        let at = origin("host_templates", index, host.name.as_ref());
        objects.push((Kind::Host, host_object(host, false, at)));
    }
    for (index, host) in document.hosts.iter().enumerate() {
        let at = origin("hosts", index, host.host_name.as_ref());
        if host.name.is_some() {
            report
                .errors
                .push(format!("{at}: name is for host_templates"));
        }
        objects.push((Kind::Host, host_object(host, true, at)));
    }
    for (index, service) in document.service_templates.iter().enumerate() {
        if service.name.as_deref().is_none_or(str::is_empty) {
            report.errors.push(format!(
                "service_templates[{index}]: a template needs a name"
            ));
        }
        let at = origin("service_templates", index, service.name.as_ref());
        objects.push((Kind::Service, service_object(service, false, at)));
    }
    for (index, service) in document.services.iter().enumerate() {
        let at = origin("services", index, service.service_description.as_ref());
        if service.name.is_some() {
            report
                .errors
                .push(format!("{at}: name is for service_templates"));
        }
        objects.push((Kind::Service, service_object(service, true, at)));
    }
    cfg::build(settings, users, objects, report)
}

#[cfg(test)]
pub(crate) mod tests {
    use std::path::{Path, PathBuf};

    use super::*;

    /// The objects of `cfg::tests::web_like`, as centreon-web sends them in
    /// a document.
    pub(crate) const WEB_LIKE: &str = r#"{
      "version": 1,
      "poller": {"id": 4, "name": "rust-poller"},
      "broker": {"address": "broker:5669", "name": "rust-poller-module"},
      "settings": {"interval_length": 60, "service_check_timeout": 20},
      "resource_macros": {"USER1": "/usr/lib/nagios/plugins", "CENTREONPLUGINS": "/usr/lib/centreon/plugins"},
      "commands": [{"command_name": "check_dummy", "command_line": "$USER1$/check_dummy $ARG1$ \"$ARG2$\""}],
      "host_templates": [
        {"name": "generic-host", "check_command": "check_dummy!0!up", "max_check_attempts": 2,
         "macros": {"SNMPCOMMUNITY": "public"}},
        {"name": "other", "max_check_attempts": 9, "check_interval": 7,
         "macros": {"snmpcommunity": "private", "EXTRA": "x"}}
      ],
      "hosts": [
        {"host_name": "h1", "alias": "First", "address": "10.0.0.1", "use": ["generic-host", "other"], "host_id": 12},
        {"host_name": "h2", "address": "10.0.0.2", "host_id": 13}
      ],
      "service_templates": [
        {"name": "svc-tpl", "service_description": "tpl", "check_command": "check_dummy!0!ok",
         "check_interval": 3, "retry_interval": 1, "max_check_attempts": 3}
      ],
      "services": [
        {"host_name": "h1", "service_description": "Ping", "use": ["svc-tpl"], "service_id": 101},
        {"host_name": "h2", "service_description": "Disk", "check_command": "check_dummy!2!full",
         "active_checks_enabled": false, "service_id": 102, "macros": {"CRIT": "90"}}
      ]
    }"#;

    fn without_sources(mut config: MonitoringConfig) -> MonitoringConfig {
        config.main_file = PathBuf::new();
        config.settings.broker_module_cfg_file = None;
        config
    }

    #[test]
    fn files_and_document_give_the_same_model() {
        let dir = tempfile::tempdir().unwrap();
        let (from_files, report) = cfg::read(&cfg::tests::web_like(dir.path()));
        assert!(report.errors.is_empty(), "{report:?}");
        let (parsed, report) = parse(WEB_LIKE.as_bytes()).unwrap();
        assert!(report.errors.is_empty(), "{report:?}");
        assert_eq!(without_sources(from_files), without_sources(parsed.config));
        assert_eq!(
            parsed.poller,
            Poller {
                id: 4,
                name: "rust-poller".into()
            }
        );
        assert_eq!(parsed.broker.address, "broker:5669");
    }

    #[test]
    fn refuses_what_the_files_refuse() {
        let document = WEB_LIKE
            .replace(
                r#""host_name": "h2", "service_description""#,
                r#""host_name": "nope", "service_description""#,
            )
            .replace(r#""use": ["svc-tpl"]"#, r#""use": ["missing"]"#);
        let report = parse(document.as_bytes()).unwrap_err();
        assert!(report.contains("'nope', which does not exist"), "{report}");
        assert!(report.contains("'missing': no such template"), "{report}");
        assert!(report.contains("services[0] 'Ping'"), "{report}");
        assert!(report.ends_with("Total Errors:   3\n"), "{report}");
    }

    #[test]
    fn refuses_unknown_fields_and_versions() {
        let unknown = WEB_LIKE.replace(
            r#""host_id": 13"#,
            r#""host_id": 13, "check_period": "24x7""#,
        );
        let report = parse(unknown.as_bytes()).unwrap_err();
        assert!(report.contains("unknown field `check_period`"), "{report}");
        assert!(report.contains("Total Errors:   1"), "{report}");

        let newer = unknown.replace(r#""version": 1"#, r#""version": 2"#);
        let report = parse(newer.as_bytes()).unwrap_err();
        assert!(report.contains("version 2 is not supported"), "{report}");

        let report = parse(b"{\"poller\": {}}").unwrap_err();
        assert!(report.contains("without version"), "{report}");
        let report = parse(b"[").unwrap_err();
        assert!(
            report.contains("invalid configuration document"),
            "{report}"
        );

        let address = WEB_LIKE.replace("broker:5669", "broker");
        let report = parse(address.as_bytes()).unwrap_err();
        assert!(report.contains("expected host:port"), "{report}");
        let interval = WEB_LIKE.replace(r#""interval_length": 60"#, r#""interval_length": 0"#);
        let report = parse(interval.as_bytes()).unwrap_err();
        assert!(
            report.contains("interval_length must be at least 1"),
            "{report}"
        );
    }

    #[test]
    fn the_schema_file_names_the_same_fields() {
        // The schema is the contract of centreon-web: same objects, same
        // field names as the model.
        let schema = Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("../../schemas/engine-configuration.v1.json");
        let schema: serde_json::Value =
            serde_json::from_str(&std::fs::read_to_string(schema).unwrap()).unwrap();
        let keys = |value: &serde_json::Value| -> Vec<String> {
            let mut keys: Vec<String> = value["properties"]
                .as_object()
                .unwrap()
                .keys()
                .cloned()
                .collect();
            keys.sort();
            keys
        };
        assert_eq!(
            keys(&schema),
            [
                "broker",
                "commands",
                "host_templates",
                "hosts",
                "poller",
                "resource_macros",
                "service_templates",
                "services",
                "settings",
                "version"
            ]
        );
        assert_eq!(schema["properties"]["version"]["const"], VERSION);
        // Every field the schema allows on a host is read by the model.
        let mut host = HostDoc::default();
        for key in keys(&schema["$defs"]["host"]) {
            let one = serde_json::json!({ key.clone(): sample(&schema["$defs"]["host"]["properties"][&key]) });
            host = serde_json::from_value(one).unwrap_or_else(|e| panic!("host.{key}: {e}"));
        }
        drop(host);
        for key in keys(&schema["$defs"]["service"]) {
            let one = serde_json::json!({ key.clone(): sample(&schema["$defs"]["service"]["properties"][&key]) });
            let _: ServiceDoc =
                serde_json::from_value(one).unwrap_or_else(|e| panic!("service.{key}: {e}"));
        }
        for key in keys(&schema["properties"]["settings"]) {
            let one = serde_json::json!({ key.clone(): 1 });
            let _: SettingsDoc =
                serde_json::from_value(one).unwrap_or_else(|e| panic!("settings.{key}: {e}"));
        }
    }

    /// A value of the type a schema property asks for.
    fn sample(property: &serde_json::Value) -> serde_json::Value {
        let kind = match property["$ref"].as_str() {
            Some("#/$defs/macros") => Some("object"),
            Some("#/$defs/uses") => Some("array"),
            Some(_) => None,
            None => property["type"].as_str(),
        };
        match kind {
            Some("string") => "x".into(),
            Some("boolean") => true.into(),
            Some("array") => serde_json::json!(["x"]),
            Some("object") => serde_json::json!({"K": "v"}),
            // `$ref` to an integer definition.
            _ => 1.into(),
        }
    }
}
