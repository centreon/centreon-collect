//! Reading of the Engine configuration centreon-web generates, reduced to
//! what the engine runs: global settings, commands, hosts, services,
//! their templates, and the `$USERn$` resource macros. Same syntax and
//! template rules as `common/engine_conf/parser.cc`. The structured
//! document (`document.rs`) is turned into the same raw objects and goes
//! through the same template resolution and checks ([`build`]).

use std::collections::{BTreeMap, HashMap, HashSet};
use std::fmt::Write as _;
use std::path::{Path, PathBuf};

mod notifications;
mod relations;

use crate::objects::{
    self, ContactConf, ContactGroupConf, DependencyConf, EscalationConf, HostGroupConf,
    ServiceGroupConf, SeverityConf, SeverityType, TagConf, TagType,
};
use crate::recurring::RecurringConf;
use crate::timeperiod::Timeperiod;

/// Level of an Engine logger (`log_level_*`): lines below it are not
/// written, and only INFO and above reach the event log (`broker_sink_mt`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
    Off,
}

impl LogLevel {
    /// `state_helper::hook`: `err` is `error`.
    #[must_use]
    pub fn parse(value: &str) -> Option<Self> {
        Some(match value.trim() {
            "trace" => Self::Trace,
            "debug" => Self::Debug,
            "info" => Self::Info,
            "warning" => Self::Warning,
            "error" | "err" => Self::Error,
            "critical" => Self::Critical,
            "off" => Self::Off,
            _ => return None,
        })
    }

    /// An INFO line of this logger is written, hence sent to the event log.
    #[must_use]
    pub fn logs_info(self) -> bool {
        self <= Self::Info
    }
}

/// `date_format` (`state_helper.cc:98-110`): the dates of the macros.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum DateFormat {
    #[default]
    Us,
    Euro,
    Iso8601,
    StrictIso8601,
}

impl DateFormat {
    /// The value of `date_format`; Engine keeps `us` for an unknown one.
    #[must_use]
    pub fn parse(value: &str) -> Self {
        match value.trim() {
            "euro" => Self::Euro,
            "iso8601" => Self::Iso8601,
            "strict-iso8601" => Self::StrictIso8601,
            _ => Self::Us,
        }
    }
}

/// Global settings of `centengine.cfg` the scheduler uses.
#[derive(Debug, Clone, PartialEq)]
pub struct Settings {
    /// Seconds in one unit of `check_interval` and `retry_interval`.
    pub interval_length: u32,
    /// Seconds before a service check is killed.
    pub service_check_timeout: u32,
    /// Seconds before a host check is killed.
    pub host_check_timeout: u32,
    /// Minutes over which the first service checks are spread, at most.
    pub max_service_check_spread: u32,
    /// Minutes over which the first host checks are spread, at most.
    pub max_host_check_spread: u32,
    /// Seconds between two instance status events.
    pub status_update_interval: u32,
    /// Zone of the timeperiods of objects without `timezone`, in the `TZ`
    /// syntax (`:Europe/Paris`); the zone of the process when empty.
    pub use_timezone: String,
    /// Minutes between two writes of the retention file.
    pub retention_update_interval: u32,
    /// Retained next checks are kept instead of spreading the checks again.
    pub use_retained_scheduling_info: bool,
    /// The `cbmod` configuration file (poller identity, central address).
    pub broker_module_cfg_file: Option<PathBuf>,
    /// SOFT service states are written to the event log.
    pub log_service_retries: bool,
    /// SOFT host states are written to the event log.
    pub log_host_retries: bool,
    /// External commands are written to the event log.
    pub log_external_commands: bool,
    /// Passive check results are written to the event log.
    pub log_passive_checks: bool,
    /// `log_level_events`: alerts, initial states, downtime alerts.
    pub log_level_events: LogLevel,
    /// `log_level_checks`: passive checks.
    pub log_level_checks: LogLevel,
    /// `log_level_external_command`: external commands.
    pub log_level_external_command: LogLevel,
    /// A dependency looks at the current SOFT state of its master, not at
    /// its last hard state.
    pub soft_state_dependencies: bool,
    /// Checks of the hosts a host depends on, at its next to last attempt.
    pub enable_predictive_host_dependency_checks: bool,
    /// Same for services.
    pub enable_predictive_service_dependency_checks: bool,
    /// Seconds a host result stays fresh enough to skip an on-demand check.
    pub cached_host_check_horizon: u32,
    /// Notifications are sent (`ENABLE_NOTIFICATIONS` toggles it).
    pub enable_notifications: bool,
    /// Seconds before a notification command is killed.
    pub notification_timeout: u32,
    /// `HOST NOTIFICATION` / `SERVICE NOTIFICATION` lines are written.
    pub log_notifications: bool,
    /// `log_level_notifications`: Engine's default `error` keeps the
    /// notification lines out of the event log.
    pub log_level_notifications: LogLevel,
    /// `log_level_runtime`: flapping start and stale results (warnings).
    pub log_level_runtime: LogLevel,
    /// Format of `$SHORTDATETIME$` and `$DATE$`.
    pub date_format: DateFormat,
    /// Characters removed from the macros of notification and event
    /// handler commands (`illegal_macro_output_chars`).
    pub illegal_macro_output_chars: String,
    /// `$ADMINEMAIL$`, `$ADMINPAGER$`.
    pub admin_email: String,
    pub admin_pager: String,
    /// Event handlers run (`ENABLE_EVENT_HANDLERS` toggles it).
    pub enable_event_handlers: bool,
    /// `global_host_event_handler`, `global_service_event_handler`:
    /// `command!args`, none when empty.
    pub global_host_event_handler: String,
    pub global_service_event_handler: String,
    /// Seconds before an event handler is killed.
    pub event_handler_timeout: u32,
    /// `HOST EVENT HANDLER` / `SERVICE EVENT HANDLER` lines are written.
    pub log_event_handlers: bool,
    /// Flapping is detected (`ENABLE_FLAP_DETECTION` toggles it).
    pub enable_flap_detection: bool,
    /// Thresholds, in percent of state change, of the objects with none.
    pub low_service_flap_threshold: f64,
    pub high_service_flap_threshold: f64,
    pub low_host_flap_threshold: f64,
    pub high_host_flap_threshold: f64,
    /// Freshness of the services and hosts with `check_freshness`.
    pub check_service_freshness: bool,
    pub check_host_freshness: bool,
    /// Seconds between two freshness checks.
    pub service_freshness_check_interval: u32,
    pub host_freshness_check_interval: u32,
    /// Seconds added to an automatic freshness threshold.
    pub additional_freshness_latency: u32,
}

impl Default for Settings {
    /// Engine's defaults (`common/engine_conf/state_helper.cc`).
    fn default() -> Self {
        Self {
            interval_length: 60,
            service_check_timeout: 60,
            host_check_timeout: 30,
            max_service_check_spread: 5,
            max_host_check_spread: 5,
            status_update_interval: 60,
            use_timezone: String::new(),
            retention_update_interval: 60,
            use_retained_scheduling_info: false,
            broker_module_cfg_file: None,
            log_service_retries: false,
            log_host_retries: false,
            log_external_commands: true,
            log_passive_checks: true,
            log_level_events: LogLevel::Info,
            log_level_checks: LogLevel::Info,
            log_level_external_command: LogLevel::Error,
            soft_state_dependencies: false,
            enable_predictive_host_dependency_checks: true,
            enable_predictive_service_dependency_checks: true,
            cached_host_check_horizon: 15,
            enable_notifications: true,
            notification_timeout: 30,
            log_notifications: true,
            log_level_notifications: LogLevel::Error,
            log_level_runtime: LogLevel::Error,
            date_format: DateFormat::Us,
            illegal_macro_output_chars: "`~$&|'\"<>".to_owned(),
            admin_email: String::new(),
            admin_pager: String::new(),
            enable_event_handlers: true,
            global_host_event_handler: String::new(),
            global_service_event_handler: String::new(),
            event_handler_timeout: 30,
            log_event_handlers: true,
            enable_flap_detection: false,
            low_service_flap_threshold: 20.0,
            high_service_flap_threshold: 30.0,
            low_host_flap_threshold: 20.0,
            high_host_flap_threshold: 30.0,
            check_service_freshness: true,
            check_host_freshness: false,
            service_freshness_check_interval: 60,
            host_freshness_check_interval: 60,
            additional_freshness_latency: 15,
        }
    }
}

