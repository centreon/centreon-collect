//! The structured configuration document, versions 1 to 5
//! (`gorgone-rs/schemas/engine-configuration.v5.json`): what centreon-web
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
//! Version 3 adds host and service groups, severities, tags, parents,
//! dependencies, acknowledgement timeouts, a service's own time zone and
//! the event log settings. Nothing changes meaning, but an engine reading
//! 1-2 would refuse every document using them: a new version lets
//! gorgone-server refuse it up front (409) instead of a failed apply.
//!
//! Version 4 adds notifications (contacts, contact groups, escalations,
//! the notification keys of hosts and services), event handlers,
//! flapping, freshness and the TLS of the connection to the central
//! broker. Nothing changes meaning; a new version for the same reason as
//! version 3.
//!
//! Version 5 adds the certificates of that connection inline
//! (`broker.tls.ca_certificate_pem`, `public_cert_pem`, `private_key_pem`),
//! the PKI centreon-web manages: the key is an `encrypt::` value,
//! decrypted with the key of the macros.
//!
//! Every object refuses a field it does not know: a field the engine would
//! ignore is a behaviour it would silently not have (a check period, a
//! downtime...), so the whole document is refused and the configuration in
//! force stays.

use std::collections::{BTreeMap, HashMap};

use serde::Deserialize;

use crate::cfg::{self, Kind, LogLevel, MonitoringConfig, RawObject, Report, Settings};

/// Latest version of the document format this engine reads; it also
/// reads the previous ones.
pub const VERSION: u32 = 5;

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
    #[serde(default)]
    pub hostgroups: Vec<HostGroupDoc>,
    #[serde(default)]
    pub servicegroups: Vec<ServiceGroupDoc>,
    #[serde(default)]
    pub severities: Vec<SeverityDoc>,
    #[serde(default)]
    pub tags: Vec<TagDoc>,
    #[serde(default)]
    pub host_dependencies: Vec<DependencyDoc>,
    #[serde(default)]
    pub service_dependencies: Vec<DependencyDoc>,
    #[serde(default)]
    pub contact_templates: Vec<ContactDoc>,
    #[serde(default)]
    pub contacts: Vec<ContactDoc>,
    #[serde(default)]
    pub contactgroups: Vec<ContactGroupDoc>,
    #[serde(default)]
    pub host_escalations: Vec<EscalationDoc>,
    #[serde(default)]
    pub service_escalations: Vec<EscalationDoc>,
}

/// A contact or a contact template (`define contact`).
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct ContactDoc {
    pub name: Option<String>,
    #[serde(default, rename = "use")]
    pub uses: Vec<String>,
    pub contact_name: Option<String>,
    pub alias: Option<String>,
    pub email: Option<String>,
    pub pager: Option<String>,
    pub address1: Option<String>,
    pub address2: Option<String>,
    pub address3: Option<String>,
    pub address4: Option<String>,
    pub address5: Option<String>,
    pub address6: Option<String>,
    pub host_notifications_enabled: Option<bool>,
    pub service_notifications_enabled: Option<bool>,
    pub host_notification_options: Option<String>,
    pub service_notification_options: Option<String>,
    pub host_notification_period: Option<String>,
    pub service_notification_period: Option<String>,
    #[serde(default)]
    pub host_notification_commands: Vec<String>,
    #[serde(default)]
    pub service_notification_commands: Vec<String>,
    pub timezone: Option<String>,
    #[serde(default)]
    pub contactgroups: Vec<String>,
}

/// A contact group (`define contactgroup`).
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct ContactGroupDoc {
    pub contactgroup_name: String,
    pub alias: Option<String>,
    #[serde(default)]
    pub members: Vec<String>,
    #[serde(default)]
    pub contactgroup_members: Vec<String>,
}

/// A host or service escalation, with the keys the .cfg generator writes.
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct EscalationDoc {
    #[serde(default)]
    pub host_name: Vec<String>,
    #[serde(default)]
    pub hostgroup_name: Vec<String>,
    #[serde(default)]
    pub service_description: Vec<String>,
    #[serde(default)]
    pub servicegroup_name: Vec<String>,
    #[serde(default)]
    pub contact_groups: Vec<String>,
    pub first_notification: Option<u32>,
    pub last_notification: Option<u32>,
    pub notification_interval: Option<f64>,
    pub escalation_period: Option<String>,
    pub escalation_options: Option<String>,
}

/// A host group (`define hostgroup`), members by host name.
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct HostGroupDoc {
    pub hostgroup_id: Option<u64>,
    pub hostgroup_name: String,
    pub alias: Option<String>,
    #[serde(default)]
    pub members: Vec<String>,
}

/// A service group (`define servicegroup`), members as `[host, service]`.
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct ServiceGroupDoc {
    pub servicegroup_id: Option<u64>,
    pub servicegroup_name: String,
    pub alias: Option<String>,
    #[serde(default)]
    pub members: Vec<(String, String)>,
    #[serde(default)]
    pub servicegroup_members: Vec<String>,
}

/// A severity (`define severity`); `type` is `host` or `service`.
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct SeverityDoc {
    pub id: u64,
    pub severity_name: String,
    pub level: u32,
    pub icon_id: Option<u64>,
    #[serde(rename = "type")]
    pub kind: String,
}

/// A tag (`define tag`); `type` is `hostcategory`, `servicecategory`,
/// `hostgroup` or `servicegroup`.
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct TagDoc {
    pub id: u64,
    pub tag_name: String,
    #[serde(rename = "type")]
    pub kind: String,
}

/// A host or service dependency, with the keys the .cfg generator writes.
#[derive(Debug, Clone, Default, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct DependencyDoc {
    #[serde(default)]
    pub host_name: Vec<String>,
    #[serde(default)]
    pub hostgroup_name: Vec<String>,
    #[serde(default)]
    pub dependent_host_name: Vec<String>,
    #[serde(default)]
    pub dependent_hostgroup_name: Vec<String>,
    #[serde(default)]
    pub service_description: Vec<String>,
    #[serde(default)]
    pub servicegroup_name: Vec<String>,
    #[serde(default)]
    pub dependent_service_description: Vec<String>,
    #[serde(default)]
    pub dependent_servicegroup_name: Vec<String>,
    pub execution_failure_criteria: Option<String>,
    pub notification_failure_criteria: Option<String>,
    pub inherits_parent: Option<bool>,
    pub dependency_period: Option<String>,
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
    /// TLS of the connection, the keys of the `cbmod` output (version 4).
    pub tls: Option<BrokerTlsDoc>,
}

