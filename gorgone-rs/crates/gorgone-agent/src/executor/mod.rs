//! Job executors, one per action kind.

mod apply_config;
mod engine_commands;
mod run_commands;
mod service_control;
mod verify;

use std::future::Future;

use gorgone_proto::agent::job_event::Body;
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::{CommandResult, JobFinished, JobProgress};
use tokio_util::sync::CancellationToken;

pub(crate) use apply_config::{ApplyConfigExecutor, BlobFetcher, FetchError};
pub(crate) use engine_commands::{EngineCommandsExecutor, EngineTransport};
pub(crate) use run_commands::RunCommandsExecutor;
pub(crate) use service_control::ServiceControlExecutor;
pub(crate) use verify::{ConfigChecker, remove_leftovers};

use crate::process::{ProcessError, ProcessOutput};

/// Receives the intermediate events of a job.
pub(crate) trait EventSink: Send + Sync {
    /// Records one event body; sequence numbers are assigned by the sink.
    fn emit(&self, body: Body);
}

/// What an executor gets besides its action.
pub(crate) struct JobContext<'a> {
    pub sink: &'a dyn EventSink,
    pub cancel: &'a CancellationToken,
}

impl JobContext<'_> {
    pub(crate) fn progress(&self, message: impl Into<String>) {
        self.sink.emit(Body::Progress(JobProgress {
            message: message.into(),
        }));
    }
}

/// Runs one kind of action. The caller emits `started` before and the
/// returned terminal event after, so an executor only emits what is between.
pub(crate) trait Executor {
    /// Action payload handled by this executor.
    type Action;

    fn execute(
        &self,
        action: Self::Action,
        ctx: &JobContext<'_>,
    ) -> impl Future<Output = JobFinished> + Send;
}

pub(crate) fn finished(outcome: Outcome, message: impl Into<String>) -> JobFinished {
    JobFinished {
        outcome: outcome.into(),
        message: message.into(),
    }
}

/// Builds the event of one command, started or not, and tells its outcome.
pub(crate) fn command_result(
    index: usize,
    command: &str,
    run: Result<ProcessOutput, ProcessError>,
) -> (CommandResult, Outcome) {
    let index = u32::try_from(index).unwrap_or(u32::MAX);
    match run {
        Ok(output) => {
            let outcome = if output.cancelled {
                Outcome::Cancelled
            } else if output.timed_out {
                Outcome::Timeout
            } else if output.succeeded() {
                Outcome::Ok
            } else {
                Outcome::Failed
            };
            let result = CommandResult {
                index,
                command: command.to_owned(),
                exit_code: output.exit_code,
                stdout: output.stdout,
                stderr: output.stderr,
                duration_ms: u64::try_from(output.duration.as_millis()).unwrap_or(u64::MAX),
                timed_out: output.timed_out,
            };
            (result, outcome)
        }
        Err(error) => {
            let result = CommandResult {
                index,
                command: command.to_owned(),
                exit_code: -1,
                stderr: error.to_string(),
                ..CommandResult::default()
            };
            (result, Outcome::Failed)
        }
    }
}

#[cfg(test)]
pub(crate) mod testing {
    use std::sync::Mutex;

    use super::*;

    /// Sink keeping the emitted bodies.
    #[derive(Default)]
    pub(crate) struct MemorySink(Mutex<Vec<Body>>);

    impl EventSink for MemorySink {
        fn emit(&self, body: Body) {
            self.0.lock().unwrap().push(body);
        }
    }

    impl MemorySink {
        pub(crate) fn command_results(&self) -> Vec<CommandResult> {
            self.0
                .lock()
                .unwrap()
                .iter()
                .filter_map(|body| match body {
                    Body::CommandResult(result) => Some(result.clone()),
                    _ => None,
                })
                .collect()
        }

        pub(crate) fn progress(&self) -> Vec<String> {
            self.0
                .lock()
                .unwrap()
                .iter()
                .filter_map(|body| match body {
                    Body::Progress(progress) => Some(progress.message.clone()),
                    _ => None,
                })
                .collect()
        }
    }

    /// Runs `executor` with a fresh sink and no cancellation.
    pub(crate) async fn run<E: Executor>(
        executor: &E,
        action: E::Action,
    ) -> (JobFinished, MemorySink) {
        let sink = MemorySink::default();
        let cancel = CancellationToken::new();
        let ctx = JobContext {
            sink: &sink,
            cancel: &cancel,
        };
        let finished = executor.execute(action, &ctx).await;
        (finished, sink)
    }
}
