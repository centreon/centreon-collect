//! `ServiceControl`: runs the command line, or makes the engine API call or
//! the HTTP calls, the administrator mapped to a `(service, operation)` pair.
//! Nothing of it comes from the central.

use std::collections::BTreeMap;
use std::path::Path;
use std::time::{Duration, Instant};

use gorgone_proto::agent::job_event::Body;
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::service_control::Operation;
use gorgone_proto::agent::{CommandResult, JobFinished, ServiceControl};
use nix::sys::signal::{Signal, kill};
use nix::unistd::Pid;

use super::{Executor, JobContext, command_result, finished};
use crate::config::{EngineRpc, ProcessSignal, Secret, ServiceAction, ServiceConfig};
use crate::engine_api::EngineApi;
use crate::http_post::{self, HttpTarget, PostError};
use crate::monitoring::Monitoring;
use crate::process::{self, CommandSpec};

pub(crate) struct ServiceControlExecutor {
    pub services: BTreeMap<String, ServiceConfig>,
    /// Engine API of the `grpc:` operations; set when `engine.grpc_url` is.
    pub engine: Option<EngineApi>,
    /// Built-in scheduler of the `scheduler:reload` operations.
    pub monitoring: Option<Monitoring>,
    /// Bound of one call of an `http-post:` operation, connection included.
    pub http_timeout: Duration,
    /// Sent with every call of an `http-post:` operation.
    pub bearer: Option<Secret>,
    pub timeout: Duration,
    pub kill_grace: Duration,
    pub max_output: usize,
}

fn operation_name(operation: Operation) -> &'static str {
    match operation {
        Operation::Unspecified => "unspecified",
        Operation::Start => "start",
        Operation::Stop => "stop",
        Operation::Reload => "reload",
        Operation::Restart => "restart",
    }
}

impl ServiceControlExecutor {
    /// Calls every URL in order, one `CommandResult` each. A failure never
    /// skips the URLs after it: they are other services to reach.
    async fn post_all(&self, what: &str, urls: &str, ctx: &JobContext<'_>) -> JobFinished {
        let targets = match HttpTarget::parse_list(urls) {
            Ok(targets) => targets,
            // Refused when the configuration is loaded.
            Err(reason) => return finished(Outcome::Rejected, format!("{what}: {reason}")),
        };
        let total = targets.len();
        let mut accepted = 0;
        let mut report = String::new();
        for (index, target) in targets.iter().enumerate() {
            let started = Instant::now();
            let answer = tokio::select! {
                biased;
                () = ctx.cancel.cancelled() => {
                    return finished(
                        Outcome::Cancelled,
                        format!("{what}: cancelled, {accepted} of {total} HTTP calls accepted{report}"),
                    );
                }
                answer = http_post::post(
                    target,
                    self.bearer.as_ref().map(Secret::expose),
                    self.http_timeout,
                ) => answer,
            };
            let mut result = CommandResult {
                index: u32::try_from(index).unwrap_or(u32::MAX),
                command: format!("POST {}", target.url()),
                exit_code: 1,
                duration_ms: u64::try_from(started.elapsed().as_millis()).unwrap_or(u64::MAX),
                ..CommandResult::default()
            };
            match answer {
                Ok(status) => {
                    if status.accepted() {
                        accepted += 1;
                        result.exit_code = 0;
                    }
                    result.stdout = status.line;
                }
                Err(error) => {
                    result.timed_out = matches!(error, PostError::Timeout(_));
                    result.stderr = error.to_string();
                }
            }
            // One of the two is empty.
            report.push_str(&format!(
                "; {}: {}{}",
                target.url(),
                result.stdout,
                result.stderr
            ));
            ctx.sink.emit(Body::CommandResult(result));
        }
        let outcome = if accepted == total {
            Outcome::Ok
        } else {
            Outcome::Failed
        };
        finished(
            outcome,
            format!("{what}: {accepted} of {total} HTTP calls accepted{report}"),
        )
    }
}

