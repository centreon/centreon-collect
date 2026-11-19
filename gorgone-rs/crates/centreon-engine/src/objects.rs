//! Objects that relate hosts and services, as Engine's configuration
//! model has them (`common/engine_conf/state.proto`): host and service
//! groups, severities, tags and dependencies, and how their `.cfg` values
//! read (`hostgroup_helper.cc`, `servicegroup_helper.cc`,
//! `severity_helper.cc`, `tag_helper.cc`, `hostdependency_helper.cc`,
//! `servicedependency_helper.cc`).

/// `(host_id, service_id)`; `service_id` 0 for a host.
pub type Key = (u64, u64);

/// Type of a tag (`engine::tag::tagtype`, `TagType` of `bbdo/neb.proto`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum TagType {
    ServiceGroup = 0,
    HostGroup = 1,
    ServiceCategory = 2,
    HostCategory = 3,
}

impl TagType {
    /// The `type` value of a `define tag` (`tag_helper::hook`).
    #[must_use]
    pub fn parse(value: &str) -> Option<Self> {
        match value.trim() {
            "hostcategory" => Some(Self::HostCategory),
            "servicecategory" => Some(Self::ServiceCategory),
            "hostgroup" => Some(Self::HostGroup),
            "servicegroup" => Some(Self::ServiceGroup),
            _ => None,
        }
    }
}

/// Type of a severity (`engine::severity::severity_type`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum SeverityType {
    Service = 0,
    Host = 1,
}

impl SeverityType {
    /// The `type` value of a `define severity` (`severity_helper::hook`).
    #[must_use]
    pub fn parse(value: &str) -> Option<Self> {
        match value.trim() {
            "host" => Some(Self::Host),
            "service" => Some(Self::Service),
            _ => None,
        }
    }
}

/// `define severity`, keyed by `(id, type)`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SeverityConf {
    pub id: u64,
    pub kind: SeverityType,
    pub name: String,
    /// At least 1; 1 is the most important.
    pub level: u32,
    pub icon_id: u64,
}

/// `define tag`, keyed by `(id, type)`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TagConf {
    pub id: u64,
    pub kind: TagType,
    pub name: String,
}

/// `define hostgroup`, members resolved to host ids.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct HostGroupConf {
    /// `hostgroup_id`: no event is sent for a group without one
    /// (`forward_pb_group`, `engine/src/broker.cc`).
    pub id: u64,
    pub name: String,
    pub alias: String,
    /// Host ids, sorted.
    pub members: Vec<u64>,
}

/// `define servicegroup`, members resolved to `(host_id, service_id)`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ServiceGroupConf {
    pub id: u64,
    pub name: String,
    pub alias: String,
    /// Sorted.
    pub members: Vec<Key>,
}

/// States a dependency fails on (`action_hd_*`, `action_sd_*`): for a
/// host UP, DOWN, UNREACHABLE; for a service OK, WARNING, CRITICAL,
/// UNKNOWN; and pending (never checked).
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct FailOn {
    /// Indexed by state code.
    pub states: [bool; 4],
    pub pending: bool,
}

impl FailOn {
    /// `execution_failure_criteria` / `notification_failure_criteria`
    /// (`hostdependency_helper::hook`, `servicedependency_helper::hook`):
    /// `None` on an unknown letter. For a service, `a` (all) leaves
    /// UNKNOWN out, as Engine does.
    #[must_use]
    pub fn parse(value: &str, host: bool) -> Option<Self> {
        let mut fail = Self::default();
        for item in value.split(',') {
            let item = item.trim();
            let state = if host {
                match item {
                    "o" | "up" => Some(0),
                    "d" | "down" => Some(1),
                    "u" | "unreachable" => Some(2),
                    "p" | "pending" => {
                        fail.pending = true;
                        None
                    }
                    // `options |= action_hd_none`: changes nothing.
                    "n" | "none" => None,
                    "a" | "all" => {
                        fail = Self {
                            states: [true, true, true, false],
                            pending: true,
                        };
                        None
                    }
                    _ => return None,
                }
            } else {
                match item {
                    "o" | "ok" => Some(0),
                    "w" | "warning" => Some(1),
                    "c" | "critical" => Some(2),
                    "u" | "unknown" => Some(3),
                    "p" | "pending" => {
                        fail.pending = true;
                        None
                    }
                    "n" | "none" => {
                        fail = Self::default();
                        None
                    }
                    "a" | "all" => {
                        fail = Self {
                            states: [true, true, true, false],
                            pending: true,
                        };
                        None
                    }
                    _ => return None,
                }
            };
            if let Some(state) = state {
                fail.states[state] = true;
            }
        }
        Some(fail)
    }

    /// `get_fail_on(state)`.
    #[must_use]
    pub fn on(&self, state: u32) -> bool {
        self.states.get(state as usize).copied().unwrap_or(false)
    }

    /// Fails on nothing.
    #[must_use]
    #[cfg(test)]
    pub fn is_none(&self) -> bool {
        !self.pending && !self.states.contains(&true)
    }
}

