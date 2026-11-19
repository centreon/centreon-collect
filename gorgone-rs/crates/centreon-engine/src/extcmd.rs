//! External commands the scheduler handles, with the argument order and
//! parsing of Engine (`engine/src/commands/processing.cc`,
//! `engine/src/commands/commands.cc`).

/// What a downtime command puts in downtime.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DowntimeTarget {
    /// `SCHEDULE_HOST_DOWNTIME`.
    Host(String),
    /// `SCHEDULE_HOST_SVC_DOWNTIME`: every service of the host.
    HostServices(String),
    /// `SCHEDULE_SVC_DOWNTIME`.
    Service(String, String),
    /// `SCHEDULE_HOSTGROUP_HOST_DOWNTIME` (each member host), with
    /// `services` `SCHEDULE_HOSTGROUP_SVC_DOWNTIME` (each service of each
    /// member host).
    HostGroup { name: String, services: bool },
    /// `SCHEDULE_SERVICEGROUP_HOST_DOWNTIME` (each host of the members),
    /// with `services` `SCHEDULE_SERVICEGROUP_SVC_DOWNTIME` (each member).
    ServiceGroup { name: String, services: bool },
}

/// What an `ENABLE_*` / `DISABLE_*` command switches.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Switch {
    Notifications,
    EventHandler,
    FlapDetection,
}

/// Where it switches it.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SwitchTarget {
    /// The whole engine (`ENABLE_NOTIFICATIONS`...).
    Global,
    Host(String),
    Service(String, String),
    /// Every service of a host (`ENABLE_HOST_SVC_NOTIFICATIONS`).
    HostServices(String),
}

/// A supported external command.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ExtCommand {
    /// `SCHEDULE_[FORCED_]SVC_CHECK;host;service;time`.
    ScheduleService {
        /// Host name.
        host: String,
        /// Service description.
        service: String,
        /// When, in seconds since the epoch.
        time: i64,
        /// Run even when active checks are disabled or a check is planned
        /// earlier.
        forced: bool,
    },
    /// `SCHEDULE_[FORCED_]HOST_CHECK;host;time`.
    ScheduleHost {
        /// Host name.
        host: String,
        /// When, in seconds since the epoch.
        time: i64,
        /// See `ScheduleService`.
        forced: bool,
    },
    /// `PROCESS_SERVICE_CHECK_RESULT;host;service;code;output`.
    ServiceResult {
        /// Host name.
        host: String,
        /// Service description.
        service: String,
        /// 0 to 3.
        code: u32,
        /// Plugin output, perfdata included.
        output: String,
    },
    /// `PROCESS_HOST_CHECK_RESULT;host;code;output`.
    HostResult {
        /// Host name.
        host: String,
        /// 0 UP, 1 DOWN, 2 UNREACHABLE.
        code: u32,
        /// Plugin output.
        output: String,
    },
    /// `SCHEDULE_{HOST,HOST_SVC,SVC}_DOWNTIME;target;start;end;fixed;
    /// trigger_id;duration;author;comment` (`cmd_schedule_downtime`).
    ScheduleDowntime {
        target: DowntimeTarget,
        start: i64,
        end: i64,
        fixed: bool,
        triggered_by: u64,
        duration: u64,
        author: String,
        comment: String,
    },
    /// `DEL_HOST_DOWNTIME;id`, `DEL_SVC_DOWNTIME;id`.
    DeleteDowntime { id: u64 },
    /// `ACKNOWLEDGE_HOST_PROBLEM;host;sticky;notify;persistent;author;comment`,
    /// `ACKNOWLEDGE_SVC_PROBLEM;host;service;...` (`cmd_acknowledge_problem`).
    Acknowledge {
        host: String,
        service: Option<String>,
        sticky: bool,
        notify: bool,
        persistent: bool,
        author: String,
        comment: String,
    },
    /// `REMOVE_HOST_ACKNOWLEDGEMENT;host`, `REMOVE_SVC_ACKNOWLEDGEMENT;host;service`.
    RemoveAcknowledgement {
        host: String,
        service: Option<String>,
    },
    /// `ADD_HOST_COMMENT;host;persistent;author;comment`,
    /// `ADD_SVC_COMMENT;host;service;persistent;author;comment`.
    AddComment {
        host: String,
        service: Option<String>,
        persistent: bool,
        author: String,
        comment: String,
    },
    /// `DEL_HOST_COMMENT;id`, `DEL_SVC_COMMENT;id`.
    DeleteComment { id: u64 },
    /// `DEL_ALL_HOST_COMMENTS;host`, `DEL_ALL_SVC_COMMENTS;host;service`.
    DeleteAllComments {
        host: String,
        service: Option<String>,
    },
    /// `ENABLE_*` / `DISABLE_*` of notifications, event handlers and flap
    /// detection (`commands.cc:2234-2350`, `:2855-2954`, `flapping.cc`,
    /// `service.cc:3027-3093`, `host.cc:2743-2803`). Arguments after the
    /// object (the legacy pages add a time) are ignored, as Engine does.
    Switch {
        what: Switch,
        target: SwitchTarget,
        enable: bool,
    },
}