/// `tls`, `private_key`, `public_cert`, `ca_certificate`, `tls_hostname`
/// of the `cbmod` output: paths on the poller. Version 5: the same
/// certificates inline, `private_key_pem` as an `encrypt::` value.
#[derive(Clone, Default, PartialEq, Eq, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct BrokerTlsDoc {
    pub tls: String,
    pub private_key: Option<String>,
    pub public_cert: Option<String>,
    pub ca_certificate: Option<String>,
    pub tls_hostname: Option<String>,
    pub ca_certificate_pem: Option<String>,
    pub public_cert_pem: Option<String>,
    pub private_key_pem: Option<String>,
}

/// The key stays out of the debug print, even encrypted.
impl std::fmt::Debug for BrokerTlsDoc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("BrokerTlsDoc")
            .field("tls", &self.tls)
            .field("private_key", &self.private_key)
            .field("public_cert", &self.public_cert)
            .field("ca_certificate", &self.ca_certificate)
            .field("tls_hostname", &self.tls_hostname)
            .field("ca_certificate_pem", &self.ca_certificate_pem.is_some())
            .field("public_cert_pem", &self.public_cert_pem.is_some())
            .field("private_key_pem", &self.private_key_pem.is_some())
            .finish()
    }
}

impl BrokerTlsDoc {
    /// The first version 5 field given, if any.
    fn version_5_field(&self) -> Option<&'static str> {
        [
            ("broker.tls.ca_certificate_pem", &self.ca_certificate_pem),
            ("broker.tls.public_cert_pem", &self.public_cert_pem),
            ("broker.tls.private_key_pem", &self.private_key_pem),
        ]
        .into_iter()
        .find_map(|(name, value)| value.is_some().then_some(name))
    }

    /// A value given both as a path and inline.
    fn ambiguous(&self) -> Option<&'static str> {
        [
            (
                "ca_certificate",
                &self.ca_certificate,
                &self.ca_certificate_pem,
            ),
            ("public_cert", &self.public_cert, &self.public_cert_pem),
            ("private_key", &self.private_key, &self.private_key_pem),
        ]
        .into_iter()
        .find_map(|(name, path, pem)| (path.is_some() && pem.is_some()).then_some(name))
    }
}

impl Broker {
    /// The TLS settings of the connection (no TLS without them). An
    /// `encrypt::` key is decrypted with `credentials` (the key of the
    /// macros), `raw::` removed; an error says why, never the value.
    pub fn tls_conf(
        &self,
        credentials: Option<&crate::crypto::Credentials>,
    ) -> crate::broker_conf::TlsConf {
        let Some(tls) = &self.tls else {
            return crate::broker_conf::TlsConf::default();
        };
        let path = |value: &Option<String>| {
            value
                .as_ref()
                .map(|v| v.trim().to_owned())
                .filter(|v| !v.is_empty())
        };
        let pem = |value: &Option<String>| value.clone().filter(|v| !v.trim().is_empty());
        let key = pem(&tls.private_key_pem).map(|value| {
            let plain = if let Some(raw) = value.strip_prefix("raw::") {
                Ok(raw.to_owned())
            } else if let Some(encrypted) = value.strip_prefix("encrypt::") {
                match credentials {
                    Some(keys) => keys
                        .decrypt(encrypted)
                        .map_err(|error| format!("cannot be decrypted: {error}")),
                    None => {
                        Err("encrypted, and no key to decrypt it (engine-context.json)".to_owned())
                    }
                }
            } else {
                Ok(value)
            };
            crate::broker_conf::PrivateKeyPem::new(plain)
        });
        crate::broker_conf::TlsConf {
            mode: crate::broker_conf::TlsMode::parse(&tls.tls).unwrap_or_default(),
            private_key: path(&tls.private_key),
            public_cert: path(&tls.public_cert),
            ca_certificate: path(&tls.ca_certificate),
            tls_hostname: path(&tls.tls_hostname),
            ca_certificate_pem: pem(&tls.ca_certificate_pem),
            public_cert_pem: pem(&tls.public_cert_pem),
            private_key_pem: key,
        }
    }
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
    pub log_service_retries: Option<bool>,
    pub log_host_retries: Option<bool>,
    pub log_external_commands: Option<bool>,
    pub log_passive_checks: Option<bool>,
    pub log_level_events: Option<String>,
    pub log_level_checks: Option<String>,
    pub log_level_external_command: Option<String>,
    pub soft_state_dependencies: Option<bool>,
    pub enable_predictive_host_dependency_checks: Option<bool>,
    pub enable_predictive_service_dependency_checks: Option<bool>,
    pub cached_host_check_horizon: Option<u32>,
    pub enable_notifications: Option<bool>,
    pub notification_timeout: Option<u32>,
    pub log_notifications: Option<bool>,
    pub log_level_notifications: Option<String>,
    pub log_level_runtime: Option<String>,
    pub date_format: Option<String>,
    pub illegal_macro_output_chars: Option<String>,
    pub admin_email: Option<String>,
    pub admin_pager: Option<String>,
    pub enable_event_handlers: Option<bool>,
    pub global_host_event_handler: Option<String>,
    pub global_service_event_handler: Option<String>,
    pub event_handler_timeout: Option<u32>,
    pub log_event_handlers: Option<bool>,
    pub enable_flap_detection: Option<bool>,
    pub low_service_flap_threshold: Option<f64>,
    pub high_service_flap_threshold: Option<f64>,
    pub low_host_flap_threshold: Option<f64>,
    pub high_host_flap_threshold: Option<f64>,
    pub check_service_freshness: Option<bool>,
    pub check_host_freshness: Option<bool>,
    pub service_freshness_check_interval: Option<u32>,
    pub host_freshness_check_interval: Option<u32>,
    pub additional_freshness_latency: Option<u32>,
}

