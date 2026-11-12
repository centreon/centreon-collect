//! `ServiceControl`: runs the command line the administrator mapped to a
//! `(service, operation)` pair. Nothing of the command comes from the central.

use std::collections::BTreeMap;
use std::time::Duration;

use gorgone_proto::agent::job_event::Body;
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::service_control::Operation;
use gorgone_proto::agent::{JobFinished, ServiceControl};

use super::{Executor, JobContext, command_result, finished};
use crate::config::ServiceConfig;
use crate::process::{self, CommandSpec};

pub(crate) struct ServiceControlExecutor {
    pub services: BTreeMap<String, ServiceConfig>,
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
        let Some(line) = service.command(operation) else {
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

        let spec = CommandSpec {
            line,
            shell: false,
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
    use crate::executor::testing::run;

    fn executor(reload: &str) -> ServiceControlExecutor {
        let service = ServiceConfig {
            reload: Some(reload.to_owned()),
            ..ServiceConfig::default()
        };
        ServiceControlExecutor {
            services: BTreeMap::from([("engine".to_owned(), service)]),
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
}