/// Why a line is refused.
#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
pub enum ParseError {
    /// Well formed, but not one the scheduler handles.
    #[error("{0} is not supported by the built-in scheduler")]
    Unsupported(String),
    /// Malformed.
    #[error("{0}")]
    Invalid(String),
}

fn time(value: &str, name: &str) -> Result<i64, ParseError> {
    value
        .trim()
        .parse()
        .map_err(|_| ParseError::Invalid(format!("{name}: invalid time {value:?}")))
}

fn code(value: &str, name: &str, max: u32) -> Result<u32, ParseError> {
    value
        .trim()
        .parse()
        .ok()
        .filter(|code| *code <= max)
        .ok_or_else(|| ParseError::Invalid(format!("{name}: invalid return code {value:?}")))
}

/// `absl::SimpleAtob`.
fn flag(value: &str, what: &str) -> Result<bool, ParseError> {
    match value.trim().to_ascii_lowercase().as_str() {
        "1" | "true" | "t" | "yes" | "y" => Ok(true),
        "0" | "false" | "f" | "no" | "n" => Ok(false),
        _ => Err(ParseError::Invalid(format!(
            "{what} {value:?} must be 1 or 0"
        ))),
    }
}

fn number<T: std::str::FromStr>(value: &str, what: &str) -> Result<T, ParseError> {
    value
        .trim()
        .parse()
        .map_err(|_| ParseError::Invalid(format!("{what} {value:?} must be an integer")))
}

/// Name and arguments of a line, as `processing::execute` logs them
/// (`EXTERNAL COMMAND: name;args`).
#[must_use]
pub fn name_and_args(line: &str) -> (&str, &str) {
    let line = line.trim_end_matches(['\r', '\n']);
    let body = match line.strip_prefix('[') {
        Some(stamped) => stamped
            .split_once(']')
            .map_or(line, |(_, rest)| rest.trim_start()),
        None => line,
    };
    body.split_once(';').unwrap_or((body, ""))
}

/// Parses `[timestamp] NAME;arg;…`.
#[cfg(test)]
pub fn parse(line: &str) -> Result<ExtCommand, ParseError> {
    parse_stamped(line).map(|(_, command)| command)
}

/// Parses `[timestamp] NAME;arg;…`, with its timestamp when it has one
/// (the entry time of comments and downtimes).
pub fn parse_stamped(line: &str) -> Result<(Option<i64>, ExtCommand), ParseError> {
    let line = line.trim_end_matches(['\r', '\n']);
    let (stamp, body) = match line.strip_prefix('[') {
        Some(stamped) => stamped
            .split_once(']')
            .map(|(stamp, rest)| (stamp.trim().parse().ok(), rest.trim_start()))
            .ok_or_else(|| ParseError::Invalid("unterminated timestamp".to_owned()))?,
        None => (None, line),
    };
    parse_body(body).map(|command| (stamp, command))
}

