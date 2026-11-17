//! The event log: the lines Engine writes at INFO through its spdlog
//! loggers, which `broker_sink_mt` (`engine/inc/com/centreon/engine/logging/broker_sink.hh`)
//! hands to `broker_log_data`, and the `pb_log_entry` `cbmod` makes of them
//! (`forward_pb_log` and `set_pb_log_data`, `engine/src/broker.cc:3252-3720`).
//! The line is built with Engine's format strings, then parsed back as
//! `set_pb_log_data` does, so that the fields of the event are those Engine
//! would send.

use gorgone_proto::bbdo::{LogEntry, log_entry};

use crate::bbdo::{event, frame};

/// `INITIAL` or `CURRENT` of `log_host_state` (`compatibility/logging.cc`);
/// Engine only logs initial states.
pub const INITIAL: &str = "INITIAL";

/// Name of a host state (`host::tab_host_states`).
#[must_use]
pub fn host_state(state: u32) -> &'static str {
    match state {
        0 => "UP",
        1 => "DOWN",
        2 => "UNREACHABLE",
        // `log_host_state` / `log_event` keep UP out of range.
        _ => "UP",
    }
}

/// Name of a service state (`service::tab_service_states`).
#[must_use]
pub fn service_state(state: u32) -> &'static str {
    match state {
        0 => "OK",
        1 => "WARNING",
        2 => "CRITICAL",
        _ => "UNKNOWN",
    }
}

/// `tab_state_type`.
#[must_use]
pub fn state_type(hard: bool) -> &'static str {
    if hard { "HARD" } else { "SOFT" }
}

/// `SERVICE ALERT: {};{};{};{};{};{}` (`service::log_event`,
/// `engine/src/service.cc:2147`).
#[must_use]
pub fn service_alert(
    host: &str,
    service: &str,
    state: u32,
    hard: bool,
    attempt: u32,
    output: &str,
) -> String {
    format!(
        "SERVICE ALERT: {host};{service};{};{};{attempt};{output}",
        service_state(state),
        state_type(hard)
    )
}

/// `HOST ALERT: {};{};{};{};{}` (`host::log_event`, `engine/src/host.cc:1234`).
#[must_use]
pub fn host_alert(host: &str, state: u32, hard: bool, attempt: u32, output: &str) -> String {
    format!(
        "HOST ALERT: {host};{};{};{attempt};{output}",
        host_state(state),
        state_type(hard)
    )
}

/// `{} HOST STATE: {};{};{};{};{}` (`log_host_state`,
/// `engine/src/compatibility/logging.cc:43-57`).
#[must_use]
pub fn host_initial_state(
    host: &str,
    state: u32,
    hard: bool,
    attempt: u32,
    output: &str,
) -> String {
    format!(
        "{INITIAL} HOST STATE: {host};{};{};{attempt};{output}",
        host_state(state),
        state_type(hard)
    )
}

/// `{} SERVICE STATE: {};{};{};{};{};{}` (`log_service_state`,
/// `engine/src/compatibility/logging.cc:65-81`).
#[must_use]
pub fn service_initial_state(
    host: &str,
    service: &str,
    state: u32,
    hard: bool,
    attempt: u32,
    output: &str,
) -> String {
    format!(
        "{INITIAL} SERVICE STATE: {host};{service};{};{};{attempt};{output}",
        service_state(state),
        state_type(hard)
    )
}

/// `EXTERNAL COMMAND: {};{}` (`processing::execute`,
/// `engine/src/commands/processing.cc:878-891`).
#[must_use]
pub fn external_command(name: &str, args: &str) -> String {
    format!("EXTERNAL COMMAND: {name};{args}")
}

/// `PASSIVE SERVICE CHECK: {};{};{};{}` (`engine/src/service.cc:1402`).
#[must_use]
pub fn passive_service_check(host: &str, service: &str, state: u32, output: &str) -> String {
    format!("PASSIVE SERVICE CHECK: {host};{service};{state};{output}")
}

/// What a downtime alert tells.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DowntimeAlert {
    Started,
    Stopped,
    Cancelled,
}

/// `HOST DOWNTIME ALERT: …` (`engine/src/downtimes/host_downtime.cc:187`,
/// `:379`, `:454`).
#[must_use]
pub fn host_downtime_alert(host: &str, alert: DowntimeAlert) -> String {
    let text = match alert {
        DowntimeAlert::Started => "STARTED; Host has entered a period of scheduled downtime",
        DowntimeAlert::Stopped => "STOPPED; Host has exited from a period of scheduled downtime",
        DowntimeAlert::Cancelled => "CANCELLED; Scheduled downtime for host has been cancelled.",
    };
    format!("HOST DOWNTIME ALERT: {host};{text}")
}