/// One expanded dependency: `dependent` depends on `master`
/// (`hostdependency_helper::expand`, `servicedependency_helper::expand`).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DependencyConf {
    pub dependent: Key,
    pub master: Key,
    /// `execution_failure_criteria`: the dependent is not checked while its
    /// master is in one of these states.
    pub execution: FailOn,
    /// `notification_failure_criteria`: kept for the notifications.
    pub notification: FailOn,
    pub inherits_parent: bool,
    /// `dependency_period`: the dependency applies in it only.
    pub period: String,
}

/// Notification flags of a notifier, a contact or an escalation
/// (`notifier::notification_flag`, `engine/inc/com/centreon/engine/notifier.hh:62-79`);
/// also the states of `flap_detection_options`.
pub mod notify_on {
    pub const UP: u32 = 1;
    pub const DOWN: u32 = 2;
    pub const UNREACHABLE: u32 = 4;
    pub const OK: u32 = 8;
    pub const WARNING: u32 = 16;
    pub const CRITICAL: u32 = 32;
    pub const UNKNOWN: u32 = 64;
    pub const FLAPPING_START: u32 = 128;
    pub const FLAPPING_STOP: u32 = 256;
    pub const FLAPPING_DISABLED: u32 = 512;
    pub const DOWNTIME: u32 = 1024;
    /// The three flapping flags (`f`).
    pub const FLAPPING: u32 = FLAPPING_START | FLAPPING_STOP | FLAPPING_DISABLED;
    /// Default `notification_options` of a host (`host_helper.cc:276-279`).
    pub const HOST_DEFAULT: u32 = UP | DOWN | UNREACHABLE | FLAPPING | DOWNTIME;
    /// Default `notification_options` of a service (`service_helper.cc:237-240`).
    pub const SERVICE_DEFAULT: u32 = OK | WARNING | CRITICAL | UNKNOWN | FLAPPING | DOWNTIME;

    /// Flag of state `state` of a host or a service.
    #[must_use]
    pub fn state(host: bool, state: u32) -> u32 {
        if host {
            [UP, DOWN, UNREACHABLE]
                .get(state as usize)
                .copied()
                .unwrap_or(UP)
        } else {
            [OK, WARNING, CRITICAL, UNKNOWN]
                .get(state as usize)
                .copied()
                .unwrap_or(UNKNOWN)
        }
    }

    /// `notification_options` (and the contact options), as
    /// `message_helper.cc:187-253` reads them: `None` on an unknown token.
    /// `r` is the recovery (UP or OK), `f` flapping, `s` downtime, `n`
    /// none (resets), `a` all.
    #[must_use]
    pub fn parse(value: &str, host: bool) -> Option<u32> {
        let mut flags = 0;
        for item in value.split(',').map(str::trim).filter(|i| !i.is_empty()) {
            flags |= match (host, item) {
                (true, "d" | "down") => DOWN,
                (true, "u" | "unreachable") => UNREACHABLE,
                (true, "r" | "recovery") => UP,
                (false, "u" | "unknown") => UNKNOWN,
                (false, "w" | "warning") => WARNING,
                (false, "c" | "critical") => CRITICAL,
                (false, "r" | "recovery") => OK,
                (_, "f" | "flapping") => FLAPPING,
                (_, "s" | "downtime") => DOWNTIME,
                (_, "n" | "none") => {
                    flags = 0;
                    0
                }
                (true, "a" | "all") => HOST_DEFAULT,
                (false, "a" | "all") => SERVICE_DEFAULT,
                _ => return None,
            };
        }
        Some(flags)
    }

    /// `escalation_options` (`hostescalation_helper`, `serviceescalation_helper`
    /// hooks; applier mapping `applier/hostescalation.cc:54-64`,
    /// `applier/serviceescalation.cc:60-80`).
    #[must_use]
    pub fn parse_escalation(value: &str, host: bool) -> Option<u32> {
        let mut flags = 0;
        for item in value.split(',').map(str::trim).filter(|i| !i.is_empty()) {
            flags |= match (host, item) {
                (true, "d" | "down") => DOWN,
                (true, "u" | "unreachable") => UNREACHABLE,
                (true, "r" | "recovery") => UP,
                (true, "a" | "all") => UP | DOWN | UNREACHABLE,
                (false, "w" | "warning") => WARNING,
                (false, "u" | "unknown") => UNKNOWN,
                (false, "c" | "critical") => CRITICAL,
                (false, "r" | "recovery") => OK,
                (false, "a" | "all") => OK | WARNING | CRITICAL | UNKNOWN,
                (_, "n" | "none") => {
                    flags = 0;
                    0
                }
                _ => return None,
            };
        }
        Some(flags)
    }