impl Settings {
    /// Reads one of the notification, event handler, flapping and
    /// freshness keys of the main file (`state_helper::hook`); `None` when
    /// `key` is not one of them, `Some(Err)` on an invalid value.
    pub(crate) fn set_more(&mut self, key: &str, value: &str) -> Option<Result<(), String>> {
        let invalid = || Err(format!("{key}: invalid value {value:?}"));
        let flag = |slot: &mut bool| match parse_bool(value) {
            Some(flag) => {
                *slot = flag;
                Ok(())
            }
            None => invalid(),
        };
        let number = |slot: &mut u32| match parse_u32(value) {
            Some(number) => {
                *slot = number;
                Ok(())
            }
            None => invalid(),
        };
        let float = |slot: &mut f64| match value.trim().parse::<f64>() {
            Ok(number) => {
                *slot = number;
                Ok(())
            }
            Err(_) => invalid(),
        };
        Some(match key {
            "enable_notifications" => flag(&mut self.enable_notifications),
            "notification_timeout" => number(&mut self.notification_timeout),
            "log_notifications" => flag(&mut self.log_notifications),
            "log_level_notifications" | "log_level_runtime" => match LogLevel::parse(value) {
                Some(level) => {
                    if key == "log_level_runtime" {
                        self.log_level_runtime = level;
                    } else {
                        self.log_level_notifications = level;
                    }
                    Ok(())
                }
                None => Err(format!(
                    "Log level '{key}' has value '{value}' but it cannot be a different string \
                     than off, critical, error, err, warning, info, debug or trace"
                )),
            },
            "date_format" => {
                self.date_format = DateFormat::parse(value);
                Ok(())
            }
            "illegal_macro_output_chars" | "illegal_output_chars" => {
                self.illegal_macro_output_chars = value.to_owned();
                Ok(())
            }
            "admin_email" => {
                self.admin_email = value.trim().to_owned();
                Ok(())
            }
            "admin_pager" => {
                self.admin_pager = value.trim().to_owned();
                Ok(())
            }
            "enable_event_handlers" => flag(&mut self.enable_event_handlers),
            "global_host_event_handler" => {
                self.global_host_event_handler = value.trim().to_owned();
                Ok(())
            }
            "global_service_event_handler" => {
                self.global_service_event_handler = value.trim().to_owned();
                Ok(())
            }
            "event_handler_timeout" => number(&mut self.event_handler_timeout),
            "log_event_handlers" => flag(&mut self.log_event_handlers),
            "enable_flap_detection" => flag(&mut self.enable_flap_detection),
            "low_service_flap_threshold" => float(&mut self.low_service_flap_threshold),
            "high_service_flap_threshold" => float(&mut self.high_service_flap_threshold),
            "low_host_flap_threshold" => float(&mut self.low_host_flap_threshold),
            "high_host_flap_threshold" => float(&mut self.high_host_flap_threshold),
            "check_service_freshness" => flag(&mut self.check_service_freshness),
            "check_host_freshness" => flag(&mut self.check_host_freshness),
            "service_freshness_check_interval" => {
                number(&mut self.service_freshness_check_interval)
            }
            "host_freshness_check_interval" => number(&mut self.host_freshness_check_interval),
            "additional_freshness_latency" => number(&mut self.additional_freshness_latency),
            _ => return None,
        })
    }
}

/// Scheduling parameters shared by hosts and services.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CheckParams {
    /// `command_name!arg1!arg2…`, as written.
    pub check_command: String,
    /// In units of `interval_length`.
    pub check_interval: u32,
    /// In units of `interval_length`.
    pub retry_interval: u32,
    /// At least 1.
    pub max_check_attempts: u32,
    /// Scheduled by the scheduler.
    pub active_checks: bool,
    /// Accepts `PROCESS_*_CHECK_RESULT`.
    pub passive_checks: bool,
    /// `check_period`: active checks run in this timeperiod only (always
    /// when empty).
    pub check_period: String,
}

/// Notifications, event handler, flapping and freshness of a host or a
/// service (`host_helper.cc:258-290`, `service_helper.cc:217-247`).
#[derive(Debug, Clone, PartialEq)]
pub struct NotifyConf {
    pub notifications_enabled: bool,
    /// In units of `interval_length`; 0: no renotification. A service
    /// with 0 takes its host's (`applier/service.cc:540-580`).
    pub notification_interval: u32,
    /// In units of `interval_length`.
    pub first_notification_delay: u32,
    pub recovery_notification_delay: u32,
    /// [`objects::notify_on`] flags.
    pub notification_options: u32,
    /// Always when empty; a service without one takes its host's.
    pub notification_period: String,
    /// Contact names and contact group names; a service with neither
    /// takes those of its host.
    pub contacts: Vec<String>,
    pub contact_groups: Vec<String>,
    /// `command!args`, none when empty.
    pub event_handler: String,
    pub event_handler_enabled: bool,
    pub flap_detection_enabled: bool,
    /// States recorded in the state history ([`objects::notify_on`]).
    pub flap_detection_options: u32,
    /// Percent; 0 takes the global threshold.
    pub low_flap_threshold: f64,
    pub high_flap_threshold: f64,
    pub check_freshness: bool,
    /// Seconds; 0 computes it.
    pub freshness_threshold: u32,
    /// `is_volatile` (services only).
    pub is_volatile: bool,
}

impl NotifyConf {
    /// Engine's defaults for a host or a service.
    #[must_use]
    pub fn defaults(host: bool) -> Self {
        use objects::notify_on;
        Self {
            notifications_enabled: true,
            notification_interval: 0,
            first_notification_delay: 0,
            recovery_notification_delay: 0,
            notification_options: if host {
                notify_on::HOST_DEFAULT
            } else {
                notify_on::SERVICE_DEFAULT
            },
            notification_period: String::new(),
            contacts: Vec::new(),
            contact_groups: Vec::new(),
            event_handler: String::new(),
            event_handler_enabled: true,
            flap_detection_enabled: true,
            flap_detection_options: if host {
                notify_on::UP | notify_on::DOWN | notify_on::UNREACHABLE
            } else {
                notify_on::OK | notify_on::WARNING | notify_on::CRITICAL | notify_on::UNKNOWN
            },
            low_flap_threshold: 0.0,
            high_flap_threshold: 0.0,
            check_freshness: false,
            freshness_threshold: 0,
            is_volatile: false,
        }
    }
}

/// A host to monitor.
#[derive(Debug, Clone, PartialEq)]
pub struct HostConf {
    /// `host_name`.
    pub name: String,
    /// `_HOST_ID`, the id of the host in Centreon.
    pub host_id: u64,
    /// `alias`, `host_name` when unset.
    pub alias: String,
    /// `address`.
    pub address: String,
    /// Check parameters; an empty `check_command` means never checked.
    pub check: CheckParams,
    /// Custom macros (`_KEY value`), names upper-cased, without `_`.
    pub macros: BTreeMap<String, String>,
    /// `timezone` of its timeperiods (`TZ` syntax); `use_timezone` when empty.
    pub timezone: String,
    /// `parents`: names of its parent hosts.
    pub parents: Vec<String>,
    /// `severity_id` (`severity`): a host severity, 0 for none.
    pub severity_id: u64,
    /// `category_tags` (host categories) then `group_tags` (host groups).
    pub tags: Vec<(u64, TagType)>,
    /// `icon_id`.
    pub icon_id: u64,
    /// `acknowledgement_timeout`, in units of `interval_length`; 0: never.
    pub acknowledgement_timeout: u32,
    /// Notifications, event handler, flapping, freshness.
    pub notify: NotifyConf,
}

/// A service to monitor.
#[derive(Debug, Clone, PartialEq)]
pub struct ServiceConf {
    /// `host_name` of its host.
    pub host_name: String,
    /// Id of its host.
    pub host_id: u64,
    /// `_SERVICE_ID`.
    pub service_id: u64,
    /// `service_description`.
    pub description: String,
    /// Check parameters.
    pub check: CheckParams,
    /// Custom macros, names upper-cased, without `_`.
    pub macros: BTreeMap<String, String>,
    /// Zone of its timeperiods: its own `timezone`, else the one of its
    /// host, as Engine inherits it (`service_helper.cc:313-344`).
    pub timezone: String,
    /// `severity_id` (`severity`): a service severity, 0 for none.
    pub severity_id: u64,
    /// `category_tags` (service categories) then `group_tags` (service
    /// groups).
    pub tags: Vec<(u64, TagType)>,
    /// `icon_id`.
    pub icon_id: u64,
    /// `acknowledgement_timeout`, in units of `interval_length`; 0: never.
    pub acknowledgement_timeout: u32,
    /// Notifications, event handler, flapping, freshness.
    pub notify: NotifyConf,
}