impl SettingsDoc {
    /// The version 4 settings given, as the main file would write them.
    fn version_4_values(&self) -> Vec<(&'static str, String)> {
        let flag = |v: Option<bool>| v.map(|v| if v { "1" } else { "0" }.to_owned());
        let num = |v: Option<u32>| v.map(|v| v.to_string());
        let float = |v: Option<f64>| v.map(|v| v.to_string());
        let text = |v: &Option<String>| v.clone();
        [
            ("enable_notifications", flag(self.enable_notifications)),
            ("notification_timeout", num(self.notification_timeout)),
            ("log_notifications", flag(self.log_notifications)),
            (
                "log_level_notifications",
                text(&self.log_level_notifications),
            ),
            ("log_level_runtime", text(&self.log_level_runtime)),
            ("date_format", text(&self.date_format)),
            (
                "illegal_macro_output_chars",
                text(&self.illegal_macro_output_chars),
            ),
            ("admin_email", text(&self.admin_email)),
            ("admin_pager", text(&self.admin_pager)),
            ("enable_event_handlers", flag(self.enable_event_handlers)),
            (
                "global_host_event_handler",
                text(&self.global_host_event_handler),
            ),
            (
                "global_service_event_handler",
                text(&self.global_service_event_handler),
            ),
            ("event_handler_timeout", num(self.event_handler_timeout)),
            ("log_event_handlers", flag(self.log_event_handlers)),
            ("enable_flap_detection", flag(self.enable_flap_detection)),
            (
                "low_service_flap_threshold",
                float(self.low_service_flap_threshold),
            ),
            (
                "high_service_flap_threshold",
                float(self.high_service_flap_threshold),
            ),
            (
                "low_host_flap_threshold",
                float(self.low_host_flap_threshold),
            ),
            (
                "high_host_flap_threshold",
                float(self.high_host_flap_threshold),
            ),
            (
                "check_service_freshness",
                flag(self.check_service_freshness),
            ),
            ("check_host_freshness", flag(self.check_host_freshness)),
            (
                "service_freshness_check_interval",
                num(self.service_freshness_check_interval),
            ),
            (
                "host_freshness_check_interval",
                num(self.host_freshness_check_interval),
            ),
            (
                "additional_freshness_latency",
                num(self.additional_freshness_latency),
            ),
        ]
        .into_iter()
        .filter_map(|(key, value)| value.map(|value| (key, value)))
        .collect()
    }

    /// The first version 3 setting given, if any.
    fn version_3_field(&self) -> Option<&'static str> {
        [
            ("log_service_retries", self.log_service_retries.is_some()),
            ("log_host_retries", self.log_host_retries.is_some()),
            (
                "log_external_commands",
                self.log_external_commands.is_some(),
            ),
            ("log_passive_checks", self.log_passive_checks.is_some()),
            ("log_level_events", self.log_level_events.is_some()),
            ("log_level_checks", self.log_level_checks.is_some()),
            (
                "log_level_external_command",
                self.log_level_external_command.is_some(),
            ),
            (
                "soft_state_dependencies",
                self.soft_state_dependencies.is_some(),
            ),
            (
                "enable_predictive_host_dependency_checks",
                self.enable_predictive_host_dependency_checks.is_some(),
            ),
            (
                "enable_predictive_service_dependency_checks",
                self.enable_predictive_service_dependency_checks.is_some(),
            ),
            (
                "cached_host_check_horizon",
                self.cached_host_check_horizon.is_some(),
            ),
        ]
        .into_iter()
        .find_map(|(name, given)| given.then_some(name))
    }
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
    #[serde(default)]
    pub parents: Vec<String>,
    #[serde(default)]
    pub hostgroups: Vec<String>,
    pub severity_id: Option<u64>,
    #[serde(default)]
    pub category_tags: Vec<u64>,
    #[serde(default)]
    pub group_tags: Vec<u64>,
    pub icon_id: Option<u64>,
    pub acknowledgement_timeout: Option<u32>,
    pub notifications_enabled: Option<bool>,
    pub notification_interval: Option<u32>,
    pub first_notification_delay: Option<u32>,
    pub recovery_notification_delay: Option<u32>,
    pub notification_options: Option<String>,
    pub notification_period: Option<String>,
    #[serde(default)]
    pub contacts: Vec<String>,
    #[serde(default)]
    pub contact_groups: Vec<String>,
    pub event_handler: Option<String>,
    pub event_handler_enabled: Option<bool>,
    pub flap_detection_enabled: Option<bool>,
    pub flap_detection_options: Option<String>,
    pub low_flap_threshold: Option<f64>,
    pub high_flap_threshold: Option<f64>,
    pub check_freshness: Option<bool>,
    pub freshness_threshold: Option<u32>,
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
    pub timezone: Option<String>,
    #[serde(default)]
    pub servicegroups: Vec<String>,
    pub severity_id: Option<u64>,
    #[serde(default)]
    pub category_tags: Vec<u64>,
    #[serde(default)]
    pub group_tags: Vec<u64>,
    pub icon_id: Option<u64>,
    pub acknowledgement_timeout: Option<u32>,
    pub notifications_enabled: Option<bool>,
    pub notification_interval: Option<u32>,
    pub first_notification_delay: Option<u32>,
    pub recovery_notification_delay: Option<u32>,
    pub notification_options: Option<String>,
    pub notification_period: Option<String>,
    #[serde(default)]
    pub contacts: Vec<String>,
    #[serde(default)]
    pub contact_groups: Vec<String>,
    pub event_handler: Option<String>,
    pub event_handler_enabled: Option<bool>,
    pub flap_detection_enabled: Option<bool>,
    pub flap_detection_options: Option<String>,
    pub low_flap_threshold: Option<f64>,
    pub high_flap_threshold: Option<f64>,
    pub check_freshness: Option<bool>,
    pub freshness_threshold: Option<u32>,
    pub is_volatile: Option<bool>,
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

    /// A list, as the .cfg writes it: comma-separated.
    fn list(&mut self, key: &str, values: &[impl ToString]) {
        if !values.is_empty() {
            let joined: Vec<String> = values.iter().map(ToString::to_string).collect();
            self.0.insert(key.to_owned(), joined.join(","));
        }
    }
}