    /// `flap_detection_options` (`service_helper.cc:71-95`,
    /// `host_helper.cc:207-229`): the states recorded in the history. For
    /// a service `n` adds nothing (it does not reset), for a host it resets.
    #[must_use]
    pub fn parse_flap(value: &str, host: bool) -> Option<u32> {
        let mut flags = 0;
        for item in value.split(',').map(str::trim).filter(|i| !i.is_empty()) {
            flags |= match (host, item) {
                (true, "o" | "up") => UP,
                (true, "d" | "down") => DOWN,
                (true, "u" | "unreachable") => UNREACHABLE,
                (true, "a" | "all") => UP | DOWN | UNREACHABLE,
                (true, "n" | "none") => {
                    flags = 0;
                    0
                }
                (false, "o" | "ok") => OK,
                (false, "w" | "warning") => WARNING,
                (false, "u" | "unknown") => UNKNOWN,
                (false, "c" | "critical") => CRITICAL,
                (false, "a" | "all") => OK | WARNING | CRITICAL | UNKNOWN,
                (false, "n" | "none") => 0,
                _ => return None,
            };
        }
        Some(flags)
    }
}

/// `define contact` (`contact_helper.cc`, `engine/src/contact.cc`).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ContactConf {
    pub name: String,
    /// The name when unset.
    pub alias: String,
    pub email: String,
    pub pager: String,
    /// `address1` to `address6`.
    pub addresses: [String; 6],
    pub host_notifications_enabled: bool,
    pub service_notifications_enabled: bool,
    /// [`notify_on`] flags; none by default.
    pub host_notification_options: u32,
    pub service_notification_options: u32,
    /// Always when empty.
    pub host_notification_period: String,
    pub service_notification_period: String,
    /// Command names, looked up as they are (no `!` arguments).
    pub host_notification_commands: Vec<String>,
    pub service_notification_commands: Vec<String>,
    /// `timezone` of its notification periods.
    pub timezone: String,
    /// Contact groups it is a member of, sorted: the first is
    /// `$CONTACTGROUPNAME$`.
    pub groups: Vec<String>,
}

/// `define contactgroup`, members flattened (`applier/contactgroup.cc:201-232`).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ContactGroupConf {
    pub name: String,
    pub alias: String,
    /// Contact names, sorted.
    pub members: Vec<String>,
}

/// One escalation of a host or service (`engine/src/escalation.cc`,
/// `hostescalation.cc`, `serviceescalation.cc`), expanded per object.
#[derive(Debug, Clone, PartialEq)]
pub struct EscalationConf {
    pub key: Key,
    pub first_notification: u32,
    /// 0: no upper bound.
    pub last_notification: u32,
    /// In units of `interval_length`; at least 0.
    pub notification_interval: f64,
    /// Always when empty.
    pub escalation_period: String,
    /// [`notify_on`] state flags.
    pub escalate_on: u32,
    /// Contact groups only: escalations have no contacts.
    pub contact_groups: Vec<String>,
}

/// Comma-separated names, trimmed, empty ones dropped (`fill_string_group`).
#[must_use]
pub fn names(value: &str) -> Vec<String> {
    value
        .split(',')
        .map(str::trim)
        .filter(|name| !name.is_empty())
        .map(str::to_owned)
        .collect()
}

/// Comma-separated ids (`category_tags`, `group_tags`); `None` when one
/// is not a number.
#[must_use]
pub fn ids(value: &str) -> Option<Vec<u64>> {
    value
        .split(',')
        .map(str::trim)
        .filter(|id| !id.is_empty())
        .map(|id| id.parse().ok())
        .collect()
}

/// `host1,service1,host2,service2` (`fill_pair_string_group`).
#[must_use]
pub fn pairs(value: &str) -> Vec<(String, String)> {
    let items: Vec<&str> = value.split(',').map(str::trim).collect();
    items
        .chunks(2)
        .filter(|pair| pair.len() == 2 && !pair[0].is_empty() && !pair[1].is_empty())
        .map(|pair| (pair[0].to_owned(), pair[1].to_owned()))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn failure_criteria_read_as_engine() {
        let service = FailOn::parse("w,c", false).unwrap();
        assert!(service.on(1) && service.on(2) && !service.on(0) && !service.on(3));
        let all = FailOn::parse("a", false).unwrap();
        assert!(all.on(2) && !all.on(3) && all.pending);
        assert!(FailOn::parse("n", false).unwrap().is_none());
        assert!(FailOn::parse("x", false).is_none());
        let host = FailOn::parse("d,u", true).unwrap();
        assert!(host.on(1) && host.on(2) && !host.on(0));
    }

    #[test]
    fn lists_read_as_engine() {
        assert_eq!(names(" a, b ,,c"), ["a", "b", "c"]);
        assert_eq!(ids("3,4"), Some(vec![3, 4]));
        assert_eq!(ids("3,x"), None);
        assert_eq!(
            pairs("h1,s1, h2 ,s2,h3"),
            [
                ("h1".to_owned(), "s1".to_owned()),
                ("h2".to_owned(), "s2".to_owned())
            ]
        );
    }
}