/// `SERVICE DOWNTIME ALERT: …` (`engine/src/downtimes/service_downtime.cc:216`,
/// `:420`, `:502`).
#[must_use]
pub fn service_downtime_alert(host: &str, service: &str, alert: DowntimeAlert) -> String {
    let text = match alert {
        DowntimeAlert::Started => "STARTED; Service has entered a period of scheduled downtime",
        DowntimeAlert::Stopped => "STOPPED; Service has exited from a period of scheduled downtime",
        DowntimeAlert::Cancelled => "CANCELLED; Scheduled downtime for service has been cancelled.",
    };
    format!("SERVICE DOWNTIME ALERT: {host};{service};{text}")
}

/// `Acknowledgement of host '{}' just expired` (`host.cc:2347`).
#[must_use]
pub fn host_acknowledgement_expired(host: &str) -> String {
    format!("Acknowledgement of host '{host}' just expired")
}

/// `Acknowledgement of service '{}' on host '{}' just expired`
/// (`service.cc:835`).
#[must_use]
pub fn service_acknowledgement_expired(host: &str, service: &str) -> String {
    format!("Acknowledgement of service '{service}' on host '{host}' just expired")
}

/// `status_id` (`broker.cc:2823`).
fn status_id(status: &str) -> i32 {
    match status {
        "DOWN" | "WARNING" => 1,
        "UNREACHABLE" | "CRITICAL" => 2,
        "UNKNOWN" => 3,
        "PENDING" => 4,
        _ => 0,
    }
}

/// `notification_status_id` (`broker.cc:2854`): the state between
/// parentheses when there are some.
fn notification_status_id(status: &str) -> i32 {
    match status.find('(') {
        Some(start) => {
            let rest = &status[start..];
            let end = rest.find(')').unwrap_or(rest.len());
            status_id(&rest[..end])
        }
        None => status_id(status),
    }
}

fn log_type(value: &str) -> log_entry::LogType {
    if value == "HARD" {
        log_entry::LogType::Hard
    } else {
        log_entry::LogType::Soft
    }
}

/// A `pb_log_entry` as `forward_pb_log` makes it of `line`; `ids` gives
/// the id of a host name, or of a (host name, service description), 0 when
/// unknown (`engine::get_host_id`, `get_service_id`).
#[must_use]
pub fn entry(
    instance_name: &str,
    ctime: i64,
    line: &str,
    ids: &dyn Fn(&str, Option<&str>) -> u64,
) -> Vec<u8> {
    let mut le = LogEntry {
        ctime: u64::try_from(ctime).unwrap_or(0),
        instance_name: instance_name.to_owned(),
        output: line.to_owned(),
        ..LogEntry::default()
    };
    fill(&mut le, line);
    // `fill_obj_on_exit`: ids from the names, whatever the parse gave.
    if !le.host_name.is_empty() {
        le.host_id = ids(&le.host_name, None);
        if !le.service_description.is_empty() {
            le.service_id = ids(&le.host_name, Some(&le.service_description));
        }
    }
    frame(event::LOG_ENTRY, &le)
}

/// `set_pb_log_data` (`broker.cc:3252-3640`): the fields of the line. A
/// missing field stops the parse with what was read so far.
fn fill(le: &mut LogEntry, output: &str) {
    use log_entry::MsgType;
    let (typ, rest) = output.split_once(':').unwrap_or((output, ""));
    let rest = rest.trim_start();
    let mut args = rest.split(';');
    let mut next = || args.next();
    macro_rules! field {
        () => {
            match next() {
                Some(value) => value,
                None => return,
            }
        };
    }
    macro_rules! output {
        () => {
            match next() {
                Some(value) if !value.is_empty() => le.output = value.to_owned(),
                _ => return,
            }
        };
    }
    // host;[service;]status;type;retry;output
    macro_rules! state_line {
        ($kind:expr, $service:expr, $status:ident) => {{
            le.msg_type = $kind.into();
            le.host_name = field!().to_owned();
            if $service {
                le.service_description = field!().to_owned();
            }
            le.status = $status(field!());
            le.r#type = log_type(field!()).into();
            match field!().parse() {
                Ok(retry) => le.retry = retry,
                Err(_) => return,
            }
            output!();
        }};
    }
    match typ {
        "SERVICE ALERT" => state_line!(MsgType::ServiceAlert, true, status_id),
        "HOST ALERT" => state_line!(MsgType::HostAlert, false, status_id),
        "INITIAL HOST STATE" => {
            state_line!(MsgType::HostInitialState, false, notification_status_id)
        }
        "INITIAL SERVICE STATE" => state_line!(MsgType::ServiceInitialState, true, status_id),
        "SERVICE EVENT HANDLER" => state_line!(MsgType::ServiceEventHandler, true, status_id),
        "HOST EVENT HANDLER" => state_line!(MsgType::HostEventHandler, false, status_id),
        "GLOBAL SERVICE EVENT HANDLER" => {
            state_line!(MsgType::GlobalServiceEventHandler, true, status_id);
        }
        "GLOBAL HOST EVENT HANDLER" => {
            state_line!(MsgType::GlobalHostEventHandler, false, status_id);
        }
        "SERVICE NOTIFICATION" | "HOST NOTIFICATION" => {
            let service = typ == "SERVICE NOTIFICATION";
            le.msg_type = if service {
                MsgType::ServiceNotification
            } else {
                MsgType::HostNotification
            }
            .into();
            le.notification_contact = field!().to_owned();
            le.host_name = field!().to_owned();
            if service {
                le.service_description = field!().to_owned();
            }
            le.status = notification_status_id(field!());
            le.notification_cmd = field!().to_owned();
            output!();
        }
        "EXTERNAL COMMAND" => {
            let command = field!();
            let service = match command {
                "ACKNOWLEDGE_SVC_PROBLEM" => true,
                "ACKNOWLEDGE_HOST_PROBLEM" => false,
                _ => {
                    le.msg_type = MsgType::Other.into();
                    le.output = output.to_owned();
                    return;
                }
            };
            le.msg_type = if service {
                MsgType::ServiceAcknowledgeProblem
            } else {
                MsgType::HostAcknowledgeProblem
            }
            .into();
            le.host_name = field!().to_owned();
            if service {
                le.service_description = field!().to_owned();
            }
            // sticky, notify, persistent
            for _ in 0..3 {
                field!();
            }
            le.notification_contact = field!().to_owned();
            output!();
        }
        "Warning" => {
            le.msg_type = MsgType::Warning.into();
            le.output = rest.to_owned();
        }
        _ => {
            le.msg_type = MsgType::Other.into();
            le.output = output.to_owned();
        }
    }
}

