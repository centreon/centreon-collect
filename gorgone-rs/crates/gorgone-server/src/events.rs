//! Mapping of agent events to the legacy log codes and JSON payloads.

use gorgone_core::legacy;
use gorgone_proto::agent::job_event::Body;
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::{CommandResult, JobFinished};
use serde_json::{Value, json};

use crate::store::state;

/// What the store records for one agent event.
#[derive(Debug)]
pub(crate) struct Mapped {
    pub(crate) code: i32,
    pub(crate) data: Value,
    /// Final job state, for a terminal event.
    pub(crate) final_state: Option<&'static str>,
}

/// Payload of the "accepted" event the server writes at seq 0.
pub(crate) fn accepted() -> Value {
    json!({ "message": "accepted" })
}

/// Payload of the event the server writes when a job expires.
pub(crate) fn expired() -> Value {
    json!({ "message": "job expired: deadline passed", "outcome": "expired" })
}

/// Payload of the event the server writes when a queued job is cancelled.
pub(crate) fn cancelled() -> Value {
    json!({ "message": "cancelled", "outcome": "cancelled" })
}

/// Maps the body of an agent event recorded at `time_ms`.
pub(crate) fn map(body: &Body, time_ms: i64) -> Mapped {
    match body {
        Body::Started(_) => intermediate("job started"),
        Body::Progress(progress) => intermediate(&progress.message),
        Body::CommandResult(result) => Mapped {
            code: legacy::COMMAND_RESULT,
            data: command_result(result, time_ms),
            final_state: None,
        },
        Body::Finished(finished) => self::finished(finished),
    }
}

fn intermediate(message: &str) -> Mapped {
    Mapped {
        code: legacy::ACTION_CONTINUE,
        data: json!({ "message": message }),
        final_state: None,
    }
}

/// Perl shape (`message`, `command`, `metadata`, `result`, `metrics`) plus
/// the flat fields of the native API.
fn command_result(result: &CommandResult, time_ms: i64) -> Value {
    let failed = result.timed_out || result.exit_code != 0;
    let end = time_ms / 1000;
    let duration = i64::try_from(result.duration_ms / 1000).unwrap_or(i64::MAX);
    json!({
        "message": if failed { "command execution issue" } else { "command has finished successfully" },
        "command": result.command,
        "metadata": null,
        "result": { "exit_code": result.exit_code, "stdout": result.stdout },
        "metrics": { "start": end.saturating_sub(duration), "end": end, "duration": duration },
        "index": result.index,
        "exit_code": result.exit_code,
        "stdout": result.stdout,
        "stderr": result.stderr,
        "duration_ms": result.duration_ms,
        "timed_out": result.timed_out,
    })
}

fn finished(finished: &JobFinished) -> Mapped {
    let (ok, outcome, default_message) = match finished.outcome() {
        Outcome::Ok => (true, "ok", "commands processing has finished successfully"),
        Outcome::Failed => (
            false,
            "failed",
            "commands processing has finished with errors",
        ),
        Outcome::Rejected => (false, "rejected", "action rejected by the agent policy"),
        Outcome::Timeout => (false, "timeout", "action timed out"),
        Outcome::Cancelled => (false, "cancelled", "action cancelled"),
        Outcome::Unspecified => (false, "unspecified", "action finished without outcome"),
    };
    let message = if finished.message.is_empty() {
        default_message
    } else {
        &finished.message
    };
    Mapped {
        code: if ok {
            legacy::ACTION_FINISH_OK
        } else {
            legacy::ACTION_FINISH_KO
        },
        data: json!({ "message": message, "outcome": outcome }),
        final_state: Some(if ok { state::SUCCEEDED } else { state::FAILED }),
    }
}

#[cfg(test)]
mod tests {
    use gorgone_proto::agent::JobStarted;

    use super::*;

    #[test]
    fn command_result_keeps_the_perl_shape() {
        let body = Body::CommandResult(CommandResult {
            index: 1,
            command: "echo ok".into(),
            exit_code: 0,
            stdout: "ok\n".into(),
            stderr: String::new(),
            duration_ms: 2500,
            timed_out: false,
        });
        let mapped = map(&body, 10_000);
        assert_eq!(mapped.code, 100);
        assert!(mapped.final_state.is_none());
        assert_eq!(
            mapped.data["result"],
            json!({"exit_code": 0, "stdout": "ok\n"})
        );
        assert_eq!(
            mapped.data["metrics"],
            json!({"start": 8, "end": 10, "duration": 2})
        );
        assert_eq!(mapped.data["metadata"], Value::Null);
        assert_eq!(mapped.data["message"], "command has finished successfully");
        assert_eq!(mapped.data["index"], 1);
    }

    #[test]
    fn terminal_codes() {
        let ok = map(
            &Body::Finished(JobFinished {
                outcome: Outcome::Ok.into(),
                message: String::new(),
            }),
            0,
        );
        assert_eq!((ok.code, ok.final_state), (2, Some(state::SUCCEEDED)));
        let ko = map(
            &Body::Finished(JobFinished {
                outcome: Outcome::Rejected.into(),
                message: "not allowed".into(),
            }),
            0,
        );
        assert_eq!((ko.code, ko.final_state), (1, Some(state::FAILED)));
        assert_eq!(ko.data["message"], "not allowed");
        assert_eq!(map(&Body::Started(JobStarted {}), 0).code, 4);
    }
}
