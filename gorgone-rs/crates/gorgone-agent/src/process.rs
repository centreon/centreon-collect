//! Child process helper: no shell by default, bounded output, whole process
//! group stopped (SIGTERM, then SIGKILL) on timeout or cancellation.

use std::process::{ExitStatus, Stdio};
use std::time::{Duration, Instant};

use nix::sys::signal::{Signal, killpg};
use nix::unistd::Pid;
use tokio::io::{AsyncRead, AsyncReadExt};
use tokio::process::{Child, Command};
use tokio_util::sync::CancellationToken;

/// Time left to the output readers once the child has exited.
const DRAIN_GRACE: Duration = Duration::from_secs(1);
const TRUNCATED_MARKER: &str = "\n[output truncated]";

/// How to run one command line.
#[derive(Debug, Clone)]
pub(crate) struct CommandSpec<'a> {
    /// Command line, as configured or as received.
    pub line: &'a str,
    /// Give the line to `sh -c` instead of splitting it into arguments.
    pub shell: bool,
    /// Argument vector already derived from `line` by the policy check;
    /// executed as is, so that what was authorised is what runs.
    pub argv: Option<&'a [String]>,
    /// Time after which the process group is stopped.
    pub timeout: Duration,
    /// Time between SIGTERM and SIGKILL when the group is stopped.
    pub kill_grace: Duration,
    /// Cap on stdout and on stderr, in bytes.
    pub max_output: usize,
}

/// Outcome of a command that could be started.
#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) struct ProcessOutput {
    /// Exit code, or 128 + signal number when killed by a signal.
    pub exit_code: i32,
    pub stdout: String,
    pub stderr: String,
    pub duration: Duration,
    pub timed_out: bool,
    pub cancelled: bool,
}

impl ProcessOutput {
    pub(crate) fn succeeded(&self) -> bool {
        self.exit_code == 0 && !self.timed_out && !self.cancelled
    }
}

