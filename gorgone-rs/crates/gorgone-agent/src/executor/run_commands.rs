//! `RunCommands`: allowlisted commands, executed one after the other.

use std::time::Duration;

use gorgone_core::allowlist::{Allowlist, Denied};
use gorgone_proto::agent::job_event::Body;
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::{JobFinished, RunCommands};

use super::{Executor, JobContext, command_result, finished};
use crate::process::{self, CommandSpec};

pub(crate) struct RunCommandsExecutor {
    pub allowlist: Allowlist,
    pub shell: bool,
    pub default_timeout: Duration,
    pub kill_grace: Duration,
    pub max_output: usize,
}

impl RunCommandsExecutor {
    /// Applies the local policy to one command. Without a shell, returns the
    /// exact argument vector to execute.
    fn authorize(&self, command: &str) -> Result<Option<Vec<String>>, Denied> {
        if !self.shell {
            return self.allowlist.authorize(command).map(Some);
        }
        // A newline would start a second shell command behind a matching first line.
        if command.chars().any(char::is_control) {
            return Err(Denied::ControlCharacter);
        }
        if !self.allowlist.is_allowed(command) {
            return Err(Denied::NotListed);
        }
        Ok(None)
    }
}

impl Executor for RunCommandsExecutor {
    type Action = RunCommands;

    async fn execute(&self, action: RunCommands, ctx: &JobContext<'_>) -> JobFinished {
        // The whole job is checked first: nothing runs when one command is refused.
        let mut authorised = Vec::with_capacity(action.commands.len());
        for (index, command) in action.commands.iter().enumerate() {
            match self.authorize(&command.command) {
                Ok(argv) => authorised.push(argv),
                Err(reason) => {
                    return finished(Outcome::Rejected, format!("command {index}: {reason}"));
                }
            }
        }

        let total = action.commands.len();
        let mut failed = 0_usize;
        let mut timed_out = false;
        let mut executed = 0_usize;
        for (index, (command, argv)) in action.commands.iter().zip(&authorised).enumerate() {
            if ctx.cancel.is_cancelled() {
                return finished(
                    Outcome::Cancelled,
                    format!("cancelled after {executed}/{total} commands"),
                );
            }
            let spec = CommandSpec {
                line: &command.command,
                shell: self.shell,
                argv: argv.as_deref(),
                timeout: match command.timeout_s {
                    0 => self.default_timeout,
                    seconds => Duration::from_secs(u64::from(seconds)),
                },
                kill_grace: self.kill_grace,
                max_output: self.max_output,
            };
            let run = process::run(&spec, ctx.cancel).await;
            let (result, outcome) = command_result(index, &command.command, run);
            ctx.sink.emit(Body::CommandResult(result));
            executed += 1;
            match outcome {
                Outcome::Ok => continue,
                Outcome::Cancelled => {
                    return finished(
                        Outcome::Cancelled,
                        format!("cancelled during command {index}"),
                    );
                }
                Outcome::Timeout => timed_out = true,
                _ => {}
            }
            failed += 1;
            if !command.continue_on_error {
                break;
            }
        }

        let summary = format!("{executed}/{total} commands executed, {failed} failed");
        if timed_out {
            finished(Outcome::Timeout, summary)
        } else if failed > 0 {
            finished(Outcome::Failed, summary)
        } else {
            finished(Outcome::Ok, summary)
        }
    }
}

#[cfg(test)]
mod tests {
    use gorgone_proto::agent::Command;

    use super::*;
    use crate::executor::testing::run;

    fn executor(patterns: &[&str]) -> RunCommandsExecutor {
        RunCommandsExecutor {
            allowlist: Allowlist::new(patterns).unwrap(),
            shell: false,
            default_timeout: Duration::from_secs(10),
            kill_grace: Duration::from_secs(2),
            max_output: 1024,
        }
    }

    fn command(line: &str, continue_on_error: bool) -> Command {
        Command {
            command: line.to_owned(),
            timeout_s: 0,
            continue_on_error,
        }
    }

    fn action(commands: Vec<Command>) -> RunCommands {
        RunCommands { commands }
    }

    #[tokio::test]
    async fn runs_allowlisted_commands() {
        let (done, sink) = run(
            &executor(&["echo .*"]),
            action(vec![
                command("echo one", false),
                command("echo 'two words'", false),
            ]),
        )
        .await;
        assert_eq!(done.outcome(), Outcome::Ok);
        let results = sink.command_results();
        assert_eq!(results.len(), 2);
        assert_eq!((results[0].index, results[0].stdout.as_str()), (0, "one\n"));
        assert_eq!(
            (results[1].index, results[1].stdout.as_str()),
            (1, "two words\n")
        );
    }

