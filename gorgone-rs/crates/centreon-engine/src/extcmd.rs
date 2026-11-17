//! External commands the scheduler handles, with the argument order of
//! Engine (`engine/src/commands/processing.cc`).

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

/// Parses `[timestamp] NAME;arg;…`.
pub fn parse(line: &str) -> Result<ExtCommand, ParseError> {
    let line = line.trim_end_matches(['\r', '\n']);
    let body = match line.strip_prefix('[') {
        Some(stamped) => stamped
            .split_once(']')
            .map(|(_, rest)| rest.trim_start())
            .ok_or_else(|| ParseError::Invalid("unterminated timestamp".to_owned()))?,
        None => line,
    };
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
        "" => Err(ParseError::Invalid("no command name".to_owned())),
        other => Err(ParseError::Unsupported(other.to_owned())),
    }
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
    fn refuses_explicitly() {
        assert_eq!(
            parse("[1] ACKNOWLEDGE_SVC_PROBLEM;h;s;1;1;1;a;c"),
            Err(ParseError::Unsupported("ACKNOWLEDGE_SVC_PROBLEM".into()))
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