/// Reasons a command could not be started.
#[derive(Debug, thiserror::Error)]
pub(crate) enum ProcessError {
    #[error("invalid command line: {0}")]
    Parse(#[from] shell_words::ParseError),
    #[error("empty command line")]
    Empty,
    #[error("cannot start {program}: {source}")]
    Spawn {
        program: String,
        source: std::io::Error,
    },
    #[error("cannot wait for {program}: {source}")]
    Wait {
        program: String,
        source: std::io::Error,
    },
}

/// Runs `spec` to completion, timeout or cancellation.
pub(crate) async fn run(
    spec: &CommandSpec<'_>,
    cancel: &CancellationToken,
) -> Result<ProcessOutput, ProcessError> {
    let argv = match spec.argv {
        Some(argv) => argv.to_vec(),
        None if spec.shell => ["sh", "-c", spec.line].map(str::to_owned).to_vec(),
        None => shell_words::split(spec.line)?,
    };
    let mut argv = argv.into_iter();
    let program = argv.next().ok_or(ProcessError::Empty)?;
    let args: Vec<String> = argv.collect();

    let started = Instant::now();
    let mut child = Command::new(&program)
        .args(&args)
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        // Own group, so that the children of the command die with it.
        .process_group(0)
        .kill_on_drop(true)
        .spawn()
        .map_err(|source| ProcessError::Spawn {
            program: program.clone(),
            source,
        })?;
    let group = child.id().and_then(|pid| i32::try_from(pid).ok());

    let stop_readers = CancellationToken::new();
    let stdout = tokio::spawn(capture(
        child.stdout.take(),
        spec.max_output,
        stop_readers.clone(),
    ));
    let stderr = tokio::spawn(capture(
        child.stderr.take(),
        spec.max_output,
        stop_readers.clone(),
    ));

    let mut timed_out = false;
    let mut cancelled = false;
    let waited = tokio::select! {
        status = child.wait() => status,
        () = tokio::time::sleep(spec.timeout) => {
            timed_out = true;
            terminate(&mut child, group, spec.kill_grace).await
        }
        () = cancel.cancelled() => {
            cancelled = true;
            terminate(&mut child, group, spec.kill_grace).await
        }
    };
    let status = match waited {
        Ok(status) => status,
        Err(source) => {
            signal_group(group, Signal::SIGKILL);
            stop_readers.cancel();
            return Err(ProcessError::Wait { program, source });
        }
    };

    // A detached grandchild may keep the pipes open: do not wait for it.
    let stopper = stop_readers.clone();
    let guard = tokio::spawn(async move {
        tokio::time::sleep(DRAIN_GRACE).await;
        stopper.cancel();
    });
    let stdout = stdout.await.unwrap_or_default();
    let stderr = stderr.await.unwrap_or_default();
    guard.abort();

    Ok(ProcessOutput {
        exit_code: exit_code(status),
        stdout,
        stderr,
        duration: started.elapsed(),
        timed_out,
        cancelled,
    })
}

fn exit_code(status: ExitStatus) -> i32 {
    use std::os::unix::process::ExitStatusExt;
    status
        .code()
        .or_else(|| status.signal().map(|signal| 128 + signal))
        .unwrap_or(-1)
}

/// Stops the process group: SIGTERM, then SIGKILL once `grace` has passed.
async fn terminate(
    child: &mut Child,
    group: Option<i32>,
    grace: Duration,
) -> std::io::Result<ExitStatus> {
    signal_group(group, Signal::SIGTERM);
    let status = match tokio::time::timeout(grace, child.wait()).await {
        Ok(status) => status,
        Err(_) => {
            signal_group(group, Signal::SIGKILL);
            child.wait().await
        }
    };
    // Members of the group that ignored SIGTERM must not outlive the command.
    signal_group(group, Signal::SIGKILL);
    status
}

fn signal_group(group: Option<i32>, signal: Signal) {
    if let Some(pgid) = group {
        // ESRCH only means the group is already gone.
        let _ = killpg(Pid::from_raw(pgid), signal);
    }
}

/// Reads a pipe to its end, keeping at most `max` bytes but draining the
/// rest so that the child never blocks on a full pipe.
async fn capture<R>(reader: Option<R>, max: usize, stop: CancellationToken) -> String
where
    R: AsyncRead + Unpin,
{
    let Some(mut reader) = reader else {
        return String::new();
    };
    let mut kept = Vec::new();
    let mut truncated = false;
    let mut chunk = [0_u8; 8192];
    loop {
        let read = tokio::select! {
            read = reader.read(&mut chunk) => read,
            () = stop.cancelled() => break,
        };
        match read {
            Ok(0) | Err(_) => break,
            Ok(n) => {
                let room = max.saturating_sub(kept.len()).min(n);
                kept.extend_from_slice(&chunk[..room]);
                truncated |= room < n;
            }
        }
    }
    let mut text = String::from_utf8_lossy(&kept).into_owned();
    if truncated {
        text.push_str(TRUNCATED_MARKER);
    }
    text
}

#[cfg(test)]
mod tests {
    use super::*;

    fn spec(line: &str, shell: bool, timeout_ms: u64) -> CommandSpec<'_> {
        CommandSpec {
            line,
            shell,
            argv: None,
            timeout: Duration::from_millis(timeout_ms),
            kill_grace: Duration::from_secs(5),
            max_output: 1024,
        }
    }

    #[tokio::test]
    async fn sigterm_first_lets_the_process_clean_up() {
        let line = "trap 'echo bye; exit 7' TERM; echo ready; sleep 60 & wait";
        let out = run_ok(&spec(line, true, 500)).await;
        assert!(out.timed_out);
        // The handler ran: SIGKILL would have given 137 and no "bye".
        assert_eq!((out.exit_code, out.stdout.as_str()), (7, "ready\nbye\n"));
        assert!(out.duration < Duration::from_secs(4), "{:?}", out.duration);
    }

    #[tokio::test]
    async fn sigkill_follows_when_sigterm_is_ignored() {
        // An ignored signal stays ignored in the `sleep` the shell execs.
        let mut spec = spec("trap '' TERM; exec sleep 60", true, 300);
        spec.kill_grace = Duration::from_millis(400);
        let out = run_ok(&spec).await;
        assert!(out.timed_out);
        assert_eq!(out.exit_code, 128 + 9);
        assert!(
            out.duration >= Duration::from_millis(700),
            "{:?}",
            out.duration
        );
        assert!(out.duration < Duration::from_secs(10), "{:?}", out.duration);
    }

