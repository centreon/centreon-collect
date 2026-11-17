//! Reading of the Engine configuration centreon-web generates, reduced to
//! what the engine runs: global settings, commands, hosts, services,
//! their templates, and the `$USERn$` resource macros. Same syntax and
//! template rules as `common/engine_conf/parser.cc`. The structured
//! document (`document.rs`) is turned into the same raw objects and goes
//! through the same template resolution and checks ([`build`]).

use std::collections::{BTreeMap, HashMap, HashSet};
use std::fmt::Write as _;
use std::path::{Path, PathBuf};

/// Global settings of `centengine.cfg` the scheduler uses.
#[derive(Debug, Clone, PartialEq, Eq)]
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
    /// The `cbmod` configuration file (poller identity, central address).
    pub broker_module_cfg_file: Option<PathBuf>,
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
            broker_module_cfg_file: None,
        }
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
}

/// A host to monitor.
#[derive(Debug, Clone, PartialEq, Eq)]
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
}

/// A service to monitor.
#[derive(Debug, Clone, PartialEq, Eq)]
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
}

/// Everything the scheduler needs from one configuration.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
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
}

/// One `define` block, keys already mapped to their canonical name.
#[derive(Debug, Clone, Default)]
pub(crate) struct RawObject {
    pub fields: BTreeMap<String, String>,
    /// Custom variables, names upper-cased.
    pub custom: BTreeMap<String, String>,
    /// `use` list, in order.
    pub uses: Vec<String>,
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
        (Kind::Service, "_SERVICE_ID") => "service_id",
        (Kind::Service, "host" | "hosts") => "host_name",
        (Kind::Service, "description") => "service_description",
        (_, "normal_check_interval") => "check_interval",
        (_, "retry_check_interval") => "retry_interval",
        (_, "active_checks_enabled") => "checks_active",
        (_, "passive_checks_enabled") => "checks_passive",
        (_, key) if key.starts_with('_') => return None,
        (_, key) => key,
    };
    Some(key)
}

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
                _ => {}
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
            // The first template that sets a field wins over the next ones.
            for (key, value) in parent.fields {
                object.fields.entry(key).or_insert(value);
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
            ..MonitoringConfig::default()
        };
        let mut hosts_by_name: HashMap<String, u64> = HashMap::new();
        let mut host_ids = HashSet::new();
        let mut service_ids = HashSet::new();
        let mut pending_services = Vec::new();
        for index in 0..self.objects.len() {
            let (kind, register) = (self.objects[index].0, self.objects[index].1.register);
            if !register {
                continue;
            }
            let object = self.resolved(index, &templates, &mut cache, &mut Vec::new());
            match kind {
                Kind::Command => self.command(&object, &mut config),
                Kind::Host => {
                    if let Some(host) = self.host(&object) {
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
            }
        }
        // Services last: they need their host whatever the file order.
        for object in pending_services {
            if let Some(service) = self.service(&object, &hosts_by_name) {
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
        (config, self.report)
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
        })
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
        })
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
                 resource_file={0}/resource.cfg\ninterval_length=60\n\
                 service_check_timeout=20\nbroker_module_cfg_file=/etc/centreon-broker/p-module.json\n\
                 log_file=/var/log/centreon-engine/centengine.log\nunknown_key=1\n",
                dir.display()
            ),
        );
        write(
            "resource.cfg",
            "$USER1$=/usr/lib/nagios/plugins\n# comment\n$CENTREONPLUGINS$=/usr/lib/centreon/plugins\n",
        );
        write(
            "commands.cfg",
            "define command {\n    command_name                   check_dummy\n    \
             command_line                   $USER1$/check_dummy $ARG1$ \"$ARG2$\"\n}\n",
        );
        write(
            "timeperiods.cfg",
            "define timeperiod {\n    timeperiod_name 24x7\n    sunday 00:00-24:00\n}\n",
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
             generic-host,other\n    _HOST_ID                       12\n    register 1\n}\n\
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
             svc-tpl\n    _SERVICE_ID 101\n    register 1\n}\n\
             define service {\n    host_name h2\n    service_description Disk\n    \
             check_command check_dummy!2!\\\n    ;a comment line is skipped\n    full\n    \
             active_checks_enabled 0\n    _SERVICE_ID 102\n    _CRIT 90\n}\n",
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
        assert!(rendered.ends_with("Total Errors:   4\n"), "{rendered}");

        std::fs::write(dir.path().join("services.cfg"), "service {\n").unwrap();
        let (_, report) = read(&main);
        assert!(report.errors[0].contains("unexpected start"), "{report:?}");
        let (_, report) = read(&dir.path().join("absent.cfg"));
        assert_eq!(report.errors.len(), 1);
    }

    #[test]
    fn command_name_stops_at_the_first_bang() {
        assert_eq!(command_name("check!a!b"), "check");
        assert_eq!(command_name("check"), "check");
    }
}