/// Everything the scheduler needs from one configuration.
#[derive(Debug, Clone, Default, PartialEq)]
pub struct MonitoringConfig {
    /// Main file it was read from.
    pub main_file: PathBuf,
    /// Global settings.
    pub settings: Settings,
    /// `$USERn$` and other resource macros, without the dollars.
    pub users: HashMap<String, String>,
    /// Command lines by command name.
    pub commands: HashMap<String, String>,
    /// Registered hosts, in file order.
    pub hosts: Vec<HostConf>,
    /// Registered services, in file order.
    pub services: Vec<ServiceConf>,
    /// Timeperiods by `timeperiod_name`, templates resolved.
    pub timeperiods: HashMap<String, Timeperiod>,
    /// Recurring downtimes by `downtime_id`.
    pub recurring: BTreeMap<u64, RecurringConf>,
    /// Macro values starting with `encrypt::` or `raw::` are read as Engine
    /// reads them (files, document version 2), not literally (version 1).
    pub secret_prefixes: bool,
    /// Host groups by name.
    pub hostgroups: BTreeMap<String, HostGroupConf>,
    /// Service groups by name.
    pub servicegroups: BTreeMap<String, ServiceGroupConf>,
    /// Severities by `(id, type)`.
    pub severities: BTreeMap<(u64, SeverityType), SeverityConf>,
    /// Tags by `(id, type)`.
    pub tags: BTreeMap<(u64, TagType), TagConf>,
    /// Host dependencies, expanded to one per (dependent, master).
    pub host_dependencies: Vec<DependencyConf>,
    /// Service dependencies, expanded the same way.
    pub service_dependencies: Vec<DependencyConf>,
    /// Contacts by name.
    pub contacts: BTreeMap<String, ContactConf>,
    /// Contact groups by name.
    pub contactgroups: BTreeMap<String, ContactGroupConf>,
    /// Host and service escalations, one per object.
    pub escalations: Vec<EscalationConf>,
}

/// Findings of a read, in the words `centengine -v` uses.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Report {
    /// Problems that make the configuration unusable.
    pub errors: Vec<String>,
    /// Things ignored or defaulted.
    pub warnings: Vec<String>,
}

impl Report {
    /// The report as `centengine -v` ends it: one line per finding, then
    /// `Total Warnings:` and `Total Errors:`, which the agent reads.
    #[must_use]
    pub fn render(&self, objects: Option<(usize, usize, usize)>) -> String {
        let mut text = String::new();
        if let Some((commands, hosts, services)) = objects {
            let _ = writeln!(text, "Checked {commands} commands.");
            let _ = writeln!(text, "Checked {hosts} hosts.");
            let _ = writeln!(text, "Checked {services} services.");
        }
        for warning in &self.warnings {
            let _ = writeln!(text, "Warning: {warning}");
        }
        for error in &self.errors {
            let _ = writeln!(text, "Error: {error}");
        }
        let _ = writeln!(text, "Total Warnings: {}", self.warnings.len());
        let _ = writeln!(text, "Total Errors:   {}", self.errors.len());
        text
    }
}

/// Object types the engine reads; the others are skipped.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub(crate) enum Kind {
    Command,
    Host,
    Service,
    Timeperiod,
    RecurringDowntime,
    HostGroup,
    ServiceGroup,
    Severity,
    Tag,
    HostDependency,
    ServiceDependency,
    Contact,
    ContactGroup,
    HostEscalation,
    ServiceEscalation,
}

/// One `define` block, keys already mapped to their canonical name.
#[derive(Debug, Clone, Default)]
pub(crate) struct RawObject {
    pub fields: BTreeMap<String, String>,
    /// Custom variables, names upper-cased.
    pub custom: BTreeMap<String, String>,
    /// `use` list, in order.
    pub uses: Vec<String>,
    /// Lines a key may repeat, in order: weekday and exception lines of a
    /// timeperiod, `period` lines of a recurring downtime.
    pub lines: Vec<(String, String)>,
    /// `name`: a template other objects can `use`.
    pub name: Option<String>,
    pub register: bool,
    /// Where the object was defined (`file:line`, or its place in the
    /// document), for messages.
    pub origin: String,
}

/// Canonical name of `key` for `kind` (the correspondence tables of
/// `host_helper.cc` and `service_helper.cc`). `None`: a custom variable.
fn canonical(kind: Kind, key: &str) -> Option<&str> {
    let key = match (kind, key) {
        (Kind::Host, "_HOST_ID") => "host_id",
        (Kind::Host, "host_groups") => "hostgroups",
        (Kind::Service, "_SERVICE_ID") => "service_id",
        (Kind::Service, "host" | "hosts") => "host_name",
        (Kind::Service, "description") => "service_description",
        (Kind::Service, "service_groups") => "servicegroups",
        (Kind::Host | Kind::Service, "severity") => "severity_id",
        (Kind::Host | Kind::Service, "contactgroups") => "contact_groups",
        (Kind::Contact, "contact_groups") => "contactgroups",
        (Kind::HostEscalation | Kind::ServiceEscalation, key) => match key {
            "contactgroups" => "contact_groups",
            "host" => "host_name",
            "hostgroup" | "hostgroups" => "hostgroup_name",
            "servicegroup" | "servicegroups" => "servicegroup_name",
            "description" => "service_description",
            key if key.starts_with('_') => return None,
            key => key,
        },
        (Kind::Tag, "tag_id") | (Kind::Severity, "severity_id") => "id",
        (Kind::Tag, "tag_type") | (Kind::Severity, "severity_type") => "type",
        (Kind::Severity, "severity_level") => "level",
        (Kind::Severity, "severity_icon_id") => "icon_id",
        (Kind::HostDependency | Kind::ServiceDependency, key) => match key {
            "hostgroup" | "hostgroup_name" => "hostgroups",
            "host" | "host_name" | "master_host" | "master_host_name" => "hosts",
            "dependent_hostgroup" | "dependent_hostgroup_name" => "dependent_hostgroups",
            "dependent_host" | "dependent_host_name" => "dependent_hosts",
            "servicegroup" | "servicegroup_name" => "servicegroups",
            "dependent_servicegroup" | "dependent_servicegroup_name" => "dependent_servicegroups",
            "description" | "master_description" | "master_service_description" => {
                "service_description"
            }
            "dependent_description" => "dependent_service_description",
            "notification_failure_criteria" => "notification_failure_options",
            "execution_failure_criteria" => "execution_failure_options",
            key if key.starts_with('_') => return None,
            key => key,
        },
        (_, "normal_check_interval") => "check_interval",
        (_, "retry_check_interval") => "retry_interval",
        (_, "active_checks_enabled") => "checks_active",
        (_, "passive_checks_enabled") => "checks_passive",
        (_, key) if key.starts_with('_') => return None,
        (_, key) => key,
    };
    Some(key)
}

/// Lists a `+` makes additive over the templates.
const ADDITIVE: [&str; 2] = ["contacts", "contact_groups"];

/// Reader of one configuration: files, then objects, then templates.
#[derive(Default)]
struct Reader {
    report: Report,
    settings: Settings,
    users: HashMap<String, String>,
    objects: Vec<(Kind, RawObject)>,
    /// Files already read, so that a loop of `cfg_dir` never runs forever.
    seen: HashSet<PathBuf>,
}

fn read_text(path: &Path) -> Result<String, String> {
    let bytes = std::fs::read(path).map_err(|error| format!("{}: {error}", path.display()))?;
    Ok(String::from_utf8_lossy(&bytes).into_owned())
}

/// `path` resolved from the directory of the main file when relative.
fn resolve(base: &Path, path: &str) -> PathBuf {
    let path = Path::new(path);
    if path.is_absolute() {
        path.to_path_buf()
    } else {
        base.join(path)
    }
}

fn parse_u32(value: &str) -> Option<u32> {
    value.trim().parse().ok()
}

fn parse_bool(value: &str) -> Option<bool> {
    match value.trim() {
        "1" | "true" | "yes" => Some(true),
        "0" | "false" | "no" => Some(false),
        _ => None,
    }
}

impl Reader {
    fn error(&mut self, message: impl Into<String>) {
        self.report.errors.push(message.into());
    }

    fn warning(&mut self, message: impl Into<String>) {
        self.report.warnings.push(message.into());
    }