fn parse_body(body: &str) -> Result<ExtCommand, ParseError> {
    let (name, args) = body.split_once(';').unwrap_or((body, ""));
    let fields = |count: usize| -> Result<Vec<&str>, ParseError> {
        let parts: Vec<&str> = args.splitn(count, ';').collect();
        if parts.len() == count {
            Ok(parts)
        } else {
            Err(ParseError::Invalid(format!(
                "{name}: expected {count} arguments, got {}",
                parts.len()
            )))
        }
    };
    match name {
        "SCHEDULE_SVC_CHECK" | "SCHEDULE_FORCED_SVC_CHECK" => {
            let parts = fields(3)?;
            Ok(ExtCommand::ScheduleService {
                host: parts[0].to_owned(),
                service: parts[1].to_owned(),
                time: time(parts[2], name)?,
                forced: name.contains("FORCED"),
            })
        }
        "SCHEDULE_HOST_CHECK" | "SCHEDULE_FORCED_HOST_CHECK" => {
            let parts = fields(2)?;
            Ok(ExtCommand::ScheduleHost {
                host: parts[0].to_owned(),
                time: time(parts[1], name)?,
                forced: name.contains("FORCED"),
            })
        }
        "PROCESS_SERVICE_CHECK_RESULT" => {
            let parts = fields(4)?;
            Ok(ExtCommand::ServiceResult {
                host: parts[0].to_owned(),
                service: parts[1].to_owned(),
                code: code(parts[2], name, 3)?,
                output: parts[3].to_owned(),
            })
        }
        "PROCESS_HOST_CHECK_RESULT" => {
            let parts = fields(3)?;
            Ok(ExtCommand::HostResult {
                host: parts[0].to_owned(),
                code: code(parts[1], name, 2)?,
                output: parts[2].to_owned(),
            })
        }
        "SCHEDULE_HOST_DOWNTIME"
        | "SCHEDULE_HOST_SVC_DOWNTIME"
        | "SCHEDULE_SVC_DOWNTIME"
        | "SCHEDULE_HOSTGROUP_HOST_DOWNTIME"
        | "SCHEDULE_HOSTGROUP_SVC_DOWNTIME"
        | "SCHEDULE_SERVICEGROUP_HOST_DOWNTIME"
        | "SCHEDULE_SERVICEGROUP_SVC_DOWNTIME" => downtime(name, args),
        "DEL_HOST_DOWNTIME" | "DEL_SVC_DOWNTIME" => {
            let id = args.split('\n').next().unwrap_or_default();
            Ok(ExtCommand::DeleteDowntime {
                id: number(id, "downtime_id")?,
            })
        }
        "ACKNOWLEDGE_HOST_PROBLEM" | "ACKNOWLEDGE_SVC_PROBLEM" => {
            let service = name == "ACKNOWLEDGE_SVC_PROBLEM";
            let parts = fields(if service { 7 } else { 6 })?;
            let at = usize::from(service);
            let kind: i64 = number(parts[at + 1], "type")?;
            let notify: i64 = number(parts[at + 2], "notify")?;
            let persistent: i64 = number(parts[at + 3], "persistent")?;
            Ok(ExtCommand::Acknowledge {
                host: parts[0].to_owned(),
                service: service.then(|| parts[1].to_owned()),
                // AckType::STICKY is 2; 0 and 1 are a normal acknowledgement.
                sticky: kind == 2,
                notify: notify > 0,
                persistent: persistent > 0,
                author: parts[at + 4].to_owned(),
                comment: parts[at + 5]
                    .split('\n')
                    .next()
                    .unwrap_or_default()
                    .to_owned(),
            })
        }
        "REMOVE_HOST_ACKNOWLEDGEMENT" | "REMOVE_SVC_ACKNOWLEDGEMENT" => {
            let mut parts = args.split(';');
            let host = parts.next().unwrap_or_default().to_owned();
            let service = if name == "REMOVE_SVC_ACKNOWLEDGEMENT" {
                Some(
                    parts
                        .next()
                        .ok_or_else(|| ParseError::Invalid(format!("{name}: no service")))?
                        .to_owned(),
                )
            } else {
                None
            };
            Ok(ExtCommand::RemoveAcknowledgement { host, service })
        }
        "ADD_HOST_COMMENT" | "ADD_SVC_COMMENT" => {
            let service = name == "ADD_SVC_COMMENT";
            let parts = fields(if service { 5 } else { 4 })?;
            let at = usize::from(service);
            Ok(ExtCommand::AddComment {
                host: parts[0].to_owned(),
                service: service.then(|| parts[1].to_owned()),
                persistent: flag(parts[at + 1], "persistent")?,
                author: parts[at + 2].to_owned(),
                comment: parts[at + 3]
                    .split('\n')
                    .next()
                    .unwrap_or_default()
                    .to_owned(),
            })
        }
        "DEL_HOST_COMMENT" | "DEL_SVC_COMMENT" => Ok(ExtCommand::DeleteComment {
            id: number(args, "comment_id")?,
        }),
        "DEL_ALL_HOST_COMMENTS" | "DEL_ALL_SVC_COMMENTS" => {
            let mut parts = args.split(';');
            let host = parts.next().unwrap_or_default().to_owned();
            let service = if name == "DEL_ALL_SVC_COMMENTS" {
                Some(
                    parts
                        .next()
                        .ok_or_else(|| ParseError::Invalid(format!("{name}: no service")))?
                        .to_owned(),
                )
            } else {
                None
            };
            Ok(ExtCommand::DeleteAllComments { host, service })
        }
        _ if switch(name).is_some() => {
            let (what, kind, enable) = switch(name).unwrap_or((Switch::Notifications, 0, true));
            let mut parts = args.split(';');
            let mut next = |what: &str| {
                parts
                    .next()
                    .filter(|part| !part.is_empty())
                    .map(str::to_owned)
                    .ok_or_else(|| ParseError::Invalid(format!("{name}: no {what}")))
            };
            let target = match kind {
                0 => SwitchTarget::Global,
                1 => SwitchTarget::Host(next("host")?),
                2 => SwitchTarget::Service(next("host")?, next("service")?),
                _ => SwitchTarget::HostServices(next("host")?),
            };
            Ok(ExtCommand::Switch {
                what,
                target,
                enable,
            })
        }
        "" => Err(ParseError::Invalid("no command name".to_owned())),
        other => Err(ParseError::Unsupported(other.to_owned())),
    }
}

