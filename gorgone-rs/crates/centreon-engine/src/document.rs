//! The structured configuration document, versions 1 and 2
//! (`gorgone-rs/schemas/engine-configuration.v2.json`): what centreon-web
//! sends instead of the `.cfg` files. Its objects become the raw objects of
//! the file reader, so that templates are resolved and objects checked by
//! the same code ([`cfg::build`]): both inputs give the same model.
//!
//! Version 2 adds timeperiods, check periods, time zones, recurring
//! downtimes and retention settings, and changes the meaning of macro
//! values: `encrypt::` and `raw::` are read as Engine reads them in its
//! files (decrypted, stripped). A version 1 document may not use any of
//! it, and its values stay literal.
//!
//! Every object refuses a field it does not know: a field the engine would
//! ignore is a behaviour it would silently not have (a check period, a
//! downtime...), so the whole document is refused and the configuration in
//! force stays.

use std::collections::{BTreeMap, HashMap};

use serde::Deserialize;

use crate::cfg::{self, Kind, MonitoringConfig, RawObject, Report, Settings};

/// Latest version of the document format this engine reads; it also
/// reads the previous ones.
pub const VERSION: u32 = 2;

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
    #[serde(default)]
    pub timeperiods: Vec<TimeperiodDoc>,
    #[serde(default)]
    pub recurring_downtimes: Vec<RecurringDoc>,
}

/// A timeperiod; it is also the template of those that `use` it.
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct TimeperiodDoc {
    pub timeperiod_name: String,
    pub alias: Option<String>,
    #[serde(default, rename = "use")]
    pub uses: Vec<String>,
    pub sunday: Option<String>,
    pub monday: Option<String>,
    pub tuesday: Option<String>,
    pub wednesday: Option<String>,
    pub thursday: Option<String>,
    pub friday: Option<String>,
    pub saturday: Option<String>,
    #[serde(default)]
    pub exceptions: Vec<ExceptionDoc>,
    #[serde(default)]
    pub exclude: Vec<String>,
}

/// One exception of a timeperiod, as centreon-web stores it.
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct ExceptionDoc {
    pub days: String,
    pub timerange: String,
}