    fn main_file(&mut self, path: &Path) {
        let text = match read_text(path) {
            Ok(text) => text,
            Err(error) => return self.error(error),
        };
        let base = path.parent().unwrap_or(Path::new("/")).to_path_buf();
        let mut object_files = Vec::new();
        let mut resource_files = Vec::new();
        let mut cbmod = None;
        for line in text.lines() {
            let line = line.trim();
            if line.is_empty() || line.starts_with('#') {
                continue;
            }
            let Some((key, value)) = line.split_once('=') else {
                continue;
            };
            let (key, value) = (key.trim_end(), value.trim_start());
            let number = |reader: &mut Self, slot: &mut u32| match parse_u32(value) {
                Some(parsed) => *slot = parsed,
                None => reader.error(format!("{key}: invalid value {value:?}")),
            };
            match key {
                "cfg_file" => object_files.push(resolve(&base, value)),
                "cfg_dir" => self.directory(&resolve(&base, value), &mut object_files),
                "resource_file" => resource_files.push(resolve(&base, value)),
                "broker_module_cfg_file" => {
                    self.settings.broker_module_cfg_file = Some(resolve(&base, value));
                }
                // Engine's compatibility hook: the file given to cbmod.so.
                "broker_module" if value.contains("cbmod.so") => {
                    if cbmod.is_none() {
                        cbmod = value.split_whitespace().nth(1).map(PathBuf::from);
                    }
                }
                "interval_length" => {
                    let mut slot = self.settings.interval_length;
                    number(self, &mut slot);
                    self.settings.interval_length = slot;
                }
                "service_check_timeout" => {
                    let mut slot = self.settings.service_check_timeout;
                    number(self, &mut slot);
                    self.settings.service_check_timeout = slot;
                }
                "host_check_timeout" => {
                    let mut slot = self.settings.host_check_timeout;
                    number(self, &mut slot);
                    self.settings.host_check_timeout = slot;
                }
                "max_service_check_spread" => {
                    let mut slot = self.settings.max_service_check_spread;
                    number(self, &mut slot);
                    self.settings.max_service_check_spread = slot;
                }
                "max_host_check_spread" => {
                    let mut slot = self.settings.max_host_check_spread;
                    number(self, &mut slot);
                    self.settings.max_host_check_spread = slot;
                }
                "status_update_interval" => {
                    let mut slot = self.settings.status_update_interval;
                    number(self, &mut slot);
                    self.settings.status_update_interval = slot;
                }
                "retention_update_interval" => {
                    let mut slot = self.settings.retention_update_interval;
                    number(self, &mut slot);
                    self.settings.retention_update_interval = slot;
                }
                "use_timezone" => self.settings.use_timezone = value.trim().to_owned(),
                "use_retained_scheduling_info"
                | "log_service_retries"
                | "log_host_retries"
                | "log_external_commands"
                | "log_passive_checks"
                | "soft_state_dependencies"
                | "enable_predictive_host_dependency_checks"
                | "enable_predictive_service_dependency_checks" => match parse_bool(value) {
                    Some(flag) => {
                        let settings = &mut self.settings;
                        *match key {
                            "use_retained_scheduling_info" => {
                                &mut settings.use_retained_scheduling_info
                            }
                            "log_service_retries" => &mut settings.log_service_retries,
                            "log_host_retries" => &mut settings.log_host_retries,
                            "log_external_commands" => &mut settings.log_external_commands,
                            "log_passive_checks" => &mut settings.log_passive_checks,
                            "soft_state_dependencies" => &mut settings.soft_state_dependencies,
                            "enable_predictive_host_dependency_checks" => {
                                &mut settings.enable_predictive_host_dependency_checks
                            }
                            _ => &mut settings.enable_predictive_service_dependency_checks,
                        } = flag;
                    }
                    None => self.error(format!("{key}: invalid value {value:?}")),
                },
                "cached_host_check_horizon" => {
                    let mut slot = self.settings.cached_host_check_horizon;
                    number(self, &mut slot);
                    self.settings.cached_host_check_horizon = slot;
                }
                "log_level_events" | "log_level_checks" | "log_level_external_command" => {
                    match LogLevel::parse(value) {
                        Some(level) => {
                            let settings = &mut self.settings;
                            *match key {
                                "log_level_events" => &mut settings.log_level_events,
                                "log_level_checks" => &mut settings.log_level_checks,
                                _ => &mut settings.log_level_external_command,
                            } = level;
                        }
                        // Engine refuses the file (`state_helper::hook`).
                        None => self.error(format!(
                            "Log level '{key}' has value '{value}' but it cannot be a different \
                             string than off, critical, error, err, warning, info, debug or trace"
                        )),
                    }
                }
                key => {
                    if let Some(Err(error)) = self.settings.set_more(key, value) {
                        self.error(error);
                    }
                }
            }
        }
        if self.settings.broker_module_cfg_file.is_none() {
            self.settings.broker_module_cfg_file = cbmod;
        }
        for file in resource_files {
            self.resource_file(&file);
        }
        for file in object_files {
            self.object_file(&file);
        }
    }

    /// Every `.cfg` file under `dir`, recursively, in name order.
    fn directory(&mut self, dir: &Path, files: &mut Vec<PathBuf>) {
        let entries = match std::fs::read_dir(dir) {
            Ok(entries) => entries,
            Err(error) => return self.error(format!("{}: {error}", dir.display())),
        };
        let mut paths: Vec<PathBuf> = entries.flatten().map(|entry| entry.path()).collect();
        paths.sort();
        for path in paths {
            if path.is_dir() {
                if self.seen.insert(path.clone()) {
                    self.directory(&path, files);
                }
            } else if path.extension().is_some_and(|ext| ext == "cfg") {
                files.push(path);
            }
        }
    }

    fn resource_file(&mut self, path: &Path) {
        let text = match read_text(path) {
            Ok(text) => text,
            Err(error) => return self.error(error),
        };
        for (index, line) in text.lines().enumerate() {
            let line = line.trim_start();
            if line.is_empty() || line.starts_with('#') || line.starts_with(';') {
                continue;
            }
            let (key, value) = line.split_once('=').unwrap_or((line, ""));
            let key = key.trim_end();
            match key
                .strip_prefix('$')
                .and_then(|key| key.strip_suffix('$'))
                .filter(|key| !key.is_empty())
            {
                Some(name) => {
                    self.users
                        .insert(name.to_owned(), value.trim_start().to_owned());
                }
                None => self.error(format!(
                    "{}:{}: invalid user key '{key}'",
                    path.display(),
                    index + 1
                )),
            }
        }
    }

    fn object_file(&mut self, path: &Path) {
        if !self.seen.insert(path.to_path_buf()) {
            return;
        }
        let text = match read_text(path) {
            Ok(text) => text,
            Err(error) => return self.error(error),
        };
        // (kind, object); kind `None` for a type the scheduler skips.
        let mut current: Option<(Option<Kind>, RawObject)> = None;
        let mut continued = String::new();
        let mut appending = false;
        for (index, raw) in text.split('\n').enumerate() {
            let number = index + 1;
            let trimmed = raw.trim();
            if trimmed.is_empty() || trimmed.starts_with('#') || trimmed.starts_with(';') {
                continue;
            }
            let line: String = if appending {
                if let Some(head) = trimmed.strip_suffix('\\') {
                    continued.push_str(head);
                    continue;
                }
                continued.push_str(trimmed);
                appending = false;
                std::mem::take(&mut continued)
            } else if let Some(head) = trimmed.strip_suffix('\\') {
                continued = head.to_owned();
                appending = true;
                continue;
            } else {
                trimmed.to_owned()
            };
            let origin = format!("{}:{number}", path.display());
            match &mut current {
                Some((kind, object)) => {
                    if line == "}" {
                        let (kind, object) = (*kind, std::mem::take(object));
                        current = None;
                        if let Some(kind) = kind {
                            self.objects.push((kind, object));
                        }
                        continue;
                    }
                    let Some(kind) = *kind else {
                        continue;
                    };
                    let (key, value) = match line.find([' ', '\t']) {
                        Some(at) => (&line[..at], line[at..].trim_start()),
                        None => (line.as_str(), ""),
                    };
                    match key {
                        "use" => {
                            object.uses = value
                                .split(',')
                                .map(str::trim)
                                .filter(|name| !name.is_empty())
                                .map(str::to_owned)
                                .collect();
                        }
                        "name" => object.name = Some(value.to_owned()),
                        "register" => match parse_bool(value) {
                            Some(register) => object.register = register,
                            None => self.error(format!("{origin}: invalid register {value:?}")),
                        },
                        key if kind == Kind::Timeperiod
                            && !matches!(key, "timeperiod_name" | "alias" | "exclude") =>
                        {
                            object.lines.push((key.to_owned(), value.to_owned()));
                        }
                        "period" if kind == Kind::RecurringDowntime => {
                            object.lines.push(("period".to_owned(), value.to_owned()));
                        }
                        key => match canonical(kind, key) {
                            Some(field) => {
                                object.fields.insert(field.to_owned(), value.to_owned());
                            }
                            None => {
                                object
                                    .custom
                                    .insert(key[1..].to_ascii_uppercase(), value.to_owned());
                            }
                        },
                    }
                }
                None => {
                    let definition = line
                        .strip_prefix("define")
                        .filter(|rest| rest.starts_with([' ', '\t']))
                        .and_then(|rest| rest.trim().strip_suffix('{'))
                        .map(str::trim);
                    let Some(kind) = definition else {
                        self.error(format!("{origin}: unexpected start of definition"));
                        // Engine stops reading the file here.
                        return;
                    };
                    let kind = match kind {
                        "command" => Some(Kind::Command),
                        "host" => Some(Kind::Host),
                        "service" => Some(Kind::Service),
                        "timeperiod" => Some(Kind::Timeperiod),
                        "recurringdowntime" => Some(Kind::RecurringDowntime),
                        "hostgroup" => Some(Kind::HostGroup),
                        "servicegroup" => Some(Kind::ServiceGroup),
                        "severity" => Some(Kind::Severity),
                        "tag" => Some(Kind::Tag),
                        "hostdependency" => Some(Kind::HostDependency),
                        "servicedependency" => Some(Kind::ServiceDependency),
                        "contact" => Some(Kind::Contact),
                        "contactgroup" => Some(Kind::ContactGroup),
                        "hostescalation" => Some(Kind::HostEscalation),
                        "serviceescalation" => Some(Kind::ServiceEscalation),
                        _ => None,
                    };
                    current = Some((
                        kind,
                        RawObject {
                            register: true,
                            origin,
                            ..RawObject::default()
                        },
                    ));
                }
            }
        }
        if current.is_some() {
            self.error(format!("{}: definition not closed", path.display()));
        }
    }