/// The switch commands: what, target kind (0 global, 1 host, 2 service,
/// 3 services of a host), enable.
fn switch(name: &str) -> Option<(Switch, u8, bool)> {
    let (enable, rest) = if let Some(rest) = name.strip_prefix("ENABLE_") {
        (true, rest)
    } else {
        (false, name.strip_prefix("DISABLE_")?)
    };
    let (what, kind) = match rest {
        "NOTIFICATIONS" => (Switch::Notifications, 0),
        "HOST_NOTIFICATIONS" => (Switch::Notifications, 1),
        "SVC_NOTIFICATIONS" => (Switch::Notifications, 2),
        "HOST_SVC_NOTIFICATIONS" => (Switch::Notifications, 3),
        "EVENT_HANDLERS" => (Switch::EventHandler, 0),
        "HOST_EVENT_HANDLER" => (Switch::EventHandler, 1),
        "SVC_EVENT_HANDLER" => (Switch::EventHandler, 2),
        "FLAP_DETECTION" => (Switch::FlapDetection, 0),
        "HOST_FLAP_DETECTION" => (Switch::FlapDetection, 1),
        "SVC_FLAP_DETECTION" => (Switch::FlapDetection, 2),
        _ => return None,
    };
    Some((what, kind, enable))
}

/// `cmd_schedule_downtime`: every argument is one `;` piece, the comment
/// included.
fn downtime(name: &str, args: &str) -> Result<ExtCommand, ParseError> {
    let mut parts = args.split(';');
    let mut next = |what: &str| {
        parts
            .next()
            .ok_or_else(|| ParseError::Invalid(format!("{name}: no {what}")))
    };
    let first = next("host")?.to_owned();
    let target = match name {
        "SCHEDULE_HOST_DOWNTIME" => DowntimeTarget::Host(first),
        "SCHEDULE_HOST_SVC_DOWNTIME" => DowntimeTarget::HostServices(first),
        "SCHEDULE_HOSTGROUP_HOST_DOWNTIME" | "SCHEDULE_HOSTGROUP_SVC_DOWNTIME" => {
            DowntimeTarget::HostGroup {
                name: first,
                services: name.ends_with("SVC_DOWNTIME"),
            }
        }
        "SCHEDULE_SERVICEGROUP_HOST_DOWNTIME" | "SCHEDULE_SERVICEGROUP_SVC_DOWNTIME" => {
            DowntimeTarget::ServiceGroup {
                name: first,
                services: name.ends_with("SVC_DOWNTIME"),
            }
        }
        _ => DowntimeTarget::Service(first, next("service")?.to_owned()),
    };
    let start = number(next("start_time")?, "start_time")?;
    let end = number(next("end_time")?, "end_time")?;
    let fixed = flag(next("fixed")?, "fixed")?;
    let triggered_by = number(next("triggered_by")?, "triggered_by")?;
    let duration = match next("duration")? {
        "" => 0,
        value => number(value, "duration")?,
    };
    let author = next("author")?.to_owned();
    let comment = next("comment")?.to_owned();
    if !fixed && duration == 0 {
        return Err(ParseError::Invalid(format!(
            "{name}: a flexible downtime needs a duration"
        )));
    }
    Ok(ExtCommand::ScheduleDowntime {
        target,
        start,
        end,
        fixed,
        triggered_by,
        duration,
        author,
        comment,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_the_supported_commands() {
        assert_eq!(
            parse("[1700000000] SCHEDULE_FORCED_SVC_CHECK;h1;Disk /;1700000005"),
            Ok(ExtCommand::ScheduleService {
                host: "h1".into(),
                service: "Disk /".into(),
                time: 1_700_000_005,
                forced: true,
            })
        );
        assert_eq!(
            parse("SCHEDULE_HOST_CHECK;h1;5"),
            Ok(ExtCommand::ScheduleHost {
                host: "h1".into(),
                time: 5,
                forced: false,
            })
        );
        // The output keeps its semicolons.
        assert_eq!(
            parse("PROCESS_SERVICE_CHECK_RESULT;h;s;2;CRIT; a;b | x=1"),
            Ok(ExtCommand::ServiceResult {
                host: "h".into(),
                service: "s".into(),
                code: 2,
                output: "CRIT; a;b | x=1".into(),
            })
        );
        assert_eq!(
            parse("PROCESS_HOST_CHECK_RESULT;h;1;down"),
            Ok(ExtCommand::HostResult {
                host: "h".into(),
                code: 1,
                output: "down".into(),
            })
        );
    }

    #[test]
    fn parses_downtime_ack_and_comment_commands() {
        assert_eq!(
            parse_stamped("[17] SCHEDULE_SVC_DOWNTIME;h;s;100;200;1;0;;me;a comment").unwrap(),
            (
                Some(17),
                ExtCommand::ScheduleDowntime {
                    target: DowntimeTarget::Service("h".into(), "s".into()),
                    start: 100,
                    end: 200,
                    fixed: true,
                    triggered_by: 0,
                    duration: 0,
                    author: "me".into(),
                    comment: "a comment".into(),
                }
            )
        );
        // The comment is one piece, as Engine reads it.
        let ExtCommand::ScheduleDowntime {
            comment, target, ..
        } = parse("SCHEDULE_HOST_SVC_DOWNTIME;h;1;2;0;3;60;me;a;b").unwrap()
        else {
            panic!()
        };
        assert_eq!(
            (comment.as_str(), target),
            ("a", DowntimeTarget::HostServices("h".into()))
        );
        assert!(parse("SCHEDULE_HOST_DOWNTIME;h;1;2;0;0;0;me;c").is_err());
        assert!(parse("SCHEDULE_HOST_DOWNTIME;h;1;2;1;0;0;me").is_err());
        assert_eq!(
            parse("DEL_SVC_DOWNTIME;12"),
            Ok(ExtCommand::DeleteDowntime { id: 12 })
        );
        assert_eq!(
            parse("ACKNOWLEDGE_SVC_PROBLEM;h;s;2;0;1;admin;text; with ;semicolons"),
            Ok(ExtCommand::Acknowledge {
                host: "h".into(),
                service: Some("s".into()),
                sticky: true,
                notify: false,
                persistent: true,
                author: "admin".into(),
                comment: "text; with ;semicolons".into(),
            })
        );
        let ExtCommand::Acknowledge {
            sticky, service, ..
        } = parse("ACKNOWLEDGE_HOST_PROBLEM;h;1;1;0;a;c").unwrap()
        else {
            panic!()
        };
        assert!(!sticky && service.is_none());
        assert_eq!(
            parse("REMOVE_SVC_ACKNOWLEDGEMENT;h;s"),
            Ok(ExtCommand::RemoveAcknowledgement {
                host: "h".into(),
                service: Some("s".into())
            })
        );
        assert_eq!(
            parse("ADD_HOST_COMMENT;h;1;me;hello; world"),
            Ok(ExtCommand::AddComment {
                host: "h".into(),
                service: None,
                persistent: true,
                author: "me".into(),
                comment: "hello; world".into(),
            })
        );
        assert!(parse("ADD_SVC_COMMENT;h;s;maybe;me;c").is_err());
        assert_eq!(
            parse("DEL_SVC_COMMENT;3"),
            Ok(ExtCommand::DeleteComment { id: 3 })
        );
        assert_eq!(
            parse("DEL_ALL_SVC_COMMENTS;h;s"),
            Ok(ExtCommand::DeleteAllComments {
                host: "h".into(),
                service: Some("s".into())
            })
        );
    }

    #[test]
    fn parses_the_switches() {
        assert_eq!(
            parse("ENABLE_NOTIFICATIONS").unwrap(),
            ExtCommand::Switch {
                what: Switch::Notifications,
                target: SwitchTarget::Global,
                enable: true
            }
        );
        // The legacy pages add a time after the object.
        assert_eq!(
            parse("[1] DISABLE_SVC_NOTIFICATIONS;h;s;1700000000").unwrap(),
            ExtCommand::Switch {
                what: Switch::Notifications,
                target: SwitchTarget::Service("h".into(), "s".into()),
                enable: false
            }
        );
        assert_eq!(
            parse("DISABLE_HOST_FLAP_DETECTION;h").unwrap(),
            ExtCommand::Switch {
                what: Switch::FlapDetection,
                target: SwitchTarget::Host("h".into()),
                enable: false
            }
        );
        assert_eq!(
            parse("ENABLE_HOST_SVC_NOTIFICATIONS;h").unwrap(),
            ExtCommand::Switch {
                what: Switch::Notifications,
                target: SwitchTarget::HostServices("h".into()),
                enable: true
            }
        );
        assert!(parse("ENABLE_SVC_EVENT_HANDLER;h").is_err());
        assert!(matches!(
            parse("SEND_CUSTOM_SVC_NOTIFICATION;h;s;0;a;c"),
            Err(ParseError::Unsupported(_))
        ));
    }

    #[test]
    fn refuses_explicitly() {
        assert_eq!(
            parse("[1] DEL_SVC_DOWNTIME_FULL;h;s;;;;;;"),
            Err(ParseError::Unsupported("DEL_SVC_DOWNTIME_FULL".into()))
        );
        assert!(matches!(
            parse("SCHEDULE_SVC_CHECK;h;s"),
            Err(ParseError::Invalid(_))
        ));
        assert!(matches!(
            parse("PROCESS_SERVICE_CHECK_RESULT;h;s;9;x"),
            Err(ParseError::Invalid(_))
        ));
        assert!(matches!(
            parse("SCHEDULE_HOST_CHECK;h;soon"),
            Err(ParseError::Invalid(_))
        ));
    }
}