#[cfg(test)]
mod tests {
    use prost::Message;

    use super::*;

    fn decode(frame: &[u8]) -> LogEntry {
        LogEntry::decode(&frame[crate::bbdo::HEADER_SIZE..]).unwrap()
    }

    fn ids(host: &str, service: Option<&str>) -> u64 {
        match (host, service) {
            ("h1", None) => 10,
            ("h1", Some("ping")) => 3,
            _ => 0,
        }
    }

    #[test]
    fn alerts_read_back_as_cbmod_fills_them() {
        let line = service_alert("h1", "ping", 2, false, 1, "CRITICAL - down");
        assert_eq!(
            line,
            "SERVICE ALERT: h1;ping;CRITICAL;SOFT;1;CRITICAL - down"
        );
        let le = decode(&entry("poller", 100, &line, &ids));
        assert_eq!(le.msg_type, log_entry::MsgType::ServiceAlert as i32);
        assert_eq!((le.host_id, le.service_id), (10, 3));
        assert_eq!((le.status, le.retry), (2, 1));
        assert_eq!(le.r#type, log_entry::LogType::Soft as i32);
        assert_eq!(le.output, "CRITICAL - down");
        assert_eq!((le.ctime, le.instance_name.as_str()), (100, "poller"));

        let line = host_initial_state("h1", 2, true, 3, "unreachable");
        assert_eq!(
            line,
            "INITIAL HOST STATE: h1;UNREACHABLE;HARD;3;unreachable"
        );
        let le = decode(&entry("poller", 1, &line, &ids));
        assert_eq!(le.msg_type, log_entry::MsgType::HostInitialState as i32);
        assert_eq!((le.host_id, le.status), (10, 2));

        // An empty output keeps the whole line, as `test_fail_and_not_empty`.
        let line = host_alert("h1", 1, true, 1, "");
        let le = decode(&entry("poller", 1, &line, &ids));
        assert_eq!(le.output, line);
        assert_eq!(le.status, 1);
    }

    #[test]
    fn external_commands_and_others() {
        let line = external_command("ACKNOWLEDGE_SVC_PROBLEM", "h1;ping;2;0;1;admin;on it");
        let le = decode(&entry("p", 1, &line, &ids));
        assert_eq!(
            le.msg_type,
            log_entry::MsgType::ServiceAcknowledgeProblem as i32
        );
        assert_eq!(
            (le.notification_contact.as_str(), le.output.as_str()),
            ("admin", "on it")
        );
        let line = external_command("SCHEDULE_FORCED_SVC_CHECK", "h1;ping;1");
        let le = decode(&entry("p", 1, &line, &ids));
        assert_eq!(le.msg_type, log_entry::MsgType::Other as i32);
        assert_eq!(le.output, line);
        assert_eq!(le.host_id, 0);
        let line = host_downtime_alert("h1", DowntimeAlert::Started);
        assert_eq!(
            line,
            "HOST DOWNTIME ALERT: h1;STARTED; Host has entered a period of scheduled downtime"
        );
        let le = decode(&entry("p", 1, &line, &ids));
        assert_eq!(le.msg_type, log_entry::MsgType::Other as i32);
    }
}