    /// Fields of object `index` with its templates merged, memoized.
    fn resolved(
        &mut self,
        index: usize,
        templates: &HashMap<(Kind, String), usize>,
        cache: &mut Vec<Option<RawObject>>,
        stack: &mut Vec<usize>,
    ) -> RawObject {
        if let Some(done) = &cache[index] {
            return done.clone();
        }
        let (kind, mut object) = self.objects[index].clone();
        if stack.contains(&index) {
            self.error(format!("{}: template loop", object.origin));
            return object;
        }
        stack.push(index);
        for name in object.uses.clone() {
            let Some(&parent) = templates.get(&(kind, name.clone())) else {
                self.error(format!(
                    "{}: cannot merge object of type '{name}': no such template",
                    object.origin
                ));
                continue;
            };
            let parent = self.resolved(parent, templates, cache, stack);
            // The first template that sets a field wins over the next ones;
            // a list starting with `+` adds the template's to its own
            // (`fill_string_group`, `parser::_merge`).
            for (key, value) in parent.fields {
                match object.fields.entry(key) {
                    std::collections::btree_map::Entry::Vacant(slot) => {
                        slot.insert(value);
                    }
                    std::collections::btree_map::Entry::Occupied(mut slot)
                        if ADDITIVE.contains(&slot.key().as_str())
                            && slot.get().trim_start().starts_with('+') =>
                    {
                        let merged = format!("{},{}", slot.get(), value.trim_start_matches('+'));
                        slot.insert(merged);
                    }
                    std::collections::btree_map::Entry::Occupied(_) => {}
                }
            }
            for (key, value) in parent.custom {
                object.custom.entry(key).or_insert(value);
            }
        }
        stack.pop();
        cache[index] = Some(object.clone());
        object
    }

    fn build(mut self, main_file: &Path) -> (MonitoringConfig, Report) {
        if self.settings.interval_length == 0 {
            self.error("interval_length must be at least 1");
        }
        let mut templates = HashMap::new();
        for (index, (kind, object)) in self.objects.iter().enumerate() {
            if let Some(name) = &object.name
                && templates.insert((*kind, name.clone()), index).is_some()
            {
                let message = format!("{}: '{name}' already exists", object.origin);
                self.report.errors.push(message);
            }
        }
        let mut cache = vec![None; self.objects.len()];
        let mut config = MonitoringConfig {
            main_file: main_file.to_path_buf(),
            settings: self.settings.clone(),
            users: std::mem::take(&mut self.users),
            secret_prefixes: true,
            ..MonitoringConfig::default()
        };
        let mut hosts_by_name: HashMap<String, u64> = HashMap::new();
        let mut host_ids = HashSet::new();
        let mut service_ids = HashSet::new();
        let mut pending_services = Vec::new();
        let mut pending_relations = Vec::new();
        let mut pending_notifications = Vec::new();
        // `hostgroups` of each host: (host id, group names, origin).
        let mut host_groups_of = Vec::new();
        let mut timeperiod_cache = vec![None; self.objects.len()];
        for index in 0..self.objects.len() {
            let (kind, register) = (self.objects[index].0, self.objects[index].1.register);
            if !register {
                continue;
            }
            if kind == Kind::Timeperiod {
                let origin = self.objects[index].1.origin.clone();
                let period =
                    self.timeperiod(index, &templates, &mut timeperiod_cache, &mut Vec::new());
                if period.name.is_empty() {
                    self.error(format!(
                        "{origin}: time period has no name (property 'timeperiod_name')"
                    ));
                } else if config.timeperiods.contains_key(&period.name) {
                    self.error(format!(
                        "{origin}: timeperiod '{}' has already been defined",
                        period.name
                    ));
                } else {
                    config.timeperiods.insert(period.name.clone(), period);
                }
                continue;
            }
            let object = self.resolved(index, &templates, &mut cache, &mut Vec::new());
            match kind {
                Kind::Timeperiod => {}
                Kind::RecurringDowntime => match RecurringConf::from_raw(&object) {
                    Ok(rule) => match config.recurring.entry(rule.id) {
                        std::collections::btree_map::Entry::Occupied(_) => self.error(format!(
                            "{}: recurring downtime {} is defined twice",
                            object.origin, rule.id
                        )),
                        std::collections::btree_map::Entry::Vacant(slot) => {
                            slot.insert(rule);
                        }
                    },
                    Err(error) => self.error(format!("{}: {error}", object.origin)),
                },
                Kind::Command => self.command(&object, &mut config),
                Kind::Host => {
                    if let Some(host) = self.host(&object) {
                        if let Some(groups) = object.fields.get("hostgroups") {
                            host_groups_of.push((
                                host.host_id,
                                objects::names(groups),
                                object.origin.clone(),
                            ));
                        }
                        if !host_ids.insert(host.host_id) {
                            self.error(format!(
                                "{}: host id {} is used twice",
                                object.origin, host.host_id
                            ));
                        } else if hosts_by_name
                            .insert(host.name.clone(), host.host_id)
                            .is_some()
                        {
                            self.error(format!(
                                "{}: host '{}' is defined twice",
                                object.origin, host.name
                            ));
                        }
                        config.hosts.push(host);
                    }
                }
                Kind::Service => pending_services.push(object),
                Kind::HostGroup
                | Kind::ServiceGroup
                | Kind::Severity
                | Kind::Tag
                | Kind::HostDependency
                | Kind::ServiceDependency => pending_relations.push((kind, object)),
                Kind::Contact
                | Kind::ContactGroup
                | Kind::HostEscalation
                | Kind::ServiceEscalation => pending_notifications.push((kind, object)),
            }
        }
        // Services last: they need their host whatever the file order.
        let host_zones: HashMap<String, String> = config
            .hosts
            .iter()
            .map(|host| (host.name.clone(), host.timezone.clone()))
            .collect();
        let host_notify: HashMap<String, NotifyConf> = config
            .hosts
            .iter()
            .map(|host| (host.name.clone(), host.notify.clone()))
            .collect();
        let mut service_groups_of = Vec::new();
        for object in pending_services {
            if let Some(mut service) = self.service(&object, &hosts_by_name) {
                if service.timezone.is_empty() {
                    service.timezone = host_zones
                        .get(&service.host_name)
                        .cloned()
                        .unwrap_or_default();
                }
                if let Some(host) = host_notify.get(&service.host_name) {
                    inherit_from_host(&mut service.notify, host);
                }
                if let Some(groups) = object.fields.get("servicegroups") {
                    service_groups_of.push((
                        (service.host_id, service.service_id),
                        objects::names(groups),
                        object.origin.clone(),
                    ));
                }
                if !service_ids.insert((service.host_id, service.service_id)) {
                    self.error(format!(
                        "{}: service ({}, {}) is defined twice",
                        object.origin, service.host_id, service.service_id
                    ));
                }
                config.services.push(service);
            }
        }
        let checks = config
            .hosts
            .iter()
            .map(|host| (&host.check, format!("host '{}'", host.name)))
            .chain(config.services.iter().map(|service| {
                let what = format!(
                    "service '{}' of host '{}'",
                    service.description, service.host_name
                );
                (&service.check, what)
            }))
            .filter(|(check, _)| !check.check_command.is_empty())
            .map(|(check, what)| (command_name(&check.check_command).to_owned(), what))
            .collect::<Vec<_>>();
        for (name, what) in checks {
            if !config.commands.contains_key(&name) {
                self.error(format!("{what}: unknown check command '{name}'"));
            }
        }
        self.relations(
            &mut config,
            pending_relations,
            &host_groups_of,
            &service_groups_of,
        );
        self.notifications(&mut config, pending_notifications);
        self.check_references(&config);
        (config, self.report)
    }