/// The notification fields of a host or a service, given.
macro_rules! notify_given {
    ($doc:expr) => {
        $doc.notifications_enabled.is_some()
            || $doc.notification_interval.is_some()
            || $doc.first_notification_delay.is_some()
            || $doc.recovery_notification_delay.is_some()
            || $doc.notification_options.is_some()
            || $doc.notification_period.is_some()
            || !$doc.contacts.is_empty()
            || !$doc.contact_groups.is_empty()
            || $doc.event_handler.is_some()
            || $doc.event_handler_enabled.is_some()
            || $doc.flap_detection_enabled.is_some()
            || $doc.flap_detection_options.is_some()
            || $doc.low_flap_threshold.is_some()
            || $doc.high_flap_threshold.is_some()
            || $doc.check_freshness.is_some()
            || $doc.freshness_threshold.is_some()
    };
}

/// The notification fields as raw-object fields.
macro_rules! notify_fields {
    ($fields:expr, $doc:expr) => {
        $fields.flag("notifications_enabled", $doc.notifications_enabled);
        $fields.number("notification_interval", $doc.notification_interval);
        $fields.number("first_notification_delay", $doc.first_notification_delay);
        $fields.number(
            "recovery_notification_delay",
            $doc.recovery_notification_delay,
        );
        $fields.text("notification_options", $doc.notification_options.as_ref());
        $fields.text("notification_period", $doc.notification_period.as_ref());
        $fields.list("contacts", &$doc.contacts);
        $fields.list("contact_groups", &$doc.contact_groups);
        $fields.text("event_handler", $doc.event_handler.as_ref());
        $fields.flag("event_handler_enabled", $doc.event_handler_enabled);
        $fields.flag("flap_detection_enabled", $doc.flap_detection_enabled);
        $fields.text(
            "flap_detection_options",
            $doc.flap_detection_options.as_ref(),
        );
        $fields.number("low_flap_threshold", $doc.low_flap_threshold);
        $fields.number("high_flap_threshold", $doc.high_flap_threshold);
        $fields.flag("check_freshness", $doc.check_freshness);
        $fields.number("freshness_threshold", $doc.freshness_threshold);
    };
}

impl HostDoc {
    fn uses_version_4(&self) -> bool {
        notify_given!(self)
    }

    fn uses_version_3(&self) -> bool {
        !self.parents.is_empty()
            || !self.hostgroups.is_empty()
            || self.severity_id.is_some()
            || !self.category_tags.is_empty()
            || !self.group_tags.is_empty()
            || self.icon_id.is_some()
            || self.acknowledgement_timeout.is_some()
    }
}

impl ServiceDoc {
    fn uses_version_4(&self) -> bool {
        notify_given!(self) || self.is_volatile.is_some()
    }