/// A recurring downtime (`recurringdowntime`).
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct RecurringDoc {
    pub downtime_id: u64,
    pub downtime_name: Option<String>,
    pub timezone: Option<String>,
    #[serde(default)]
    pub host_ids: Vec<u64>,
    #[serde(default)]
    pub service_ids: Vec<(u64, u64)>,
    #[serde(default)]
    pub periods: Vec<String>,
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
    pub use_timezone: Option<String>,
    pub retention_update_interval: Option<u32>,
    pub use_retained_scheduling_info: Option<bool>,
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
    pub check_period: Option<String>,
    pub timezone: Option<String>,
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
    pub check_period: Option<String>,
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
    fields.text("check_period", host.check_period.as_ref());
    fields.text("timezone", host.timezone.as_ref());
    RawObject {
        fields: fields.0,
        custom: macros(&host.macros),
        uses: host.uses.clone(),
        name: host.name.clone(),
        register,
        origin,
        ..RawObject::default()
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
    fields.text("check_period", service.check_period.as_ref());
    RawObject {
        fields: fields.0,
        custom: macros(&service.macros),
        uses: service.uses.clone(),
        name: service.name.clone(),
        register,
        origin,
        ..RawObject::default()
    }
}

fn timeperiod_object(period: &TimeperiodDoc, origin: String) -> RawObject {
    let mut fields = Fields::default();
    fields.text("timeperiod_name", Some(&period.timeperiod_name));
    fields.text("alias", period.alias.as_ref());
    if !period.exclude.is_empty() {
        fields
            .0
            .insert("exclude".to_owned(), period.exclude.join(","));
    }
    let days = [
        ("sunday", &period.sunday),
        ("monday", &period.monday),
        ("tuesday", &period.tuesday),
        ("wednesday", &period.wednesday),
        ("thursday", &period.thursday),
        ("friday", &period.friday),
        ("saturday", &period.saturday),
    ];
    let mut lines: Vec<(String, String)> = days
        .iter()
        .filter_map(|(day, ranges)| ranges.as_ref().map(|r| ((*day).to_owned(), r.clone())))
        .collect();
    // The line the .cfg generator writes: the days, then the ranges.
    for exception in &period.exceptions {
        let line = format!("{} {}", exception.days.trim(), exception.timerange.trim());
        let (key, value) = line.split_once(' ').unwrap_or((line.as_str(), ""));
        lines.push((key.to_owned(), value.trim_start().to_owned()));
    }
    RawObject {
        fields: fields.0,
        uses: period.uses.clone(),
        // centreon-web names each timeperiod as a template of its name.
        name: Some(period.timeperiod_name.clone()),
        register: true,
        origin,
        lines,
        ..RawObject::default()
    }
}

fn recurring_object(rule: &RecurringDoc, origin: String) -> RawObject {
    let mut fields = Fields::default();
    fields.number("downtime_id", Some(rule.downtime_id));
    fields.text("downtime_name", rule.downtime_name.as_ref());
    fields.text("timezone", rule.timezone.as_ref());
    let join = |ids: Vec<String>| (!ids.is_empty()).then(|| ids.join(","));
    fields.text(
        "host_ids",
        join(rule.host_ids.iter().map(u64::to_string).collect()).as_ref(),
    );
    fields.text(
        "service_ids",
        join(
            rule.service_ids
                .iter()
                .map(|(host, service)| format!("{host}:{service}"))
                .collect(),
        )
        .as_ref(),
    );
    RawObject {
        fields: fields.0,
        register: true,
        origin,
        lines: rule
            .periods
            .iter()
            .map(|period| ("period".to_owned(), period.clone()))
            .collect(),
        ..RawObject::default()
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
        Some(serde_json::Value::Number(number))
            if number
                .as_u64()
                .is_some_and(|v| (1..=u64::from(VERSION)).contains(&v)) => {}
        Some(other) => {
            return refuse(format!(
                "configuration document version {other} is not supported: this engine reads \
                 versions 1 to {VERSION}"
            ));
        }
        None => return refuse("configuration document without version".to_owned()),
    }
    let document: Document = match serde_json::from_slice(bytes) {
        Ok(document) => document,
        Err(error) => return refuse(format!("invalid configuration document: {error}")),
    };
    if document.version < 2
        && let Some(field) = version_2_field(&document)
    {
        return refuse(format!(
            "{field} needs version 2 of the configuration document"
        ));
    }
    let (mut config, report) = model(&document);
    // Version 1 values are literal: centreon-web decrypted them.
    config.secret_prefixes = document.version >= 2;
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

/// The first version 2 field a document uses, if any.
fn version_2_field(document: &Document) -> Option<&'static str> {
    let settings = &document.settings;
    if settings.use_timezone.is_some()
        || settings.retention_update_interval.is_some()
        || settings.use_retained_scheduling_info.is_some()
    {
        return Some(
            "settings.use_timezone, retention_update_interval or use_retained_scheduling_info",
        );
    }
    if !document.timeperiods.is_empty() {
        return Some("timeperiods");
    }
    if !document.recurring_downtimes.is_empty() {
        return Some("recurring_downtimes");
    }
    let hosts = document.host_templates.iter().chain(&document.hosts);
    if hosts
        .into_iter()
        .any(|h| h.check_period.is_some() || h.timezone.is_some())
    {
        return Some("check_period or timezone of a host");
    }
    let services = document.service_templates.iter().chain(&document.services);
    if services.into_iter().any(|s| s.check_period.is_some()) {
        return Some("check_period of a service");
    }
    None
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
        use_timezone: given.use_timezone.clone().unwrap_or_default(),
        retention_update_interval: given
            .retention_update_interval
            .unwrap_or(defaults.retention_update_interval),
        use_retained_scheduling_info: given
            .use_retained_scheduling_info
            .unwrap_or(defaults.use_retained_scheduling_info),
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
    for (index, period) in document.timeperiods.iter().enumerate() {
        let at = origin("timeperiods", index, Some(&period.timeperiod_name));
        objects.push((Kind::Timeperiod, timeperiod_object(period, at)));
    }
    for (index, rule) in document.recurring_downtimes.iter().enumerate() {
        let at = format!("recurring_downtimes[{index}] #{}", rule.downtime_id);
        objects.push((Kind::RecurringDowntime, recurring_object(rule, at)));
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
      "version": 2,
      "poller": {"id": 4, "name": "rust-poller"},
      "broker": {"address": "broker:5669", "name": "rust-poller-module"},
      "settings": {"interval_length": 60, "service_check_timeout": 20, "retention_update_interval": 30},
      "resource_macros": {"USER1": "/usr/lib/nagios/plugins", "CENTREONPLUGINS": "/usr/lib/centreon/plugins"},
      "commands": [{"command_name": "check_dummy", "command_line": "$USER1$/check_dummy $ARG1$ \"$ARG2$\""}],
      "host_templates": [
        {"name": "generic-host", "check_command": "check_dummy!0!up", "max_check_attempts": 2,
         "macros": {"SNMPCOMMUNITY": "public"}},
        {"name": "other", "max_check_attempts": 9, "check_interval": 7,
         "macros": {"snmpcommunity": "private", "EXTRA": "x"}}
      ],
      "hosts": [
        {"host_name": "h1", "alias": "First", "address": "10.0.0.1", "use": ["generic-host", "other"], "host_id": 12,
         "check_period": "workhours", "timezone": ":Europe/Paris"},
        {"host_name": "h2", "address": "10.0.0.2", "host_id": 13}
      ],
      "service_templates": [
        {"name": "svc-tpl", "service_description": "tpl", "check_command": "check_dummy!0!ok",
         "check_interval": 3, "retry_interval": 1, "max_check_attempts": 3}
      ],
      "services": [
        {"host_name": "h1", "service_description": "Ping", "use": ["svc-tpl"], "service_id": 101},
        {"host_name": "h2", "service_description": "Disk", "check_command": "check_dummy!2!full",
         "active_checks_enabled": false, "service_id": 102, "macros": {"CRIT": "90"},
         "check_period": "24x7"}
      ],
      "timeperiods": [
        {"timeperiod_name": "24x7", "alias": "Always", "sunday": "00:00-24:00", "monday": "00:00-24:00"},
        {"timeperiod_name": "workhours", "alias": "Work", "use": ["24x7"], "monday": "09:00-17:00",
         "exceptions": [{"days": "december 25", "timerange": "00:00-24:00"}], "exclude": ["nonworkhours"]},
        {"timeperiod_name": "nonworkhours", "alias": "Nights",
         "exceptions": [{"days": "2026-12-24 - 2026-12-26", "timerange": "00:00-24:00"}]}
      ],
      "recurring_downtimes": [
        {"downtime_id": 5, "downtime_name": "maintenance", "host_ids": [12], "service_ids": [[13, 102]],
         "periods": ["weekly:1,2 22:00-23:00 fixed", "monthly:1 01:00-02:00 flexible:600"]}
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
            r#""host_id": 13, "notification_period": "24x7""#,
        );
        let report = parse(unknown.as_bytes()).unwrap_err();
        assert!(
            report.contains("unknown field `notification_period`"),
            "{report}"
        );
        assert!(report.contains("Total Errors:   1"), "{report}");

        let newer = unknown.replace(r#""version": 2"#, r#""version": 3"#);
        let report = parse(newer.as_bytes()).unwrap_err();
        assert!(report.contains("version 3 is not supported"), "{report}");

        // Version 1 takes no version 2 field, and keeps its values literal.
        let old = WEB_LIKE.replace(r#""version": 2"#, r#""version": 1"#);
        let report = parse(old.as_bytes()).unwrap_err();
        assert!(report.contains("needs version 2"), "{report}");
        let v1 =
            r#"{"version": 1, "poller": {"id": 1, "name": "p"}, "broker": {"address": "b:1"}}"#;
        let (parsed, _) = parse(v1.as_bytes()).unwrap();
        assert!(!parsed.config.secret_prefixes);

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
            .join("../../schemas/engine-configuration.v2.json");
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
                "recurring_downtimes",
                "resource_macros",
                "service_templates",
                "services",
                "settings",
                "timeperiods",
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
            let one = serde_json::json!({ key.clone(): sample(&schema["properties"]["settings"]["properties"][&key]) });
            let _: SettingsDoc =
                serde_json::from_value(one).unwrap_or_else(|e| panic!("settings.{key}: {e}"));
        }
        for (def, required) in [
            ("timeperiod", serde_json::json!({"timeperiod_name": "t"})),
            ("recurring_downtime", serde_json::json!({"downtime_id": 1})),
        ] {
            for key in keys(&schema["$defs"][def]) {
                let mut one = required.clone();
                one[&key] = sample(&schema["$defs"][def]["properties"][&key]);
                if def == "timeperiod" {
                    let _: TimeperiodDoc =
                        serde_json::from_value(one).unwrap_or_else(|e| panic!("{def}.{key}: {e}"));
                } else {
                    let _: RecurringDoc =
                        serde_json::from_value(one).unwrap_or_else(|e| panic!("{def}.{key}: {e}"));
                }
            }
        }
    }

    /// A value of the type a schema property asks for.
    fn sample(property: &serde_json::Value) -> serde_json::Value {
        let kind = match property["$ref"].as_str() {
            Some("#/$defs/macros") => Some("object"),
            Some("#/$defs/uses" | "#/$defs/names") => Some("array"),
            Some(_) => None,
            None => property["type"].as_str(),
        };
        match kind {
            Some("string") => "x".into(),
            Some("boolean") => true.into(),
            Some("array") => match property["items"]["type"].as_str() {
                Some("integer") => serde_json::json!([1]),
                Some("array") => serde_json::json!([[1, 2]]),
                Some("object") => {
                    serde_json::json!([{"days": "monday 1", "timerange": "00:00-01:00"}])
                }
                _ => serde_json::json!(["x"]),
            },
            Some("object") => serde_json::json!({"K": "v"}),
            // `$ref` to an integer definition.
            _ => 1.into(),
        }
    }
}