    /// Timeperiods, exclusions and recurring downtime targets that must
    /// exist (`timeperiod::resolve`, `applier::host`/`service` resolution,
    /// `applier::recurringdowntime`).
    fn check_references(&mut self, config: &MonitoringConfig) {
        let mut errors = Vec::new();
        for period in config.timeperiods.values() {
            for excluded in &period.exclude {
                if !config.timeperiods.contains_key(excluded) {
                    errors.push(format!(
                        "excluded time period '{excluded}' specified in timeperiod '{}' is not \
                         defined anywhere",
                        period.name
                    ));
                }
            }
        }
        let periods = config
            .hosts
            .iter()
            .map(|host| (&host.check.check_period, format!("host '{}'", host.name)))
            .chain(config.services.iter().map(|service| {
                (
                    &service.check.check_period,
                    format!(
                        "service '{}' on host '{}'",
                        service.description, service.host_name
                    ),
                )
            }));
        for (period, what) in periods {
            if !period.is_empty() && !config.timeperiods.contains_key(period) {
                errors.push(format!(
                    "check period '{period}' specified for {what} is not defined anywhere"
                ));
            }
        }
        errors.sort();
        self.report.errors.extend(errors);
        let host_ids: HashSet<u64> = config.hosts.iter().map(|h| h.host_id).collect();
        let service_ids: HashSet<(u64, u64)> = config
            .services
            .iter()
            .map(|s| (s.host_id, s.service_id))
            .collect();
        for rule in config.recurring.values() {
            for host_id in &rule.host_ids {
                if !host_ids.contains(host_id) {
                    self.warning(format!(
                        "recurring downtime {}: host {host_id} is not on this poller, ignored",
                        rule.id
                    ));
                }
            }
            for (host_id, service_id) in &rule.service_ids {
                if !service_ids.contains(&(*host_id, *service_id)) {
                    self.warning(format!(
                        "recurring downtime {}: service {host_id}:{service_id} is not on this \
                         poller, ignored",
                        rule.id
                    ));
                }
            }
        }
    }

    /// Timeperiod of object `index`, its `use` templates merged in order
    /// as `parser::_merge` does, memoized.
    fn timeperiod(
        &mut self,
        index: usize,
        templates: &HashMap<(Kind, String), usize>,
        cache: &mut Vec<Option<Timeperiod>>,
        stack: &mut Vec<usize>,
    ) -> Timeperiod {
        if let Some(done) = &cache[index] {
            return done.clone();
        }
        let object = self.objects[index].1.clone();
        let mut period = Timeperiod {
            name: object
                .fields
                .get("timeperiod_name")
                .cloned()
                .unwrap_or_default(),
            alias: object.fields.get("alias").cloned().unwrap_or_default(),
            ..Timeperiod::default()
        };
        // `+a,b`: the exclusions of the templates are added to these.
        let mut additive = false;
        if let Some(exclude) = object.fields.get("exclude") {
            let exclude = exclude.trim();
            additive = exclude.starts_with('+');
            period.exclude = exclude
                .trim_start_matches('+')
                .split(',')
                .map(str::trim)
                .filter(|name| !name.is_empty())
                .map(str::to_owned)
                .collect();
        }
        for (key, value) in &object.lines {
            if let Err(error) = period.add_line(key, value) {
                self.error(format!("{}: {error}", object.origin));
            }
        }
        if stack.contains(&index) {
            self.error(format!("{}: template loop", object.origin));
            return period;
        }
        stack.push(index);
        for name in &object.uses {
            let Some(&parent) = templates.get(&(Kind::Timeperiod, name.clone())) else {
                self.error(format!(
                    "{}: cannot merge object of type '{name}': no such template",
                    object.origin
                ));
                continue;
            };
            let parent = self.timeperiod(parent, templates, cache, stack);
            if additive {
                for name in &parent.exclude {
                    if !period.exclude.contains(name) {
                        period.exclude.push(name.clone());
                    }
                }
            }
            period.merge(&parent);
        }
        stack.pop();
        cache[index] = Some(period.clone());
        period
    }

    fn command(&mut self, object: &RawObject, config: &mut MonitoringConfig) {
        let name = object
            .fields
            .get("command_name")
            .cloned()
            .unwrap_or_default();
        let line = object
            .fields
            .get("command_line")
            .cloned()
            .unwrap_or_default();
        if name.is_empty() || line.is_empty() {
            self.error(format!(
                "{}: a command needs command_name and command_line",
                object.origin
            ));
            return;
        }
        if object.fields.contains_key("connector") {
            self.warning(format!(
                "{}: command '{name}' uses a connector; it runs as a plain process",
                object.origin
            ));
        }
        config.commands.insert(name, line);
    }

    fn check_params(&mut self, object: &RawObject) -> CheckParams {
        let mut number = |key: &str, default: u32| match object.fields.get(key) {
            Some(value) => parse_u32(value).unwrap_or_else(|| {
                self.report
                    .errors
                    .push(format!("{}: invalid {key} {value:?}", object.origin));
                default
            }),
            None => default,
        };
        let check_interval = number("check_interval", 5);
        let retry_interval = number("retry_interval", 1);
        let max_check_attempts = number("max_check_attempts", 3).max(1);
        let mut flag = |key: &str| match object.fields.get(key) {
            Some(value) => parse_bool(value).unwrap_or_else(|| {
                self.report
                    .errors
                    .push(format!("{}: invalid {key} {value:?}", object.origin));
                true
            }),
            None => true,
        };
        CheckParams {
            check_command: object
                .fields
                .get("check_command")
                .cloned()
                .unwrap_or_default(),
            check_interval,
            retry_interval,
            max_check_attempts,
            active_checks: flag("checks_active"),
            passive_checks: flag("checks_passive"),
            check_period: object
                .fields
                .get("check_period")
                .map(|period| period.trim().to_owned())
                .unwrap_or_default(),
        }
    }

    fn id(&mut self, object: &RawObject, key: &str) -> Option<u64> {
        let value = object.fields.get(key)?;
        match value.trim().parse::<u64>() {
            Ok(id) if id > 0 => Some(id),
            _ => {
                self.error(format!("{}: invalid {key} {value:?}", object.origin));
                None
            }
        }
    }

    fn host(&mut self, object: &RawObject) -> Option<HostConf> {
        let Some(name) = object.fields.get("host_name").filter(|n| !n.is_empty()) else {
            self.error(format!("{}: a host needs host_name", object.origin));
            return None;
        };
        let name = name.clone();
        let Some(host_id) = self.id(object, "host_id") else {
            self.error(format!("{}: host '{name}' has no _HOST_ID", object.origin));
            return None;
        };
        let check = self.check_params(object);
        Some(HostConf {
            alias: object
                .fields
                .get("alias")
                .filter(|alias| !alias.is_empty())
                .cloned()
                .unwrap_or_else(|| name.clone()),
            address: object.fields.get("address").cloned().unwrap_or_default(),
            name,
            host_id,
            check,
            macros: object.custom.clone(),
            timezone: object
                .fields
                .get("timezone")
                .map(|zone| zone.trim().to_owned())
                .unwrap_or_default(),
            parents: object
                .fields
                .get("parents")
                .map(|parents| objects::names(parents))
                .unwrap_or_default(),
            severity_id: self.number(object, "severity_id"),
            tags: self.tags(object, TagType::HostCategory, TagType::HostGroup),
            icon_id: self.number(object, "icon_id"),
            acknowledgement_timeout: u32::try_from(self.number(object, "acknowledgement_timeout"))
                .unwrap_or(u32::MAX),
            notify: self.notify_conf(object, true),
        })
    }

    /// An optional number field, 0 when absent.
    fn number(&mut self, object: &RawObject, key: &str) -> u64 {
        match object.fields.get(key).map(|value| value.trim()) {
            None | Some("") => 0,
            Some(value) => value.parse().unwrap_or_else(|_| {
                self.error(format!("{}: invalid {key} {value:?}", object.origin));
                0
            }),
        }
    }

    /// `category_tags` then `group_tags` of a host or a service
    /// (`host_helper::hook`, `service_helper::hook`).
    fn tags(
        &mut self,
        object: &RawObject,
        category: TagType,
        group: TagType,
    ) -> Vec<(u64, TagType)> {
        let mut tags = Vec::new();
        for (key, kind) in [("category_tags", category), ("group_tags", group)] {
            let Some(value) = object.fields.get(key) else {
                continue;
            };
            match objects::ids(value) {
                Some(ids) => tags.extend(ids.into_iter().map(|id| (id, kind))),
                None => self.error(format!("{}: invalid {key} {value:?}", object.origin)),
            }
        }
        tags
    }

    fn service(&mut self, object: &RawObject, hosts: &HashMap<String, u64>) -> Option<ServiceConf> {
        let description = object
            .fields
            .get("service_description")
            .filter(|d| !d.is_empty())
            .cloned();
        let Some(description) = description else {
            self.error(format!(
                "{}: services must have a non-empty description",
                object.origin
            ));
            return None;
        };
        let host_name = object.fields.get("host_name").cloned().unwrap_or_default();
        let Some(&host_id) = hosts.get(&host_name) else {
            self.error(format!(
                "{}: service '{description}' is on host '{host_name}', which does not exist",
                object.origin
            ));
            return None;
        };
        let Some(service_id) = self.id(object, "service_id") else {
            self.error(format!(
                "{}: service '{description}' has no _SERVICE_ID",
                object.origin
            ));
            return None;
        };
        let check = self.check_params(object);
        if check.check_command.is_empty() {
            self.error(format!(
                "{}: service '{description}' has an empty check command",
                object.origin
            ));
            return None;
        }
        Some(ServiceConf {
            host_name,
            host_id,
            service_id,
            description,
            check,
            macros: object.custom.clone(),
            timezone: object
                .fields
                .get("timezone")
                .map(|zone| zone.trim().to_owned())
                .unwrap_or_default(),
            severity_id: self.number(object, "severity_id"),
            tags: self.tags(object, TagType::ServiceCategory, TagType::ServiceGroup),
            icon_id: self.number(object, "icon_id"),
            acknowledgement_timeout: u32::try_from(self.number(object, "acknowledgement_timeout"))
                .unwrap_or(u32::MAX),
            notify: self.notify_conf(object, false),
        })
    }
}