    fn uses_version_3(&self) -> bool {
        self.timezone.is_some()
            || !self.servicegroups.is_empty()
            || self.severity_id.is_some()
            || !self.category_tags.is_empty()
            || !self.group_tags.is_empty()
            || self.icon_id.is_some()
            || self.acknowledgement_timeout.is_some()
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
    fields.list("parents", &host.parents);
    fields.list("hostgroups", &host.hostgroups);
    fields.number("severity_id", host.severity_id);
    fields.list("category_tags", &host.category_tags);
    fields.list("group_tags", &host.group_tags);
    fields.number("icon_id", host.icon_id);
    fields.number("acknowledgement_timeout", host.acknowledgement_timeout);
    notify_fields!(fields, host);
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
    fields.text("timezone", service.timezone.as_ref());
    fields.list("servicegroups", &service.servicegroups);
    fields.number("severity_id", service.severity_id);
    fields.list("category_tags", &service.category_tags);
    fields.list("group_tags", &service.group_tags);
    fields.number("icon_id", service.icon_id);
    fields.number("acknowledgement_timeout", service.acknowledgement_timeout);
    notify_fields!(fields, service);
    fields.flag("is_volatile", service.is_volatile);
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

/// A group, severity, tag or dependency as the raw object of the file
/// reader.
fn registered(fields: Fields, origin: String) -> RawObject {
    RawObject {
        fields: fields.0,
        register: true,
        origin,
        ..RawObject::default()
    }
}

fn dependency_object(dependency: &DependencyDoc, origin: String) -> RawObject {
    let mut fields = Fields::default();
    fields.list("hosts", &dependency.host_name);
    fields.list("hostgroups", &dependency.hostgroup_name);
    fields.list("dependent_hosts", &dependency.dependent_host_name);
    fields.list("dependent_hostgroups", &dependency.dependent_hostgroup_name);
    fields.list("service_description", &dependency.service_description);
    fields.list("servicegroups", &dependency.servicegroup_name);
    fields.list(
        "dependent_service_description",
        &dependency.dependent_service_description,
    );
    fields.list(
        "dependent_servicegroups",
        &dependency.dependent_servicegroup_name,
    );
    fields.text(
        "execution_failure_options",
        dependency.execution_failure_criteria.as_ref(),
    );
    fields.text(
        "notification_failure_options",
        dependency.notification_failure_criteria.as_ref(),
    );
    fields.flag("inherits_parent", dependency.inherits_parent);
    fields.text("dependency_period", dependency.dependency_period.as_ref());
    registered(fields, origin)
}

fn contact_object(contact: &ContactDoc, register: bool, origin: String) -> RawObject {
    let mut fields = Fields::default();
    fields.text("contact_name", contact.contact_name.as_ref());
    fields.text("alias", contact.alias.as_ref());
    fields.text("email", contact.email.as_ref());
    fields.text("pager", contact.pager.as_ref());
    for (key, value) in [
        ("address1", &contact.address1),
        ("address2", &contact.address2),
        ("address3", &contact.address3),
        ("address4", &contact.address4),
        ("address5", &contact.address5),
        ("address6", &contact.address6),
    ] {
        fields.text(key, value.as_ref());
    }
    fields.flag(
        "host_notifications_enabled",
        contact.host_notifications_enabled,
    );
    fields.flag(
        "service_notifications_enabled",
        contact.service_notifications_enabled,
    );
    fields.text(
        "host_notification_options",
        contact.host_notification_options.as_ref(),
    );
    fields.text(
        "service_notification_options",
        contact.service_notification_options.as_ref(),
    );
    fields.text(
        "host_notification_period",
        contact.host_notification_period.as_ref(),
    );
    fields.text(
        "service_notification_period",
        contact.service_notification_period.as_ref(),
    );
    fields.list(
        "host_notification_commands",
        &contact.host_notification_commands,
    );
    fields.list(
        "service_notification_commands",
        &contact.service_notification_commands,
    );
    fields.text("timezone", contact.timezone.as_ref());
    fields.list("contactgroups", &contact.contactgroups);
    RawObject {
        fields: fields.0,
        uses: contact.uses.clone(),
        name: contact.name.clone(),
        register,
        origin,
        ..RawObject::default()
    }
}

fn escalation_object(escalation: &EscalationDoc, origin: String) -> RawObject {
    let mut fields = Fields::default();
    fields.list("host_name", &escalation.host_name);
    fields.list("hostgroup_name", &escalation.hostgroup_name);
    fields.list("service_description", &escalation.service_description);
    fields.list("servicegroup_name", &escalation.servicegroup_name);
    fields.list("contact_groups", &escalation.contact_groups);
    fields.number("first_notification", escalation.first_notification);
    fields.number("last_notification", escalation.last_notification);
    fields.number("notification_interval", escalation.notification_interval);
    fields.text("escalation_period", escalation.escalation_period.as_ref());
    fields.text("escalation_options", escalation.escalation_options.as_ref());
    registered(fields, origin)
}

/// Contacts, contact groups and escalations of a document (version 4).
fn notification_objects(
    document: &Document,
    objects: &mut Vec<(Kind, RawObject)>,
    report: &mut Report,
) {
    for (index, contact) in document.contact_templates.iter().enumerate() {
        if contact.name.as_deref().is_none_or(str::is_empty) {
            report.errors.push(format!(
                "contact_templates[{index}]: a template needs a name"
            ));
        }
        let at = origin("contact_templates", index, contact.name.as_ref());
        objects.push((Kind::Contact, contact_object(contact, false, at)));
    }
    for (index, contact) in document.contacts.iter().enumerate() {
        let at = origin("contacts", index, contact.contact_name.as_ref());
        if contact.name.is_some() {
            report
                .errors
                .push(format!("{at}: name is for contact_templates"));
        }
        objects.push((Kind::Contact, contact_object(contact, true, at)));
    }
    for (index, group) in document.contactgroups.iter().enumerate() {
        let mut fields = Fields::default();
        fields.text("contactgroup_name", Some(&group.contactgroup_name));
        fields.text("alias", group.alias.as_ref());
        fields.list("members", &group.members);
        fields.list("contactgroup_members", &group.contactgroup_members);
        let at = origin("contactgroups", index, Some(&group.contactgroup_name));
        objects.push((Kind::ContactGroup, registered(fields, at)));
    }
    for (index, escalation) in document.host_escalations.iter().enumerate() {
        let at = origin("host_escalations", index, None);
        objects.push((Kind::HostEscalation, escalation_object(escalation, at)));
    }
    for (index, escalation) in document.service_escalations.iter().enumerate() {
        let at = origin("service_escalations", index, None);
        objects.push((Kind::ServiceEscalation, escalation_object(escalation, at)));
    }
}

/// The relation objects of a document (version 3).
fn relation_objects(document: &Document, objects: &mut Vec<(Kind, RawObject)>) {
    for (index, group) in document.hostgroups.iter().enumerate() {
        let mut fields = Fields::default();
        fields.number("hostgroup_id", group.hostgroup_id);
        fields.text("hostgroup_name", Some(&group.hostgroup_name));
        fields.text("alias", group.alias.as_ref());
        fields.list("members", &group.members);
        let at = origin("hostgroups", index, Some(&group.hostgroup_name));
        objects.push((Kind::HostGroup, registered(fields, at)));
    }
    for (index, group) in document.servicegroups.iter().enumerate() {
        let mut fields = Fields::default();
        fields.number("servicegroup_id", group.servicegroup_id);
        fields.text("servicegroup_name", Some(&group.servicegroup_name));
        fields.text("alias", group.alias.as_ref());
        let members: Vec<&String> = group
            .members
            .iter()
            .flat_map(|(host, service)| [host, service])
            .collect();
        fields.list("members", &members);
        fields.list("servicegroup_members", &group.servicegroup_members);
        let at = origin("servicegroups", index, Some(&group.servicegroup_name));
        objects.push((Kind::ServiceGroup, registered(fields, at)));
    }
    for (index, severity) in document.severities.iter().enumerate() {
        let mut fields = Fields::default();
        fields.number("id", Some(severity.id));
        fields.text("severity_name", Some(&severity.severity_name));
        fields.number("level", Some(severity.level));
        fields.number("icon_id", severity.icon_id);
        fields.text("type", Some(&severity.kind));
        let at = origin("severities", index, Some(&severity.severity_name));
        objects.push((Kind::Severity, registered(fields, at)));
    }
    for (index, tag) in document.tags.iter().enumerate() {
        let mut fields = Fields::default();
        fields.number("id", Some(tag.id));
        fields.text("tag_name", Some(&tag.tag_name));
        fields.text("type", Some(&tag.kind));
        let at = origin("tags", index, Some(&tag.tag_name));
        objects.push((Kind::Tag, registered(fields, at)));
    }
    for (index, dependency) in document.host_dependencies.iter().enumerate() {
        let at = origin("host_dependencies", index, None);
        objects.push((Kind::HostDependency, dependency_object(dependency, at)));
    }
    for (index, dependency) in document.service_dependencies.iter().enumerate() {
        let at = origin("service_dependencies", index, None);
        objects.push((Kind::ServiceDependency, dependency_object(dependency, at)));
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
    if document.version < 3
        && let Some(field) = version_3_field(&document)
    {
        return refuse(format!(
            "{field} needs version 3 of the configuration document"
        ));
    }
    if document.version < 4
        && let Some(field) = version_4_field(&document)
    {
        return refuse(format!(
            "{field} needs version 4 of the configuration document"
        ));
    }
    if document.version < 5
        && let Some(field) = document
            .broker
            .tls
            .as_ref()
            .and_then(BrokerTlsDoc::version_5_field)
    {
        return refuse(format!(
            "{field} needs version 5 of the configuration document"
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
    if let Some(name) = document
        .broker
        .tls
        .as_ref()
        .and_then(BrokerTlsDoc::ambiguous)
    {
        report
            .errors
            .push(format!("broker.tls: {name} and {name}_pem are both given"));
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

/// The first version 4 field a document uses, if any.
fn version_4_field(document: &Document) -> Option<&'static str> {
    if let Some((key, _)) = document.settings.version_4_values().first() {
        return Some(key);
    }
    if document.broker.tls.is_some() {
        return Some("broker.tls");
    }
    let lists = [
        ("contact_templates", document.contact_templates.is_empty()),
        ("contacts", document.contacts.is_empty()),
        ("contactgroups", document.contactgroups.is_empty()),
        ("host_escalations", document.host_escalations.is_empty()),
        (
            "service_escalations",
            document.service_escalations.is_empty(),
        ),
    ];
    if let Some((name, _)) = lists.into_iter().find(|(_, empty)| !empty) {
        return Some(name);
    }
    if document
        .host_templates
        .iter()
        .chain(&document.hosts)
        .any(HostDoc::uses_version_4)
    {
        return Some("notification, event handler, flapping or freshness field of a host");
    }
    if document
        .service_templates
        .iter()
        .chain(&document.services)
        .any(ServiceDoc::uses_version_4)
    {
        return Some("notification, event handler, flapping or freshness field of a service");
    }
    None
}

/// The first version 3 field a document uses, if any.
fn version_3_field(document: &Document) -> Option<&'static str> {
    if let Some(field) = document.settings.version_3_field() {
        return Some(field);
    }
    let lists = [
        ("hostgroups", document.hostgroups.is_empty()),
        ("servicegroups", document.servicegroups.is_empty()),
        ("severities", document.severities.is_empty()),
        ("tags", document.tags.is_empty()),
        ("host_dependencies", document.host_dependencies.is_empty()),
        (
            "service_dependencies",
            document.service_dependencies.is_empty(),
        ),
    ];
    if let Some((name, _)) = lists.into_iter().find(|(_, empty)| !empty) {
        return Some(name);
    }
    if document
        .host_templates
        .iter()
        .chain(&document.hosts)
        .any(HostDoc::uses_version_3)
    {
        return Some(
            "parents, hostgroups, severity_id, category_tags, group_tags, icon_id or \
             acknowledgement_timeout of a host",
        );
    }
    if document
        .service_templates
        .iter()
        .chain(&document.services)
        .any(ServiceDoc::uses_version_3)
    {
        return Some(
            "timezone, servicegroups, severity_id, category_tags, group_tags, icon_id or \
             acknowledgement_timeout of a service",
        );
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
        log_service_retries: given
            .log_service_retries
            .unwrap_or(defaults.log_service_retries),
        log_host_retries: given.log_host_retries.unwrap_or(defaults.log_host_retries),
        log_external_commands: given
            .log_external_commands
            .unwrap_or(defaults.log_external_commands),
        log_passive_checks: given
            .log_passive_checks
            .unwrap_or(defaults.log_passive_checks),
        log_level_events: level(
            &mut report,
            "log_level_events",
            given.log_level_events.as_deref(),
            defaults.log_level_events,
        ),
        log_level_checks: level(
            &mut report,
            "log_level_checks",
            given.log_level_checks.as_deref(),
            defaults.log_level_checks,
        ),
        log_level_external_command: level(
            &mut report,
            "log_level_external_command",
            given.log_level_external_command.as_deref(),
            defaults.log_level_external_command,
        ),
        soft_state_dependencies: given
            .soft_state_dependencies
            .unwrap_or(defaults.soft_state_dependencies),
        enable_predictive_host_dependency_checks: given
            .enable_predictive_host_dependency_checks
            .unwrap_or(defaults.enable_predictive_host_dependency_checks),
        enable_predictive_service_dependency_checks: given
            .enable_predictive_service_dependency_checks
            .unwrap_or(defaults.enable_predictive_service_dependency_checks),
        cached_host_check_horizon: given
            .cached_host_check_horizon
            .unwrap_or(defaults.cached_host_check_horizon),
        ..defaults.clone()
    };
    let mut settings = settings;
    for (key, value) in given.version_4_values() {
        if let Some(Err(error)) = settings.set_more(key, &value) {
            report.errors.push(format!("settings.{error}"));
        }
    }
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
    relation_objects(document, &mut objects);
    notification_objects(document, &mut objects, &mut report);
    cfg::build(settings, users, objects, report)
}

/// A `log_level_*` setting; an invalid one is an error, as in the files.
fn level(report: &mut Report, key: &str, value: Option<&str>, default: LogLevel) -> LogLevel {
    let Some(value) = value else {
        return default;
    };
    LogLevel::parse(value).unwrap_or_else(|| {
        report.errors.push(format!(
            "settings.{key}: '{value}' is not off, critical, error, err, warning, info, debug \
             or trace"
        ));
        default
    })
}

#[cfg(test)]
pub(crate) mod tests {
    use std::path::{Path, PathBuf};

    use super::*;

    /// The objects of `cfg::tests::web_like`, as centreon-web sends them in
    /// a document.
    pub(crate) const WEB_LIKE: &str = r#"{
      "version": 4,
      "poller": {"id": 4, "name": "rust-poller"},
      "broker": {"address": "broker:5669", "name": "rust-poller-module"},
      "settings": {"interval_length": 60, "service_check_timeout": 20, "retention_update_interval": 30,
                   "log_service_retries": true, "log_level_external_command": "info",
                   "enable_flap_detection": true, "notification_timeout": 12},
      "resource_macros": {"USER1": "/usr/lib/nagios/plugins", "CENTREONPLUGINS": "/usr/lib/centreon/plugins"},
      "commands": [{"command_name": "check_dummy", "command_line": "$USER1$/check_dummy $ARG1$ \"$ARG2$\""},
                   {"command_name": "notify", "command_line": "/bin/true $CONTACTEMAIL$"}],
      "contact_templates": [{"name": "tpl-contact", "host_notification_commands": ["notify"]}],
      "contacts": [
        {"contact_name": "admin", "alias": "Administrator", "email": "admin@localhost",
         "service_notification_options": "w,c,r,f", "service_notification_commands": ["notify"],
         "contactgroups": ["team"]},
        {"contact_name": "guest", "use": ["tpl-contact"]}
      ],
      "contactgroups": [
        {"contactgroup_name": "ops", "alias": "Operators", "members": ["guest"], "contactgroup_members": ["team"]},
        {"contactgroup_name": "team", "alias": "Team"}
      ],
      "service_escalations": [{"host_name": ["h2"], "service_description": ["Disk"], "first_notification": 2,
                               "last_notification": 0, "notification_interval": 5, "escalation_options": "c,r",
                               "contact_groups": ["ops"]}],
      "host_escalations": [{"host_name": ["h1"], "first_notification": 1, "escalation_options": "d",
                            "contact_groups": ["team"]}],
      "host_templates": [
        {"name": "generic-host", "check_command": "check_dummy!0!up", "max_check_attempts": 2,
         "macros": {"SNMPCOMMUNITY": "public"}},
        {"name": "other", "max_check_attempts": 9, "check_interval": 7,
         "macros": {"snmpcommunity": "private", "EXTRA": "x"}}
      ],
      "hosts": [
        {"host_name": "h1", "alias": "First", "address": "10.0.0.1", "use": ["generic-host", "other"], "host_id": 12,
         "check_period": "workhours", "timezone": ":Europe/Paris", "parents": ["h2"], "category_tags": [7],
         "group_tags": [20], "severity_id": 3, "acknowledgement_timeout": 5,
         "contacts": ["admin"], "notification_interval": 10, "notification_options": "d,r"},
        {"host_name": "h2", "address": "10.0.0.2", "host_id": 13}
      ],
      "service_templates": [
        {"name": "svc-tpl", "service_description": "tpl", "check_command": "check_dummy!0!ok",
         "check_interval": 3, "retry_interval": 1, "max_check_attempts": 3}
      ],
      "services": [
        {"host_name": "h1", "service_description": "Ping", "use": ["svc-tpl"], "service_id": 101,
         "group_tags": [30], "severity_id": 4, "event_handler": "check_dummy!0!handled",
         "check_freshness": true, "high_flap_threshold": 50},
        {"host_name": "h2", "service_description": "Disk", "check_command": "check_dummy!2!full",
         "active_checks_enabled": false, "service_id": 102, "macros": {"CRIT": "90"},
         "check_period": "24x7", "contact_groups": ["ops"]}
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
      ],
      "hostgroups": [{"hostgroup_id": 20, "hostgroup_name": "hg1", "alias": "First hosts", "members": ["h1", "h2"]}],
      "servicegroups": [{"servicegroup_id": 30, "servicegroup_name": "sg1", "alias": "Some services",
                         "members": [["h1", "Ping"], ["h2", "Disk"]]}],
      "tags": [{"id": 7, "tag_name": "cat7", "type": "hostcategory"}, {"id": 20, "tag_name": "hg1", "type": "hostgroup"},
               {"id": 30, "tag_name": "sg1", "type": "servicegroup"}],
      "severities": [{"id": 3, "severity_name": "high", "level": 1, "icon_id": 9, "type": "host"},
                     {"id": 4, "severity_name": "low", "level": 5, "type": "service"}],
      "service_dependencies": [{"execution_failure_criteria": "c,u", "notification_failure_criteria": "w,c",
                                "inherits_parent": true, "host_name": ["h1"], "service_description": ["Ping"],
                                "dependent_host_name": ["h2"], "dependent_service_description": ["Disk"]}],
      "host_dependencies": [{"execution_failure_criteria": "d", "dependency_period": "24x7",
                             "hostgroup_name": ["hg1"], "dependent_host_name": ["h1"]}]
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
        // The group, the dependency and the escalation of the removed
        // services are refused too.
        assert!(report.ends_with("Total Errors:   8\n"), "{report}");
    }

    #[test]
    fn inline_tls_key_is_decrypted_with_the_macro_key() {
        let keys =
            crate::crypto::Credentials::from_json(crate::crypto::tests::BENCH_KEY.as_bytes())
                .unwrap();
        let document = |key: &str| {
            format!(
                r#"{{"version": 5, "poller": {{"id": 1, "name": "p"}},
                    "broker": {{"address": "b:1", "tls": {{"tls": "yes",
                    "ca_certificate_pem": "CA", "public_cert_pem": "CERT",
                    "private_key_pem": "{key}", "tls_hostname": "b"}}}}}}"#
            )
        };
        let encrypted = format!("encrypt::{}", crate::crypto::tests::FROM_WEB.trim());
        let (parsed, _) = parse(document(&encrypted).as_bytes()).unwrap();
        let conf = parsed.broker.tls_conf(Some(&keys));
        assert_eq!(conf.ca_certificate_pem.as_deref(), Some("CA"));
        assert_eq!(conf.public_cert_pem.as_deref(), Some("CERT"));
        assert_eq!(
            conf.private_key_pem.as_ref().unwrap().pem(),
            Ok("s3cr3t-value")
        );
        assert!(conf.has_certificate());
        // Neither the decrypted key nor its ciphertext is printed.
        let printed = format!("{:?} {:?}", parsed.broker, conf);
        assert!(!printed.contains("s3cr3t") && !printed.contains(&encrypted[10..30]));
        // Without the key file: the reason, not the value.
        let conf = parsed.broker.tls_conf(None);
        let error = conf.private_key_pem.unwrap().pem().unwrap_err().to_owned();
        assert!(error.contains("no key to decrypt it"), "{error}");
        // A forged value does not decrypt.
        let forged = encrypted.replace('A', "B");
        let (parsed, _) = parse(document(&forged).as_bytes()).unwrap();
        let conf = parsed.broker.tls_conf(Some(&keys));
        assert!(conf.private_key_pem.unwrap().pem().is_err());
        // raw:: and a literal key are taken as they are.
        let (parsed, _) = parse(document("raw::PLAIN").as_bytes()).unwrap();
        let conf = parsed.broker.tls_conf(Some(&keys));
        assert_eq!(conf.private_key_pem.unwrap().pem(), Ok("PLAIN"));
    }

    #[test]
    fn refuses_unknown_fields_and_versions() {
        let unknown = WEB_LIKE.replace(r#""host_id": 13"#, r#""host_id": 13, "notes_url": "x""#);
        let report = parse(unknown.as_bytes()).unwrap_err();
        assert!(report.contains("unknown field `notes_url`"), "{report}");
        assert!(report.contains("Total Errors:   1"), "{report}");

        let newer = unknown.replace(r#""version": 4"#, r#""version": 6"#);
        let report = parse(newer.as_bytes()).unwrap_err();
        assert!(report.contains("version 6 is not supported"), "{report}");

        // Version 1 takes no version 2 field, and keeps its values literal.
        let old = WEB_LIKE.replace(r#""version": 4"#, r#""version": 1"#);
        let report = parse(old.as_bytes()).unwrap_err();
        assert!(report.contains("needs version 2"), "{report}");
        // Version 2 takes no version 3 field, version 3 no version 4 field.
        let v2 = WEB_LIKE.replace(r#""version": 4"#, r#""version": 2"#);
        let report = parse(v2.as_bytes()).unwrap_err();
        assert!(report.contains("needs version"), "{report}");
        let v3 = WEB_LIKE.replace(r#""version": 4"#, r#""version": 3"#);
        let report = parse(v3.as_bytes()).unwrap_err();
        assert!(report.contains("needs version 4"), "{report}");
        // Version 4 takes no inline certificate.
        let inline = WEB_LIKE.replace(
            r#""name": "rust-poller-module"}"#,
            r#""name": "rust-poller-module", "tls": {"tls": "yes", "public_cert_pem": "x"}}"#,
        );
        let report = parse(inline.as_bytes()).unwrap_err();
        assert!(
            report.contains("broker.tls.public_cert_pem needs version 5"),
            "{report}"
        );
        let both = inline
            .replace(r#""version": 4"#, r#""version": 5"#)
            .replace(
                r#""public_cert_pem": "x""#,
                r#""public_cert_pem": "x", "public_cert": "/c""#,
            );
        let report = parse(both.as_bytes()).unwrap_err();
        assert!(
            report.contains("public_cert and public_cert_pem are both given"),
            "{report}"
        );
        let level = WEB_LIKE.replace(
            r#""log_level_external_command": "info""#,
            r#""log_level_external_command": "loud""#,
        );
        let report = parse(level.as_bytes()).unwrap_err();
        assert!(
            report.contains("settings.log_level_external_command"),
            "{report}"
        );
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
            .join("../../schemas/engine-configuration.v5.json");
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
                "contact_templates",
                "contactgroups",
                "contacts",
                "host_dependencies",
                "host_escalations",
                "host_templates",
                "hostgroups",
                "hosts",
                "poller",
                "recurring_downtimes",
                "resource_macros",
                "service_dependencies",
                "service_escalations",
                "service_templates",
                "servicegroups",
                "services",
                "settings",
                "severities",
                "tags",
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
        let tls = &schema["properties"]["broker"]["properties"]["tls"];
        for key in keys(tls) {
            let mut one = serde_json::json!({"tls": "yes"});
            one[&key] = sample(&tls["properties"][&key]);
            serde_json::from_value::<BrokerTlsDoc>(one)
                .unwrap_or_else(|e| panic!("tls.{key}: {e}"));
        }
        for key in keys(&schema["properties"]["settings"]) {
            let one = serde_json::json!({ key.clone(): sample(&schema["properties"]["settings"]["properties"][&key]) });
            let _: SettingsDoc =
                serde_json::from_value(one).unwrap_or_else(|e| panic!("settings.{key}: {e}"));
        }
        for (def, required) in [
            ("timeperiod", serde_json::json!({"timeperiod_name": "t"})),
            ("recurring_downtime", serde_json::json!({"downtime_id": 1})),
            ("hostgroup", serde_json::json!({"hostgroup_name": "g"})),
            (
                "servicegroup",
                serde_json::json!({"servicegroup_name": "g"}),
            ),
            (
                "severity",
                serde_json::json!({"id": 1, "severity_name": "s", "level": 1, "type": "host"}),
            ),
            (
                "tag",
                serde_json::json!({"id": 1, "tag_name": "t", "type": "hostgroup"}),
            ),
            ("dependency", serde_json::json!({})),
            ("contact", serde_json::json!({})),
            (
                "contactgroup",
                serde_json::json!({"contactgroup_name": "g"}),
            ),
            ("escalation", serde_json::json!({})),
        ] {
            for key in keys(&schema["$defs"][def]) {
                let mut one = required.clone();
                one[&key] = sample(&schema["$defs"][def]["properties"][&key]);
                let read = match def {
                    "timeperiod" => serde_json::from_value::<TimeperiodDoc>(one).map(drop),
                    "recurring_downtime" => serde_json::from_value::<RecurringDoc>(one).map(drop),
                    "hostgroup" => serde_json::from_value::<HostGroupDoc>(one).map(drop),
                    "servicegroup" => serde_json::from_value::<ServiceGroupDoc>(one).map(drop),
                    "severity" => serde_json::from_value::<SeverityDoc>(one).map(drop),
                    "tag" => serde_json::from_value::<TagDoc>(one).map(drop),
                    "contact" => serde_json::from_value::<ContactDoc>(one).map(drop),
                    "contactgroup" => serde_json::from_value::<ContactGroupDoc>(one).map(drop),
                    "escalation" => serde_json::from_value::<EscalationDoc>(one).map(drop),
                    _ => serde_json::from_value::<DependencyDoc>(one).map(drop),
                };
                read.unwrap_or_else(|e| panic!("{def}.{key}: {e}"));
            }
        }
    }

    /// A value of the type a schema property asks for.
    fn sample(property: &serde_json::Value) -> serde_json::Value {
        if let Some(values) = property["enum"].as_array() {
            return values[0].clone();
        }
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
                Some("array") if property["items"]["items"]["type"] == "string" => {
                    serde_json::json!([["h", "s"]])
                }
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