    #[tokio::test]
    async fn one_refused_command_rejects_the_whole_job() {
        let dir = tempfile::tempdir().unwrap();
        let marker = dir.path().join("marker");
        let touch = format!("touch {}", marker.display());
        let (done, sink) = run(
            &executor(&["touch .*"]),
            action(vec![
                command(&touch, false),
                command("rm -rf /tmp/x", false),
            ]),
        )
        .await;
        assert_eq!(done.outcome(), Outcome::Rejected);
        assert!(sink.command_results().is_empty());
        assert!(!marker.exists());
    }

    #[tokio::test]
    async fn path_escape_and_control_characters_are_rejected() {
        let dir = tempfile::tempdir().unwrap();
        let marker = dir.path().join("marker");
        let exec = executor(&[r"/usr/lib/centreon/plugins/\S+( .*)?", "echo .*"]);
        let escape = format!(
            "/usr/lib/centreon/plugins/../../../bin/sh -c 'touch {}'",
            marker.display()
        );
        let second_line = format!("echo ok\ntouch {}", marker.display());
        for (line, needle) in [
            (escape.as_str(), "program path"),
            ("/usr/lib/centreon/plugins/./check_x", "program path"),
            (second_line.as_str(), "control character"),
            ("echo a\0b", "control character"),
            ("echo 'unbalanced", "cannot be parsed"),
            ("rm -rf /", "does not match"),
        ] {
            let (done, sink) = run(
                &exec,
                action(vec![command("echo first", false), command(line, false)]),
            )
            .await;
            assert_eq!(done.outcome(), Outcome::Rejected, "{line:?}");
            assert!(done.message.starts_with("command 1: "), "{}", done.message);
            assert!(done.message.contains(needle), "{}", done.message);
            assert!(sink.command_results().is_empty());
        }
        assert!(!marker.exists());
    }

    #[tokio::test]
    async fn shell_mode_still_refuses_control_characters() {
        let dir = tempfile::tempdir().unwrap();
        let marker = dir.path().join("marker");
        let mut exec = executor(&["(?s)echo .*"]);
        exec.shell = true;
        let (done, sink) = run(&exec, action(vec![command("echo a; echo b", false)])).await;
        assert_eq!(done.outcome(), Outcome::Ok);
        assert_eq!(sink.command_results()[0].stdout, "a\nb\n");

        // The pattern matches across lines; the newline is refused anyway.
        let injected = format!("echo ok\ntouch {}", marker.display());
        let (done, sink) = run(&exec, action(vec![command(&injected, false)])).await;
        assert_eq!(done.outcome(), Outcome::Rejected);
        assert!(
            done.message.contains("control character"),
            "{}",
            done.message
        );
        assert!(sink.command_results().is_empty());
        assert!(!marker.exists());
    }

    #[tokio::test]
    async fn stops_at_first_failure_unless_told_otherwise() {
        let exec = executor(&["false", "echo .*", "/missing/tool"]);
        let (done, sink) = run(
            &exec,
            action(vec![command("false", false), command("echo no", false)]),
        )
        .await;
        assert_eq!(done.outcome(), Outcome::Failed);
        assert_eq!(sink.command_results().len(), 1);

        let (done, sink) = run(
            &exec,
            action(vec![
                command("false", true),
                command("/missing/tool", true),
                command("echo yes", false),
            ]),
        )
        .await;
        assert_eq!(done.outcome(), Outcome::Failed);
        let results = sink.command_results();
        assert_eq!(results.len(), 3);
        assert_eq!(results[0].exit_code, 1);
        assert_eq!(results[1].exit_code, -1);
        assert!(results[1].stderr.contains("/missing/tool"));
        assert_eq!(results[2].stdout, "yes\n");
    }

    #[tokio::test]
    async fn timeout_gives_timeout_outcome() {
        let mut slow = command("sleep 30", false);
        slow.timeout_s = 1;
        let (done, sink) = run(&executor(&["sleep 30"]), action(vec![slow])).await;
        assert_eq!(done.outcome(), Outcome::Timeout);
        let results = sink.command_results();
        assert!(results[0].timed_out);
        assert!(results[0].duration_ms < 10_000);
    }

    #[tokio::test]
    async fn empty_job_is_ok() {
        let (done, _) = run(&executor(&[]), action(vec![])).await;
        assert_eq!(done.outcome(), Outcome::Ok);
    }
}