/// What a service takes from its host (`applier/service.cc:540-580`):
/// contacts and contact groups when it has neither, the notification
/// interval when its own is 0, the notification period when it has none.
fn inherit_from_host(service: &mut NotifyConf, host: &NotifyConf) {
    if service.contacts.is_empty() && service.contact_groups.is_empty() {
        service.contacts.clone_from(&host.contacts);
        service.contact_groups.clone_from(&host.contact_groups);
    }
    if service.notification_interval == 0 {
        service.notification_interval = host.notification_interval;
    }
    if service.notification_period.is_empty() {
        service
            .notification_period
            .clone_from(&host.notification_period);
    }
}

/// Name of the command of a `check_command` value: what is before the
/// first `!`.
#[must_use]
pub fn command_name(check_command: &str) -> &str {
    check_command
        .split_once('!')
        .map_or(check_command, |(name, _)| name)
        .trim()
}

/// Resolves the templates of `objects` and checks the result, as for the
/// files: what the structured document goes through.
#[must_use]
pub(crate) fn build(
    settings: Settings,
    users: HashMap<String, String>,
    objects: Vec<(Kind, RawObject)>,
    report: Report,
) -> (MonitoringConfig, Report) {
    let reader = Reader {
        report,
        settings,
        users,
        objects,
        seen: HashSet::new(),
    };
    reader.build(Path::new(""))
}

