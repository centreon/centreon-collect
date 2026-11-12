//! `EngineCommands`: external commands written to the engine command pipe.

use std::fs::{File, OpenOptions};
use std::io::{self, Write};
use std::os::unix::fs::{FileTypeExt, OpenOptionsExt};
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::{EngineCommands, JobFinished};
use nix::errno::Errno;
use nix::fcntl::OFlag;

use super::{Executor, JobContext, finished};

/// Time given to the engine to make room in a full pipe.
const WRITE_PATIENCE: Duration = Duration::from_secs(5);

pub(crate) struct EngineCommandsExecutor {
    pub command_file: PathBuf,
}

#[derive(Debug, thiserror::Error)]
enum PipeError {
    #[error("{0}: {1}")]
    Unavailable(PathBuf, io::Error),
    #[error("{0} is not a FIFO")]
    NotFifo(PathBuf),
    #[error("{0}: no process reads the pipe, is the engine running?")]
    NoReader(PathBuf),
    #[error("{path}: write failed after {written} commands: {source}")]
    Write {
        path: PathBuf,
        written: usize,
        source: io::Error,
    },
}

fn open_pipe(path: &Path) -> Result<File, PipeError> {
    let unavailable = |e| PipeError::Unavailable(path.to_path_buf(), e);
    let kind = std::fs::metadata(path).map_err(unavailable)?.file_type();
    if !kind.is_fifo() {
        return Err(PipeError::NotFifo(path.to_path_buf()));
    }
    // Non-blocking: opening a FIFO nobody reads fails instead of hanging.
    let pipe = OpenOptions::new()
        .write(true)
        .custom_flags(OFlag::O_NONBLOCK.bits())
        .open(path)
        .map_err(|e| match e.raw_os_error() {
            Some(code) if code == Errno::ENXIO as i32 => PipeError::NoReader(path.to_path_buf()),
            _ => unavailable(e),
        })?;
    // The path may have been replaced between the two calls.
    let opened = pipe.metadata().map_err(unavailable)?.file_type();
    if !opened.is_fifo() {
        return Err(PipeError::NotFifo(path.to_path_buf()));
    }
    Ok(pipe)
}

/// `write_all` on a non-blocking pipe: waits a bounded time when it is full.
fn write_line(pipe: &mut File, line: &[u8]) -> io::Result<()> {
    let deadline = Instant::now() + WRITE_PATIENCE;
    let mut rest = line;
    while !rest.is_empty() {
        match pipe.write(rest) {
            Ok(0) => return Err(io::ErrorKind::WriteZero.into()),
            Ok(n) => rest = &rest[n..],
            Err(e) if e.kind() == io::ErrorKind::Interrupted => {}
            Err(e) if e.kind() == io::ErrorKind::WouldBlock => {
                if Instant::now() >= deadline {
                    return Err(io::Error::new(
                        io::ErrorKind::TimedOut,
                        "pipe full, the engine does not read it",
                    ));
                }
                std::thread::sleep(Duration::from_millis(20));
            }
            Err(e) => return Err(e),
        }
    }
    Ok(())
}

fn write_commands(path: &Path, commands: &[String]) -> Result<usize, PipeError> {
    let mut pipe = open_pipe(path)?;
    let mut written = 0;
    for command in commands {
        let line = command.trim_end_matches(['\r', '\n']);
        if line.is_empty() {
            continue;
        }
        write_line(&mut pipe, format!("{line}\n").as_bytes()).map_err(|source| {
            PipeError::Write {
                path: path.to_path_buf(),
                written,
                source,
            }
        })?;
        written += 1;
    }
    Ok(written)
}

impl Executor for EngineCommandsExecutor {
    type Action = EngineCommands;

    async fn execute(&self, action: EngineCommands, ctx: &JobContext<'_>) -> JobFinished {
        if ctx.cancel.is_cancelled() {
            return finished(Outcome::Cancelled, "cancelled before start");
        }
        let path = self.command_file.clone();
        let written =
            tokio::task::spawn_blocking(move || write_commands(&path, &action.commands)).await;
        match written {
            Ok(Ok(count)) => finished(
                Outcome::Ok,
                format!("{count} commands written to the engine command pipe"),
            ),
            Ok(Err(error)) => finished(Outcome::Failed, error.to_string()),
            Err(error) => finished(Outcome::Failed, format!("writer task failed: {error}")),
        }
    }
}

#[cfg(test)]
mod tests {
    use std::io::Read;

    use nix::sys::stat::Mode;

    use super::*;
    use crate::executor::testing::run;

    fn action(commands: &[&str]) -> EngineCommands {
        EngineCommands {
            commands: commands.iter().map(|c| (*c).to_owned()).collect(),
        }
    }

    #[tokio::test]
    async fn writes_newline_terminated_lines() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("centengine.cmd");
        nix::unistd::mkfifo(&path, Mode::from_bits_truncate(0o600)).unwrap();
        let reader = std::thread::spawn({
            let path = path.clone();
            move || {
                let mut content = String::new();
                File::open(path)
                    .unwrap()
                    .read_to_string(&mut content)
                    .unwrap();
                content
            }
        });
        // The reader's open only returns once a writer comes: retry until then.
        let exec = EngineCommandsExecutor { command_file: path };
        let mut done = None;
        for _ in 0..100 {
            let (attempt, _) = run(
                &exec,
                action(&[
                    "[1] RESTART_PROGRAM",
                    "[2] SCHEDULE_FORCED_HOST_CHECK;h;2\n",
                    "",
                ]),
            )
            .await;
            if attempt.outcome() == Outcome::Ok {
                done = Some(attempt);
                break;
            }
            tokio::time::sleep(Duration::from_millis(20)).await;
        }
        let done = done.expect("the reader never opened the pipe");
        assert!(done.message.starts_with("2 commands"), "{}", done.message);
        assert_eq!(
            reader.join().unwrap(),
            "[1] RESTART_PROGRAM\n[2] SCHEDULE_FORCED_HOST_CHECK;h;2\n"
        );
    }

    #[tokio::test]
    async fn fails_without_reader_or_fifo() {
        let dir = tempfile::tempdir().unwrap();
        let fifo = dir.path().join("fifo");
        nix::unistd::mkfifo(&fifo, Mode::from_bits_truncate(0o600)).unwrap();
        let regular = dir.path().join("regular");
        std::fs::write(&regular, "").unwrap();

        for (path, needle) in [
            (fifo, "no process reads"),
            (regular.clone(), "not a FIFO"),
            (dir.path().join("missing"), "missing"),
        ] {
            let exec = EngineCommandsExecutor { command_file: path };
            let (done, _) = run(&exec, action(&["[1] X"])).await;
            assert_eq!(done.outcome(), Outcome::Failed);
            assert!(done.message.contains(needle), "{}", done.message);
        }
        assert_eq!(std::fs::read_to_string(regular).unwrap(), "");
    }
}
