//! Retention: what survives a restart of the engine, as Engine's
//! `retention.dat` does (`engine/src/retention/dump.cc`): the state of each
//! host and service, downtimes, comments and acknowledgements. Not that
//! file's format: one JSON document of this engine, `retention.json`,
//! written aside, synced and renamed (never half written), every
//! `retention_update_interval` minutes and at a clean stop, read once at
//! start. An object or a downtime target that is gone is skipped.

use std::collections::HashMap;
use std::fs::{self, File, OpenOptions};
use std::io::{self, Write};
use std::os::unix::fs::OpenOptionsExt;
use std::path::Path;

use serde::{Deserialize, Serialize};

use crate::book::{Acknowledgement, Comment, Downtime, Key};
use crate::state::CheckState;

use super::Core;

/// Version of the file format.
const FORMAT: u32 = 1;

#[derive(Serialize, Deserialize)]
struct RetainedObject {
    host_id: u64,
    service_id: u64,
    state: CheckState,
}

#[derive(Serialize, Deserialize)]
struct File_ {
    format: u32,
    saved_at: i64,
    objects: Vec<RetainedObject>,
    downtimes: Vec<Downtime>,
    comments: Vec<Comment>,
    acknowledgements: Vec<Acknowledgement>,
    last_downtime_id: u64,
}

/// What a retention file holds, until the objects exist again.
#[derive(Default)]
pub(crate) struct Retained {
    pub states: HashMap<Key, CheckState>,
    pub downtimes: Vec<Downtime>,
    pub comments: Vec<Comment>,
    pub acknowledgements: Vec<Acknowledgement>,
    pub last_downtime_id: u64,
}

/// Reads the retention file; nothing when it is absent or unreadable (the
/// engine starts as without one, and says why).
pub(crate) fn read(path: &Path) -> Option<Retained> {
    let bytes = match fs::read(path) {
        Ok(bytes) => bytes,
        Err(error) if error.kind() == io::ErrorKind::NotFound => return None,
        Err(error) => {
            tracing::warn!(file = %path.display(), %error, "retention file not read");
            return None;
        }
    };
    let file: File_ = match serde_json::from_slice(&bytes) {
        Ok(file) => file,
        Err(error) => {
            tracing::warn!(file = %path.display(), %error, "retention file not read");
            return None;
        }
    };
    if file.format != FORMAT {
        tracing::warn!(file = %path.display(), format = file.format, "unknown retention format");
        return None;
    }
    tracing::info!(
        file = %path.display(),
        objects = file.objects.len(),
        downtimes = file.downtimes.len(),
        "retention file read"
    );
    Some(Retained {
        states: file
            .objects
            .into_iter()
            .map(|o| ((o.host_id, o.service_id), o.state))
            .collect(),
        downtimes: file.downtimes,
        comments: file.comments,
        acknowledgements: file.acknowledgements,
        last_downtime_id: file.last_downtime_id,
    })
}

/// Replaces `path` with `bytes`: written aside, synced, renamed.
fn write_atomically(path: &Path, bytes: &[u8]) -> io::Result<()> {
    let tmp = path.with_extension("tmp");
    let mut file = OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(true)
        .mode(0o600)
        .open(&tmp)?;
    file.write_all(bytes)?;
    file.sync_all()?;
    fs::rename(&tmp, path)?;
    if let Some(dir) = path.parent() {
        File::open(dir)?.sync_all()?;
    }
    Ok(())
}

impl Core {
    /// Writes the retention file. A periodic write happens off this task;
    /// the one of a clean stop (`last`) before it returns.
    pub(crate) fn save_retention(&mut self, last: bool) {
        let Some(path) = self.options.retention_file.clone() else {
            return;
        };
        // Nothing loaded yet: what the file holds is still the truth.
        if self.loaded.is_none() {
            return;
        }
        let file = File_ {
            format: FORMAT,
            saved_at: self.clock.now(),
            objects: self
                .objs
                .iter()
                .map(|obj| RetainedObject {
                    host_id: obj.key.0,
                    service_id: obj.key.1,
                    state: obj.state.clone(),
                })
                .collect(),
            downtimes: self.book.downtimes.values().cloned().collect(),
            comments: self.book.comments.values().cloned().collect(),
            acknowledgements: self.book.acknowledgements.values().cloned().collect(),
            last_downtime_id: self.book.last_downtime_id(),
        };
        let bytes = match serde_json::to_vec(&file) {
            Ok(bytes) => bytes,
            Err(error) => {
                tracing::error!(%error, "retention not serialized");
                return;
            }
        };
        let write = move || {
            if let Err(error) = write_atomically(&path, &bytes) {
                tracing::error!(file = %path.display(), %error, "retention file not written");
            } else {
                tracing::debug!(file = %path.display(), "retention file written");
            }
        };
        if last {
            write();
        } else {
            tokio::task::spawn_blocking(write);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn unreadable_files_are_ignored() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("retention.json");
        assert!(read(&path).is_none());
        std::fs::write(&path, "{").unwrap();
        assert!(read(&path).is_none());
        write_atomically(
            &path,
            br#"{"format":9,"saved_at":0,"objects":[],"downtimes":[],"comments":[],
                "acknowledgements":[],"last_downtime_id":0}"#,
        )
        .unwrap();
        assert!(read(&path).is_none());
        assert!(!path.with_extension("tmp").exists());
    }
}