/// Sends `signal` to the process whose pid `pid_file` holds; says what it did.
fn send_signal(signal: ProcessSignal, pid_file: &Path) -> Result<String, String> {
    let text = std::fs::read_to_string(pid_file)
        .map_err(|error| format!("{}: {error}", pid_file.display()))?;
    let pid = text
        .trim()
        .parse::<i32>()
        .ok()
        .filter(|pid| *pid > 0)
        .ok_or_else(|| format!("{}: no process id in the file", pid_file.display()))?;
    let signal = match signal {
        ProcessSignal::Hup => Signal::SIGHUP,
        ProcessSignal::Int => Signal::SIGINT,
        ProcessSignal::Term => Signal::SIGTERM,
        ProcessSignal::Usr1 => Signal::SIGUSR1,
        ProcessSignal::Usr2 => Signal::SIGUSR2,
    };
    let name = signal.as_str();
    kill(Pid::from_raw(pid), signal).map_err(|error| format!("{name} to pid {pid}: {error}"))?;
    Ok(format!("{name} sent to pid {pid}"))
}

impl Executor for ServiceControlExecutor {
    type Action = ServiceControl;

    async fn execute(&self, action: ServiceControl, ctx: &JobContext<'_>) -> JobFinished {
        let Ok(operation) = Operation::try_from(action.operation) else {
            return finished(Outcome::Rejected, "unknown operation");
        };
        let name = operation_name(operation);
        let service_name = &action.service;
        let Some(service) = self.services.get(service_name) else {
            return finished(
                Outcome::Rejected,
                format!("service {service_name:?} is not configured on this agent"),
            );
        };
        let Some(mapped) = service.action(operation) else {
            return finished(
                Outcome::Rejected,
                format!(
                    "operation {name} of service {service_name:?} is not configured on this agent"
                ),
            );
        };
        if ctx.cancel.is_cancelled() {
            return finished(Outcome::Cancelled, "cancelled before start");
        }

        let line = match mapped {
            ServiceAction::Command(line) => line,
            ServiceAction::HttpPost(urls) => {
                return self
                    .post_all(&format!("{name} {service_name}"), urls, ctx)
                    .await;
            }
            ServiceAction::Signal(signal, pid_file) => {
                let (outcome, done) = match send_signal(signal, pid_file) {
                    Ok(done) => (Outcome::Ok, done),
                    Err(error) => (Outcome::Failed, error),
                };
                return finished(outcome, format!("{name} {service_name}: {done}"));
            }
            ServiceAction::SchedulerReload => {
                let Some(monitoring) = &self.monitoring else {
                    return finished(Outcome::Rejected, "the built-in scheduler is not enabled");
                };
                return match monitoring.reload().await {
                    Ok(summary) => finished(
                        Outcome::Ok,
                        format!(
                            "{name} {service_name}: {}",
                            summary.lines().next().unwrap_or_default()
                        ),
                    ),
                    Err(report) => {
                        ctx.sink.emit(Body::CommandResult(CommandResult {
                            command: format!("{name} {service_name}"),
                            exit_code: 1,
                            stdout: report,
                            ..CommandResult::default()
                        }));
                        finished(
                            Outcome::Failed,
                            format!(
                                "{name} {service_name}: configuration refused, the running one is kept"
                            ),
                        )
                    }
                };
            }
            ServiceAction::Engine(rpc) => {
                let asked = match rpc {
                    EngineRpc::Reload => "configuration reload",
                    EngineRpc::Shutdown => "shutdown",
                };
                let Some(engine) = &self.engine else {
                    return finished(Outcome::Rejected, "engine.grpc_url is not configured");
                };
                return match engine.signal(rpc).await {
                    Ok(()) => finished(
                        Outcome::Ok,
                        format!("{name} {service_name}: {asked} accepted by the engine API"),
                    ),
                    Err(error) => finished(
                        Outcome::Failed,
                        format!("{name} {service_name}: {asked} through the engine API: {error}"),
                    ),
                };
            }
        };
        let spec = CommandSpec {
            line,
            shell: false,
            argv: None,
            timeout: self.timeout,
            kill_grace: self.kill_grace,
            max_output: self.max_output,
        };
        let run = process::run(&spec, ctx.cancel).await;
        let (result, outcome) = command_result(0, line, run);
        let exit_code = result.exit_code;
        ctx.sink.emit(Body::CommandResult(result));
        let message = match outcome {
            Outcome::Ok => format!("{name} {service_name}: done"),
            Outcome::Timeout => format!("{name} {service_name}: timed out"),
            Outcome::Cancelled => format!("{name} {service_name}: cancelled"),
            _ => format!("{name} {service_name}: exit code {exit_code}"),
        };
        finished(outcome, message)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::executor::testing::{MemorySink, run};
    use crate::http_post::testing::{FakeHttp, refused_url};

    const OK: &[u8] = b"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";

    fn reload() -> ServiceControl {
        ServiceControl {
            service: "engine".to_owned(),
            operation: Operation::Reload.into(),
        }
    }

    fn posting(urls: &[String]) -> ServiceControlExecutor {
        executor(&format!("http-post:{}", urls.join(" ")))
    }

    fn executor(reload: &str) -> ServiceControlExecutor {
        let service = ServiceConfig {
            reload: Some(reload.to_owned()),
            ..ServiceConfig::default()
        };
        ServiceControlExecutor {
            services: BTreeMap::from([("engine".to_owned(), service)]),
            engine: None,
            monitoring: None,
            http_timeout: Duration::from_millis(300),
            bearer: None,
            timeout: Duration::from_secs(10),
            kill_grace: Duration::from_secs(2),
            max_output: 1024,
        }
    }

    #[tokio::test]
    async fn runs_the_mapped_command() {
        let action = ServiceControl {
            service: "engine".to_owned(),
            operation: Operation::Reload.into(),
        };
        let (done, sink) = run(&executor("echo reloaded"), action).await;
        assert_eq!(done.outcome(), Outcome::Ok);
        let results = sink.command_results();
        assert_eq!(results.len(), 1);
        assert_eq!(results[0].stdout, "reloaded\n");
    }

    #[tokio::test]
    async fn signals_the_process_of_the_pid_file() {
        use std::os::unix::process::ExitStatusExt;

        let dir = tempfile::tempdir().unwrap();
        let pid_file = dir.path().join("cbd.pid");
        let mut child = std::process::Command::new("sleep")
            .arg("30")
            .spawn()
            .unwrap();
        std::fs::write(&pid_file, format!("{}\n", child.id())).unwrap();
        let exec = executor(&format!("signal:TERM:{}", pid_file.display()));
        let (done, sink) = run(&exec, reload()).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{done:?}");
        assert_eq!(
            done.message,
            format!("reload engine: SIGTERM sent to pid {}", child.id())
        );
        assert!(sink.command_results().is_empty());
        assert_eq!(child.wait().unwrap().signal(), Some(Signal::SIGTERM as i32));

        // No pid file, or one without a pid: failed, nothing signalled.
        std::fs::write(&pid_file, "none").unwrap();
        let (done, _) = run(&exec, reload()).await;
        assert_eq!(done.outcome(), Outcome::Failed, "{done:?}");
        let missing = executor(&format!("signal:HUP:{}", dir.path().join("x").display()));
        let (done, _) = run(&missing, reload()).await;
        assert_eq!(done.outcome(), Outcome::Failed, "{done:?}");
    }

    #[tokio::test]
    async fn failure_is_reported() {
        let action = ServiceControl {
            service: "engine".to_owned(),
            operation: Operation::Reload.into(),
        };
        let (done, sink) = run(&executor("false"), action).await;
        assert_eq!(done.outcome(), Outcome::Failed);
        assert_eq!(sink.command_results()[0].exit_code, 1);
    }

    #[tokio::test]
    async fn rejects_what_is_not_configured() {
        let exec = executor("echo reloaded");
        for (service, operation) in [
            ("unknown", Operation::Reload as i32),
            ("engine", Operation::Restart as i32),
            ("engine", Operation::Unspecified as i32),
            ("engine", 42),
        ] {
            let action = ServiceControl {
                service: service.to_owned(),
                operation,
            };
            let (done, sink) = run(&exec, action).await;
            assert_eq!(done.outcome(), Outcome::Rejected, "{service} {operation}");
            assert!(sink.command_results().is_empty());
        }
    }

    #[tokio::test]
    async fn posts_to_every_url_in_order() {
        let first = FakeHttp::start(Some(OK)).await;
        let second = FakeHttp::start(Some(b"HTTP/1.1 202 Accepted\r\n\r\n")).await;
        let urls = [first.url("/reload"), second.url("/reload")];
        let (done, sink) = run(&posting(&urls), reload()).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{done:?}");
        assert_eq!(
            done.message,
            format!(
                "reload engine: 2 of 2 HTTP calls accepted; {}: HTTP/1.1 200 OK; \
                 {}: HTTP/1.1 202 Accepted",
                urls[0], urls[1]
            )
        );
        let results = sink.command_results();
        assert_eq!(results.len(), 2);
        for (index, (result, status)) in results
            .iter()
            .zip(["HTTP/1.1 200 OK", "HTTP/1.1 202 Accepted"])
            .enumerate()
        {
            assert_eq!(result.index as usize, index);
            assert_eq!(result.command, format!("POST {}", urls[index]));
            assert_eq!(result.exit_code, 0);
            assert_eq!(result.stdout, status);
            assert!(result.stderr.is_empty() && !result.timed_out);
        }
        assert!(first.requests()[0].starts_with("POST /reload HTTP/1.1\r\n"));
        assert_eq!(second.requests().len(), 1);
    }

    #[tokio::test]
    async fn one_failing_url_fails_the_job_without_skipping_the_others() {
        let refusing = FakeHttp::start(Some(b"HTTP/1.1 503 Service Unavailable\r\n\r\n")).await;
        let garbled = FakeHttp::start(Some(b"not http\r\n")).await;
        let silent = FakeHttp::start(None).await;
        let last = FakeHttp::start(Some(OK)).await;
        let urls = [
            refusing.url("/reload"),
            refused_url("/reload").await,
            garbled.url("/reload"),
            silent.url("/reload"),
            last.url("/reload"),
        ];
        let (done, sink) = run(&posting(&urls), reload()).await;
        assert_eq!(done.outcome(), Outcome::Failed, "{done:?}");
        assert!(
            done.message
                .starts_with("reload engine: 1 of 5 HTTP calls accepted; "),
            "{done:?}"
        );
        for url in &urls {
            assert!(done.message.contains(url.as_str()), "{done:?}");
        }
        let results = sink.command_results();
        assert_eq!(results.len(), 5);
        let codes: Vec<i32> = results.iter().map(|result| result.exit_code).collect();
        assert_eq!(codes, [1, 1, 1, 1, 0]);
        assert_eq!(results[0].stdout, "HTTP/1.1 503 Service Unavailable");
        assert!(results[0].stderr.is_empty());
        assert!(results[1].stderr.starts_with("connection to "));
        assert!(results[1].stdout.is_empty());
        assert!(results[2].stderr.starts_with("malformed status line"));
        assert_eq!(results[3].stderr, "no answer within 300 ms");
        let timed_out: Vec<bool> = results.iter().map(|result| result.timed_out).collect();
        assert_eq!(timed_out, [false, false, false, true, false]);
        // The URL after the failures was called all the same.
        assert_eq!(last.requests().len(), 1);
    }

    #[tokio::test]
    async fn cancellation_stops_the_calls() {
        let silent = FakeHttp::start(None).await;
        let last = FakeHttp::start(Some(OK)).await;
        let mut exec = posting(&[silent.url("/reload"), last.url("/reload")]);
        exec.http_timeout = Duration::from_secs(30);
        let sink = MemorySink::default();
        let cancel = tokio_util::sync::CancellationToken::new();
        let ctx = JobContext {
            sink: &sink,
            cancel: &cancel,
        };
        let stop = async {
            tokio::time::sleep(Duration::from_millis(100)).await;
            cancel.cancel();
        };
        let (done, ()) = tokio::join!(exec.execute(reload(), &ctx), stop);
        assert_eq!(done.outcome(), Outcome::Cancelled, "{done:?}");
        assert!(sink.command_results().is_empty());
        assert!(last.requests().is_empty());
    }

    #[tokio::test]
    async fn url_list_that_was_not_checked_is_rejected() {
        for value in ["http-post:", "http-post:https://broker:8080/reload"] {
            let (done, sink) = run(&executor(value), reload()).await;
            assert_eq!(done.outcome(), Outcome::Rejected, "{done:?}");
            assert!(sink.command_results().is_empty());
        }
    }
}
