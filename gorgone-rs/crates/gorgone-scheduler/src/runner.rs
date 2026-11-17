//! Execution of one plugin: no shell, own process group, bounded output,
//! killed at its timeout; then the result read as Engine reads it
//! (`engine/src/commands/raw_v2.cc:395-406`, `engine/src/service.cc:1255-1310`).

use std::process::Stdio;
use std::time::{Duration, Instant};

use nix::sys::signal::{Signal, killpg};
use nix::unistd::Pid;
use tokio::io::{AsyncRead, AsyncReadExt};
use tokio::process::Command;

use crate::state::code;

/// What a plugin run gave.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct PluginRun {
    /// Exit code; `None` when killed by a signal or never started.
    pub exit_code: Option<i32>,
    /// Killed at its timeout.
    pub timed_out: bool,
    /// Standard output, at most the cap.
    pub stdout: String,
    /// Standard error, at most the cap.
    pub stderr: String,
    /// Could not be started: why.
    pub spawn_error: Option<String>,
    /// Run time.
    pub duration: Duration,
}

/// Reads `reader` to its end, keeping the first `cap` bytes only: the rest
/// is read and dropped so that the plugin never blocks on a full pipe.
async fn read_capped(mut reader: impl AsyncRead + Unpin, cap: usize) -> Vec<u8> {
    let mut kept = Vec::new();
    let mut chunk = [0u8; 4096];
    loop {
        match reader.read(&mut chunk).await {
            Ok(0) | Err(_) => return kept,
            Ok(n) => {
                let room = cap.saturating_sub(kept.len());
                kept.extend_from_slice(&chunk[..n.min(room)]);
            }
        }
    }
}

/// Runs `argv` (program first) and waits for it, at most `timeout`.
pub async fn run(argv: &[String], timeout: Duration, max_output: usize) -> PluginRun {
    let started = Instant::now();
    let Some((program, args)) = argv.split_first() else {
        return PluginRun {
            spawn_error: Some("empty command line".to_owned()),
            ..PluginRun::default()
        };
    };
    let mut command = Command::new(program);
    command
        .args(args)
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        // Its own group: the timeout kills what the plugin started too.
        .process_group(0)
        .kill_on_drop(true);
    let mut child = match command.spawn() {
        Ok(child) => child,
        Err(error) => {
            return PluginRun {
                spawn_error: Some(error.to_string()),
                exit_code: (error.kind() == std::io::ErrorKind::NotFound).then_some(127),
                duration: started.elapsed(),
                ..PluginRun::default()
            };
        }
    };
    let pid = child.id().and_then(|pid| i32::try_from(pid).ok());
    let stdout = child.stdout.take();
    let stderr = child.stderr.take();
    let work = async {
        let out = async {
            match stdout {
                Some(pipe) => read_capped(pipe, max_output).await,
                None => Vec::new(),
            }
        };
        let err = async {
            match stderr {
                Some(pipe) => read_capped(pipe, max_output).await,
                None => Vec::new(),
            }
        };
        let (out, err) = tokio::join!(out, err);
        let status = child.wait().await;
        (out, err, status)
    };
    match tokio::time::timeout(timeout, work).await {
        Ok((out, err, status)) => PluginRun {
            exit_code: status.ok().and_then(|status| status.code()),
            timed_out: false,
            stdout: String::from_utf8_lossy(&out).into_owned(),
            stderr: String::from_utf8_lossy(&err).into_owned(),
            spawn_error: None,
            duration: started.elapsed(),
        },
        Err(_) => {
            if let Some(pid) = pid {
                let _ = killpg(Pid::from_raw(pid), Signal::SIGKILL);
            }
            // `work` is dropped: kill_on_drop reaps the child.
            PluginRun {
                timed_out: true,
                duration: started.elapsed(),
                ..PluginRun::default()
            }
        }
    }
}

/// State code (0 to 3) and raw output of a run, as Engine sees a service
/// check result.
#[must_use]
pub fn result_of(run: &PluginRun) -> (u32, String) {
    if run.timed_out {
        return (code::UNKNOWN, "(Process Timeout)".to_owned());
    }
    let out_of_bounds = |code: i32| {
        let hint = match code {
            126 => " - plugin may not be executable",
            127 => " - plugin may be missing",
            _ => "",
        };
        format!("(Return code of {code} is out of bounds{hint})")
    };
    match (run.exit_code, &run.spawn_error) {
        (Some(code), _) if (0..=3).contains(&code) && run.spawn_error.is_none() => {
            let output = if run.stdout.is_empty() {
                run.stderr.clone()
            } else {
                run.stdout.clone()
            };
            (code.unsigned_abs(), output)
        }
        (Some(code), _) => (code::UNKNOWN, out_of_bounds(code)),
        (None, _) => (
            code::UNKNOWN,
            "(Service check did not exit properly)".to_owned(),
        ),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn argv(parts: &[&str]) -> Vec<String> {
        parts.iter().map(|part| (*part).to_owned()).collect()
    }

    #[tokio::test]
    async fn captures_output_and_code() {
        let run = run(
            &argv(&["/bin/sh", "-c", "echo 'WARN | a=1'; echo err >&2; exit 1"]),
            Duration::from_secs(5),
            1024,
        )
        .await;
        assert_eq!(run.exit_code, Some(1));
        assert_eq!(result_of(&run), (1, "WARN | a=1\n".to_owned()));
        assert_eq!(run.stderr, "err\n");
    }

    #[tokio::test]
    async fn stderr_when_stdout_is_empty_and_cap() {
        let run = run(
            &argv(&[
                "/bin/sh",
                "-c",
                "echo only-err >&2; head -c 100000 /dev/zero; exit 2",
            ]),
            Duration::from_secs(5),
            16,
        )
        .await;
        assert_eq!(run.stdout.len(), 16);
        assert_eq!(run.exit_code, Some(2));
        let run = super::run(
            &argv(&["/bin/sh", "-c", "echo only-err >&2; exit 2"]),
            Duration::from_secs(5),
            16,
        )
        .await;
        assert_eq!(result_of(&run), (2, "only-err\n".to_owned()));
    }

    #[tokio::test]
    async fn timeout_kills_the_whole_group() {
        let started = Instant::now();
        let run = run(
            &argv(&["/bin/sh", "-c", "sleep 30 & sleep 30; echo late"]),
            Duration::from_millis(300),
            1024,
        )
        .await;
        assert!(run.timed_out);
        assert!(started.elapsed() < Duration::from_secs(5));
        assert_eq!(result_of(&run), (3, "(Process Timeout)".to_owned()));
    }

    #[tokio::test]
    async fn out_of_bounds_and_missing() {
        let run = run(
            &argv(&["/bin/sh", "-c", "exit 7"]),
            Duration::from_secs(5),
            64,
        )
        .await;
        assert_eq!(
            result_of(&run),
            (3, "(Return code of 7 is out of bounds)".to_owned())
        );
        let run = super::run(&argv(&["/nonexistent/plugin"]), Duration::from_secs(5), 64).await;
        assert_eq!(
            result_of(&run),
            (
                3,
                "(Return code of 127 is out of bounds - plugin may be missing)".to_owned()
            )
        );
    }
}