    #[tokio::test]
    async fn survivor_of_sigterm_in_the_group_is_killed() {
        let dir = tempfile::tempdir().unwrap();
        let pid_file = dir.path().join("pid");
        // The shell dies on SIGTERM, its background child ignores it.
        let line = format!(
            "(trap '' TERM; exec sleep 60) & echo $! > {}; wait",
            pid_file.display()
        );
        let out = run_ok(&spec(&line, true, 500)).await;
        assert!(out.timed_out);
        assert!(out.duration < Duration::from_secs(4), "{:?}", out.duration);
        let pid: i32 = std::fs::read_to_string(&pid_file)
            .unwrap()
            .trim()
            .parse()
            .unwrap();
        for _ in 0..50 {
            if nix::sys::signal::kill(Pid::from_raw(pid), None).is_err() {
                return;
            }
            tokio::time::sleep(Duration::from_millis(20)).await;
        }
        panic!("process {pid} ignored SIGTERM and survived");
    }

    async fn run_ok(spec: &CommandSpec<'_>) -> ProcessOutput {
        run(spec, &CancellationToken::new()).await.unwrap()
    }

    #[tokio::test]
    async fn captures_output_and_exit_code() {
        let out = run_ok(&spec("sh -c 'echo out; echo err >&2; exit 3'", false, 5000)).await;
        assert_eq!(
            (out.exit_code, out.stdout.as_str(), out.stderr.as_str()),
            (3, "out\n", "err\n")
        );
        assert!(!out.succeeded());
    }

    #[tokio::test]
    async fn no_shell_means_no_shell_syntax() {
        let out = run_ok(&spec("echo a; echo $HOME | cat", false, 5000)).await;
        assert_eq!(out.stdout, "a; echo $HOME | cat\n");
        let out = run_ok(&spec("echo a; echo b", true, 5000)).await;
        assert_eq!(out.stdout, "a\nb\n");
    }

    #[tokio::test]
    async fn truncates_output() {
        let mut spec = spec("head -c 100000 /dev/zero", false, 5000);
        spec.max_output = 10;
        let out = run_ok(&spec).await;
        assert_eq!(out.exit_code, 0);
        assert_eq!(out.stdout.len(), 10 + TRUNCATED_MARKER.len());
        assert!(out.stdout.ends_with(TRUNCATED_MARKER));
    }

    #[tokio::test]
    async fn start_errors() {
        let cancel = CancellationToken::new();
        let missing = run(&spec("/nonexistent/binary", false, 1000), &cancel).await;
        assert!(matches!(missing, Err(ProcessError::Spawn { .. })));
        let empty = run(&spec("   ", false, 1000), &cancel).await;
        assert!(matches!(empty, Err(ProcessError::Empty)));
        let quote = run(&spec("echo 'open", false, 1000), &cancel).await;
        assert!(matches!(quote, Err(ProcessError::Parse(_))));
    }

    #[tokio::test]
    async fn timeout_kills_the_whole_group() {
        let dir = tempfile::tempdir().unwrap();
        let pid_file = dir.path().join("pid");
        let line = format!("sleep 60 & echo $! > {}; wait", pid_file.display());
        let out = run_ok(&spec(&line, true, 500)).await;
        assert!(out.timed_out);
        assert_eq!(out.exit_code, 128 + 15);
        assert!(out.duration < Duration::from_secs(4));

        let pid: i32 = std::fs::read_to_string(&pid_file)
            .unwrap()
            .trim()
            .parse()
            .unwrap();
        // The background `sleep` was in the group; give the kernel a moment.
        for _ in 0..50 {
            if nix::sys::signal::kill(Pid::from_raw(pid), None).is_err() {
                return;
            }
            tokio::time::sleep(Duration::from_millis(20)).await;
        }
        panic!("grandchild {pid} survived the timeout");
    }

    #[tokio::test]
    async fn cancellation_kills_the_process() {
        let cancel = CancellationToken::new();
        let trigger = cancel.clone();
        tokio::spawn(async move {
            tokio::time::sleep(Duration::from_millis(200)).await;
            trigger.cancel();
        });
        let out = run(&spec("sleep 60", false, 30_000), &cancel)
            .await
            .unwrap();
        assert!(out.cancelled && !out.timed_out);
        assert!(out.duration < Duration::from_secs(10));
    }

    #[tokio::test]
    async fn detached_grandchild_does_not_block() {
        // `setsid` leaves the group and keeps the pipes open.
        let out = run_ok(&spec("setsid sleep 5 & echo done", true, 10_000)).await;
        assert_eq!(out.stdout, "done\n");
        assert!(out.duration < Duration::from_secs(4));
    }
}