/// Reads the configuration whose main file is `main_file`. The report holds
/// the errors; the configuration is usable only when it has none.
#[must_use]
pub fn read(main_file: &Path) -> (MonitoringConfig, Report) {
    let mut reader = Reader::default();
    reader.main_file(main_file);
    reader.build(main_file)
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;

    /// Files shaped like what centreon-web generates for a poller.
    pub(crate) fn web_like(dir: &Path) -> PathBuf {
        let write = |name: &str, text: &str| std::fs::write(dir.join(name), text).unwrap();
        write(
            "centengine.cfg",
            &format!(
                "# generated\ncfg_file={0}/hostTemplates.cfg\ncfg_file={0}/hosts.cfg\n\
                 cfg_file={0}/serviceTemplates.cfg\ncfg_file={0}/services.cfg\n\
                 cfg_file=commands.cfg\ncfg_file={0}/timeperiods.cfg\n\
                 cfg_file={0}/groups.cfg\ncfg_file={0}/tags.cfg\n\
                 cfg_file={0}/severities.cfg\ncfg_file={0}/dependencies.cfg\n\
                 cfg_file={0}/contacts.cfg\nenable_flap_detection=1\nnotification_timeout=12\n\
                 resource_file={0}/resource.cfg\ninterval_length=60\n\
                 service_check_timeout=20\nretention_update_interval=30\n\
                 log_service_retries=1\nlog_level_external_command=info\n\
                 broker_module_cfg_file=/etc/centreon-broker/p-module.json\n\
                 log_file=/var/log/centreon-engine/centengine.log\nunknown_key=1\n",
                dir.display()
            ),
        );
        write(
            "resource.cfg",
            "$USER1$=/usr/lib/nagios/plugins\n# comment\n$CENTREONPLUGINS$=/usr/lib/centreon/plugins\n",
        );
        write(
            "groups.cfg",
            "define hostgroup {\n    hostgroup_id 20\n    hostgroup_name hg1\n    \
             alias First hosts\n    members h1,h2\n}\n\
             define servicegroup {\n    servicegroup_id 30\n    servicegroup_name sg1\n    \
             alias Some services\n    members h1,Ping,h2,Disk\n}\n",
        );
        write(
            "tags.cfg",
            "define tag {\n    id 7\n    tag_name cat7\n    type hostcategory\n}\n\
             define tag {\n    id 20\n    tag_name hg1\n    type hostgroup\n}\n\
             define tag {\n    id 30\n    tag_name sg1\n    type servicegroup\n}\n",
        );
        write(
            "severities.cfg",
            "define severity {\n    id 3\n    severity_name high\n    level 1\n    \
             icon_id 9\n    type host\n}\n\
             define severity {\n    id 4\n    severity_name low\n    level 5\n    \
             type service\n}\n",
        );
        write(
            "dependencies.cfg",
            "define servicedependency {\n    ;dependency_name ping-disk\n    \
             execution_failure_criteria c,u\n    notification_failure_criteria w,c\n    \
             inherits_parent 1\n    host_name h1\n    service_description Ping\n    \
             dependent_host_name h2\n    dependent_service_description Disk\n}\n\
             define hostdependency {\n    execution_failure_criteria d\n    \
             dependency_period 24x7\n    hostgroup_name hg1\n    dependent_host_name h1\n}\n",
        );
        write(
            "commands.cfg",
            "define command {\n    command_name                   check_dummy\n    \
             command_line                   $USER1$/check_dummy $ARG1$ \"$ARG2$\"\n}\n\
             define command {\n    command_name notify\n    command_line /bin/true $CONTACTEMAIL$\n}\n",
        );
        write(
            "contacts.cfg",
            "define contact {\n    name tpl-contact\n    host_notification_commands notify\n    \
             register 0\n}\n\
             define contact {\n    contact_name admin\n    alias Administrator\n    \
             email admin@localhost\n    service_notification_options w,c,r,f\n    \
             service_notification_commands notify\n    contactgroups team\n}\n\
             define contact {\n    contact_name guest\n    use tpl-contact\n}\n\
             define contactgroup {\n    contactgroup_name ops\n    alias Operators\n    \
             members guest\n    contactgroup_members team\n}\n\
             define contactgroup {\n    contactgroup_name team\n    alias Team\n}\n\
             define serviceescalation {\n    host_name h2\n    service_description Disk\n    \
             first_notification 2\n    last_notification 0\n    notification_interval 5\n    \
             escalation_options c,r\n    contact_groups ops\n}\n\
             define hostescalation {\n    host_name h1\n    first_notification 1\n    \
             escalation_options d\n    contact_groups team\n}\n",
        );
        write(
            "timeperiods.cfg",
            "define timeperiod {\n    name 24x7\n    timeperiod_name 24x7\n    alias Always\n    \
             sunday 00:00-24:00\n    monday 00:00-24:00\n}\n\
             define timeperiod {\n    name workhours\n    timeperiod_name workhours\n    \
             alias Work\n    use 24x7\n    monday 09:00-17:00\n    december 25 00:00-24:00\n    \
             exclude nonworkhours\n}\n\
             define timeperiod {\n    name nonworkhours\n    timeperiod_name nonworkhours\n    \
             alias Nights\n    2026-12-24 - 2026-12-26 00:00-24:00\n}\n\
             define recurringdowntime {\n    downtime_id 5\n    downtime_name maintenance\n    \
             host_ids 12\n    service_ids 13:102\n    period weekly:1,2 22:00-23:00 fixed\n    \
             period monthly:1 01:00-02:00 flexible:600\n}\n",
        );
        write(
            "hostTemplates.cfg",
            "define host {\n    name                           generic-host\n    \
             check_command                  check_dummy!0!up\n    max_check_attempts 2\n    \
             _SNMPCOMMUNITY public\n    register 0\n}\n\
             define host {\n    name other\n    max_check_attempts 9\n    check_interval 7\n    \
             _SNMPCOMMUNITY private\n    _EXTRA x\n    register 0\n}\n",
        );
        write(
            "hosts.cfg",
            "define host {\n    host_name                      h1\n    alias \
             First\n    address                        10.0.0.1\n    use \
             generic-host,other\n    _HOST_ID                       12\n    register 1\n    \
             check_period workhours\n    timezone :Europe/Paris\n    parents h2\n    \
             category_tags 7\n    group_tags 20\n    severity 3\n    \
             acknowledgement_timeout 5\n    contacts admin\n    notification_interval 10\n    \
             notification_options d,r\n}\n\
             define host {\n    host_name h2\n    address 10.0.0.2\n    _HOST_ID 13\n}\n",
        );
        write(
            "serviceTemplates.cfg",
            "define service {\n    name svc-tpl\n    service_description tpl\n    \
             check_command check_dummy!0!ok\n    normal_check_interval 3\n    \
             retry_check_interval 1\n    max_check_attempts 3\n    register 0\n}\n",
        );
        write(
            "services.cfg",
            "define service {\n    host_name h1\n    service_description Ping\n    use \
             svc-tpl\n    _SERVICE_ID 101\n    register 1\n    group_tags 30\n    severity 4\n    \
             event_handler check_dummy!0!handled\n    check_freshness 1\n    \
             high_flap_threshold 50\n}\n\
             define service {\n    host_name h2\n    service_description Disk\n    \
             check_command check_dummy!2!\\\n    ;a comment line is skipped\n    full\n    \
             active_checks_enabled 0\n    _SERVICE_ID 102\n    _CRIT 90\n    \
             check_period 24x7\n    contact_groups ops\n}\n",
        );
        dir.join("centengine.cfg")
    }

    #[test]
    fn reads_web_output_with_templates() {
        let dir = tempfile::tempdir().unwrap();
        let (config, report) = read(&web_like(dir.path()));
        assert!(report.errors.is_empty(), "{report:?}");
        assert_eq!(config.settings.service_check_timeout, 20);
        assert_eq!(
            config.settings.broker_module_cfg_file.as_deref(),
            Some(Path::new("/etc/centreon-broker/p-module.json"))
        );
        assert_eq!(config.users["USER1"], "/usr/lib/nagios/plugins");
        assert_eq!(
            config.commands["check_dummy"],
            "$USER1$/check_dummy $ARG1$ \"$ARG2$\""
        );

        let h1 = &config.hosts[0];
        assert_eq!((h1.name.as_str(), h1.host_id), ("h1", 12));
        assert_eq!(h1.alias, "First");
        // First template wins; fields only the second sets come from it.
        assert_eq!(h1.check.max_check_attempts, 2);
        assert_eq!(h1.check.check_interval, 7);
        assert_eq!(h1.check.check_command, "check_dummy!0!up");
        assert_eq!(h1.macros["SNMPCOMMUNITY"], "public");
        assert_eq!(h1.macros["EXTRA"], "x");
        // _HOST_ID is the id, not a macro.
        assert!(!h1.macros.contains_key("HOST_ID"));

        let h2 = &config.hosts[1];
        assert_eq!(h2.alias, "h2");
        assert!(h2.check.check_command.is_empty());
        assert_eq!(h2.check.max_check_attempts, 3);

        let ping = &config.services[0];
        assert_eq!((ping.host_id, ping.service_id), (12, 101));
        assert_eq!(ping.description, "Ping");
        assert_eq!(
            (ping.check.check_interval, ping.check.retry_interval),
            (3, 1)
        );
        let disk = &config.services[1];
        assert_eq!(disk.check.check_command, "check_dummy!2!full");
        assert!(!disk.check.active_checks && disk.check.passive_checks);
        assert_eq!(disk.macros["CRIT"], "90");
        assert_eq!(disk.check.check_interval, 5);
        // Timeperiods: own weekdays win, others and exceptions are merged.
        assert_eq!(config.settings.retention_update_interval, 30);
        let work = &config.timeperiods["workhours"];
        assert_eq!(work.days[1].len(), 1);
        assert_eq!(work.days[0].len(), 1);
        assert_eq!(work.exclude, ["nonworkhours"]);
        assert_eq!(h1.check.check_period, "workhours");
        assert_eq!(h1.timezone, ":Europe/Paris");
        // A service has the zone of its host.
        assert_eq!(ping.timezone, ":Europe/Paris");
        assert_eq!(disk.check.check_period, "24x7");
        // Relations.
        assert_eq!(h1.parents, ["h2"]);
        assert_eq!(
            h1.tags,
            [(7, TagType::HostCategory), (20, TagType::HostGroup)]
        );
        assert_eq!((h1.severity_id, h1.acknowledgement_timeout), (3, 5));
        assert_eq!(config.hostgroups["hg1"].members, [12, 13]);
        assert_eq!(config.hostgroups["hg1"].alias, "First hosts");
        assert_eq!(config.servicegroups["sg1"].members, [(12, 101), (13, 102)]);
        assert_eq!(ping.severity_id, 4);
        assert_eq!(config.severities[&(3, SeverityType::Host)].icon_id, 9);
        assert_eq!(config.tags.len(), 3);
        let dependency = &config.service_dependencies[0];
        assert_eq!(
            (dependency.dependent, dependency.master),
            ((13, 102), (12, 101))
        );
        assert!(dependency.execution.on(2) && dependency.execution.on(3));
        assert!(!dependency.execution.on(1) && dependency.notification.on(1));
        assert!(dependency.inherits_parent);
        // A host group master: one dependency per member, the dependent itself included.
        assert_eq!(config.host_dependencies.len(), 2);
        assert!(config.settings.log_service_retries);
        assert_eq!(config.settings.log_level_external_command, LogLevel::Info);
        let rule = &config.recurring[&5];
        assert_eq!(
            (rule.host_ids.clone(), rule.service_ids.clone()),
            (vec![12], vec![(13, 102)])
        );
        assert_eq!(rule.periods.len(), 2);
        assert_eq!(report.warnings.len(), 0, "{report:?}");
    }

    #[test]
    fn refuses_unknown_timeperiods() {
        let dir = tempfile::tempdir().unwrap();
        let main = web_like(dir.path());
        let path = dir.path().join("timeperiods.cfg");
        let text = std::fs::read_to_string(&path).unwrap();
        std::fs::write(
            &path,
            text.replace("exclude nonworkhours", "exclude nope")
                .replace("check_period workhours", "")
                .replace("monday 09:00-17:00", "monday 9h"),
        )
        .unwrap();
        let hosts = dir.path().join("hosts.cfg");
        let text = std::fs::read_to_string(&hosts).unwrap();
        std::fs::write(
            &hosts,
            text.replace("check_period workhours", "check_period missing"),
        )
        .unwrap();
        let (_, report) = read(&main);
        let all = report.errors.join("\n");
        assert!(all.contains("excluded time period 'nope'"), "{all}");
        assert!(
            all.contains("check period 'missing' specified for host 'h1'"),
            "{all}"
        );
        assert!(all.contains("invalid timeperiod line 'monday 9h'"), "{all}");
    }

    #[test]
    fn reports_what_engine_refuses() {
        let dir = tempfile::tempdir().unwrap();
        let main = web_like(dir.path());
        std::fs::write(
            dir.path().join("services.cfg"),
            "define service {\n    host_name nope\n    service_description A\n    \
             check_command check_dummy\n    _SERVICE_ID 1\n}\n\
             define service {\n    host_name h1\n    service_description B\n    use missing\n    \
             check_command unknown!1\n    _SERVICE_ID 2\n}\n\
             define service {\n    host_name h1\n    service_description C\n    check_command check_dummy\n}\n",
        )
        .unwrap();
        let (_, report) = read(&main);
        let all = report.errors.join("\n");
        assert!(all.contains("'nope', which does not exist"), "{all}");
        assert!(all.contains("'missing': no such template"), "{all}");
        assert!(all.contains("unknown check command 'unknown'"), "{all}");
        assert!(all.contains("has no _SERVICE_ID"), "{all}");
        let rendered = report.render(None);
        assert!(rendered.ends_with("Total Errors:   9\n"), "{rendered}");

        std::fs::write(dir.path().join("services.cfg"), "service {\n").unwrap();
        let (_, report) = read(&main);
        assert!(report.errors[0].contains("unexpected start"), "{report:?}");
        let (_, report) = read(&dir.path().join("absent.cfg"));
        assert_eq!(report.errors.len(), 1);
    }

    #[test]
    fn refuses_unknown_relations() {
        let dir = tempfile::tempdir().unwrap();
        let main = web_like(dir.path());
        let hosts = dir.path().join("hosts.cfg");
        let text = std::fs::read_to_string(&hosts).unwrap();
        std::fs::write(
            &hosts,
            text.replace("parents h2", "parents nope\n    hostgroups missing")
                .replace("category_tags 7", "category_tags 8")
                .replace("severity 3", "severity 4"),
        )
        .unwrap();
        let groups = dir.path().join("groups.cfg");
        let text = std::fs::read_to_string(&groups).unwrap();
        std::fs::write(&groups, text.replace("h2,Disk", "h2,Nope")).unwrap();
        std::fs::write(
            dir.path().join("dependencies.cfg"),
            "define servicedependency {\n    host_name h1\n    service_description Ping\n}\n\
             define hostdependency {\n    host_name h1\n    dependent_host_name h2\n    \
             execution_failure_criteria x\n}\n",
        )
        .unwrap();
        let (_, report) = read(&main);
        let all = report.errors.join("\n");
        for expected in [
            "'nope' is not a valid parent for host 'h1'!",
            "Could not add host 'h1' to non-existing host group 'missing'",
            "Could not find tag '8' on which to apply host (12)",
            "Could not add the severity (4, 1) to the host 'h1'",
            "Service 'Nope' on host 'h2' specified in service group 'sg1' is not defined",
            "Service dependency is not attached to any dependent service",
            "invalid execution_failure_options \"x\"",
        ] {
            assert!(all.contains(expected), "{expected}: {all}");
        }
    }

    #[test]
    fn command_name_stops_at_the_first_bang() {
        assert_eq!(command_name("check!a!b"), "check");
        assert_eq!(command_name("check"), "check");
    }
}
