//! On-disk copy of the journal: one append-only file per job, so that an
//! agent restart neither loses events nor runs a job twice.
//!
//! A file is a sequence of records `tag (1) | length (4, LE) | checksum (4, LE)
//! | payload`. A record that is incomplete or does not match its checksum
//! ends the file: it is the tail a crash left behind.

use std::fs::{self, DirBuilder, File, OpenOptions};
use std::io::{self, Write};
use std::os::unix::fs::{DirBuilderExt, OpenOptionsExt};
use std::path::{Path, PathBuf};

use gorgone_proto::agent::JobEvent;
use gorgone_proto::agent::job_event::Body;
use prost::Message;

const EXTENSION: &str = "job";
const HEADER_LEN: usize = 9;
/// First record: admission time (8, LE) then the job id.
const TAG_JOB: u8 = b'J';
/// One `JobEvent`, protobuf encoded.
const TAG_EVENT: u8 = b'E';
/// Sequence number (8, LE) of an acknowledged event.
const TAG_ACK: u8 = b'A';

/// A job read back from disk.
#[derive(Debug)]
pub(crate) struct LoadedJob {
    pub id: String,
    pub admitted_ms: i64,
    /// Events in sequence order, with their acknowledged flag.
    pub events: Vec<(JobEvent, bool)>,
}

/// Directory holding the job files.
pub(crate) struct Store {
    dir: PathBuf,
}

// FNV-1a: enough to tell a torn or overwritten record from a whole one.
fn checksum(tag: u8, payload: &[u8]) -> u32 {
    std::iter::once(&tag)
        .chain(payload)
        .fold(0x811c_9dc5_u32, |hash, byte| {
            (hash ^ u32::from(*byte)).wrapping_mul(0x0100_0193)
        })
}

fn encode(tag: u8, payload: &[u8]) -> io::Result<Vec<u8>> {
    let length = u32::try_from(payload.len())
        .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "record too large"))?;
    let mut record = Vec::with_capacity(HEADER_LEN + payload.len());
    record.push(tag);
    record.extend_from_slice(&length.to_le_bytes());
    record.extend_from_slice(&checksum(tag, payload).to_le_bytes());
    record.extend_from_slice(payload);
    Ok(record)
}

/// Splits the next whole record off `bytes`.
fn next_record(bytes: &[u8]) -> Option<(u8, &[u8], &[u8])> {
    let (header, rest) = bytes.split_at_checked(HEADER_LEN)?;
    let tag = header[0];
    let length = u32::from_le_bytes(header[1..5].try_into().ok()?);
    let expected = u32::from_le_bytes(header[5..9].try_into().ok()?);
    let (payload, rest) = rest.split_at_checked(usize::try_from(length).ok()?)?;
    (checksum(tag, payload) == expected).then_some((tag, payload, rest))
}

/// Decodes a job file; also returns the length of its valid prefix.
fn parse(bytes: &[u8]) -> (Option<LoadedJob>, usize) {
    let mut job: Option<LoadedJob> = None;
    let mut rest = bytes;
    while let Some((tag, payload, after)) = next_record(rest) {
        let accepted = match tag {
            TAG_JOB if job.is_none() => {
                job = parse_header(payload);
                job.is_some()
            }
            TAG_EVENT => job.as_mut().is_some_and(|job| push_event(job, payload)),
            TAG_ACK => job.as_mut().is_some_and(|job| apply_ack(job, payload)),
            _ => false,
        };
        if !accepted {
            break;
        }
        rest = after;
    }
    (job, bytes.len() - rest.len())
}

fn parse_header(payload: &[u8]) -> Option<LoadedJob> {
    let (time, id) = payload.split_at_checked(8)?;
    Some(LoadedJob {
        admitted_ms: i64::from_le_bytes(time.try_into().ok()?),
        id: String::from_utf8(id.to_vec()).ok()?,
        events: Vec::new(),
    })
}

/// Accepts the event only if it continues the job: right id, next sequence
/// number, nothing after the terminal event.
fn push_event(job: &mut LoadedJob, payload: &[u8]) -> bool {
    let finished = job
        .events
        .last()
        .is_some_and(|(event, _)| matches!(event.body, Some(Body::Finished(_))));
    match JobEvent::decode(payload) {
        Ok(event)
            if !finished
                && event.job_id == job.id
                && event.seq == job.events.len() as u64 + 1
                && event.body.is_some() =>
        {
            job.events.push((event, false));
            true
        }
        _ => false,
    }
}

fn apply_ack(job: &mut LoadedJob, payload: &[u8]) -> bool {
    let Ok(seq) = <[u8; 8]>::try_from(payload) else {
        return false;
    };
    let acked = u64::from_le_bytes(seq)
        .checked_sub(1)
        .and_then(|index| usize::try_from(index).ok())
        .and_then(|index| job.events.get_mut(index));
    if let Some((_, acked)) = acked {
        *acked = true;
    }
    true
}

impl Store {
    /// Opens `dir`, creating it if needed, and reads the jobs it holds,
    /// oldest first. Unusable files are removed, torn tails cut off.
    pub(crate) fn open(dir: &Path) -> io::Result<(Self, Vec<LoadedJob>)> {
        DirBuilder::new().recursive(true).mode(0o700).create(dir)?;
        let store = Self {
            dir: dir.to_path_buf(),
        };
        let mut jobs = Vec::new();
        for entry in fs::read_dir(dir)? {
            let path = entry?.path();
            if path.extension().is_none_or(|ext| ext != EXTENSION) {
                continue;
            }
            let bytes = fs::read(&path)?;
            let (job, valid) = parse(&bytes);
            // The name must be the one appends will use, or the file is orphaned.
            let job = job.filter(|job| store.path(&job.id) == path);
            let Some(job) = job else {
                tracing::warn!(path = %path.display(), "unusable job file removed");
                fs::remove_file(&path)?;
                continue;
            };
            if valid < bytes.len() {
                tracing::warn!(job_id = %job.id, "torn tail of the job file cut off");
                // Later appends must follow the last whole record.
                OpenOptions::new()
                    .write(true)
                    .open(&path)?
                    .set_len(valid as u64)?;
            }
            jobs.push(job);
        }
        jobs.sort_by(|a, b| (a.admitted_ms, &a.id).cmp(&(b.admitted_ms, &b.id)));
        Ok((store, jobs))
    }

    // Job ids come from the server: never use them as a file name.
    fn path(&self, job_id: &str) -> PathBuf {
        let name = gorgone_core::bundle::digest(job_id.as_bytes());
        self.dir.join(format!("{name}.{EXTENSION}"))
    }

    fn append(&self, job_id: &str, tag: u8, payload: &[u8], create: bool) -> io::Result<File> {
        let record = encode(tag, payload)?;
        let mut file = OpenOptions::new()
            .append(true)
            .create_new(create)
            .mode(0o600)
            .open(self.path(job_id))?;
        // One write per record: a crash leaves at most one torn record.
        file.write_all(&record)?;
        Ok(file)
    }

    /// Starts the file of a newly admitted job.
    pub(crate) fn create(&self, job_id: &str, admitted_ms: i64) -> io::Result<()> {
        let mut payload = admitted_ms.to_le_bytes().to_vec();
        payload.extend_from_slice(job_id.as_bytes());
        self.append(job_id, TAG_JOB, &payload, true)?.sync_all()?;
        // Makes the new name itself durable.
        File::open(&self.dir)?.sync_all()
    }

    /// Appends an event; `sync` forces it to disk before returning.
    pub(crate) fn append_event(&self, event: &JobEvent, sync: bool) -> io::Result<()> {
        let file = self.append(&event.job_id, TAG_EVENT, &event.encode_to_vec(), false)?;
        if sync {
            file.sync_all()?;
        }
        Ok(())
    }

    /// Records an acknowledgement. Not synced: losing it only costs a resend.
    pub(crate) fn append_ack(&self, job_id: &str, seq: u64) -> io::Result<()> {
        self.append(job_id, TAG_ACK, &seq.to_le_bytes(), false)
            .map(drop)
    }

    /// Deletes the file of a forgotten job.
    pub(crate) fn remove(&self, job_id: &str) -> io::Result<()> {
        match fs::remove_file(self.path(job_id)) {
            Err(error) if error.kind() != io::ErrorKind::NotFound => Err(error),
            _ => Ok(()),
        }
    }
}

#[cfg(test)]
mod tests {
    use gorgone_proto::agent::{JobFinished, JobStarted};

    use super::*;

    fn event(job_id: &str, seq: u64, body: Body) -> JobEvent {
        JobEvent {
            job_id: job_id.to_owned(),
            seq,
            time_unix_ms: 1,
            body: Some(body),
        }
    }

    fn started(job_id: &str) -> JobEvent {
        event(job_id, 1, Body::Started(JobStarted {}))
    }

    fn finished(job_id: &str, seq: u64) -> JobEvent {
        event(job_id, seq, Body::Finished(JobFinished::default()))
    }

    fn reopen(dir: &Path) -> Vec<LoadedJob> {
        Store::open(dir).unwrap().1
    }

    #[test]
    fn round_trip_keeps_order_events_and_acks() {
        let dir = tempfile::tempdir().unwrap();
        let (store, jobs) = Store::open(dir.path()).unwrap();
        assert!(jobs.is_empty());
        // Ids that would be dangerous as file names.
        for (id, time) in [("../second", 20), ("first/..", 10)] {
            store.create(id, time).unwrap();
            store.append_event(&started(id), false).unwrap();
            store.append_event(&finished(id, 2), true).unwrap();
        }
        store.append_ack("../second", 2).unwrap();
        store.append_ack("../second", 7).unwrap();
        assert!(store.create("first/..", 30).is_err());

        let jobs = reopen(dir.path());
        assert_eq!(jobs.len(), 2);
        assert_eq!((jobs[0].id.as_str(), jobs[0].admitted_ms), ("first/..", 10));
        assert_eq!(jobs[1].id, "../second");
        assert_eq!(jobs[1].events[0], (started("../second"), false));
        assert_eq!(jobs[1].events[1], (finished("../second", 2), true));
        assert_eq!(fs::read_dir(dir.path()).unwrap().count(), 2);

        store.remove("first/..").unwrap();
        store.remove("first/..").unwrap();
        assert_eq!(reopen(dir.path()).len(), 1);
    }

    #[test]
    fn torn_or_corrupted_tail_is_cut_off() {
        let dir = tempfile::tempdir().unwrap();
        let (store, _) = Store::open(dir.path()).unwrap();
        store.create("a", 1).unwrap();
        store.append_event(&started("a"), false).unwrap();
        let path = store.path("a");
        let whole = fs::metadata(&path).unwrap().len();
        store.append_event(&finished("a", 2), true).unwrap();
        let full = fs::read(&path).unwrap();

        // Crash in the middle of the last record.
        fs::write(&path, &full[..full.len() - 1]).unwrap();
        let jobs = reopen(dir.path());
        assert_eq!(jobs[0].events, [(started("a"), false)]);
        assert_eq!(fs::metadata(&path).unwrap().len(), whole);

        // Appends after the repair are readable.
        store.append_event(&finished("a", 2), true).unwrap();
        assert_eq!(reopen(dir.path())[0].events.len(), 2);

        // A flipped payload byte no longer matches the checksum.
        let mut flipped = full.clone();
        *flipped.last_mut().unwrap() ^= 0xff;
        fs::write(&path, &flipped).unwrap();
        assert_eq!(reopen(dir.path())[0].events.len(), 1);

        // Garbage after the terminal event is dropped, the events are kept.
        let mut trailing = full.clone();
        trailing.extend_from_slice(b"garbage that is not a record");
        fs::write(&path, &trailing).unwrap();
        assert_eq!(reopen(dir.path())[0].events.len(), 2);
        assert_eq!(fs::read(&path).unwrap(), full);
    }

    #[test]
    fn inconsistent_records_end_the_file() {
        let dir = tempfile::tempdir().unwrap();
        let (store, _) = Store::open(dir.path()).unwrap();
        store.create("a", 1).unwrap();
        store.append_event(&started("a"), false).unwrap();
        // A gap in the sequence numbers.
        store.append_event(&finished("a", 5), false).unwrap();
        assert_eq!(reopen(dir.path())[0].events.len(), 1);
        // An event of another job, written to this file.
        let record = encode(TAG_EVENT, &finished("b", 2).encode_to_vec()).unwrap();
        let mut file = OpenOptions::new()
            .append(true)
            .open(store.path("a"))
            .unwrap();
        file.write_all(&record).unwrap();
        assert_eq!(reopen(dir.path())[0].events.len(), 1);
        // Nothing follows a terminal event.
        store.append_event(&finished("a", 2), false).unwrap();
        store.append_event(&finished("a", 3), false).unwrap();
        assert_eq!(reopen(dir.path())[0].events.len(), 2);
    }

    #[test]
    fn unusable_files_are_removed_and_others_ignored() {
        let dir = tempfile::tempdir().unwrap();
        fs::write(dir.path().join("broken.job"), b"not a job file").unwrap();
        fs::write(dir.path().join("empty.job"), b"").unwrap();
        fs::write(dir.path().join("README"), b"kept").unwrap();
        let (store, _) = Store::open(dir.path()).unwrap();
        store.create("a", 1).unwrap();
        // A valid file under a name that is not the one of its job.
        fs::copy(store.path("a"), dir.path().join("copy.job")).unwrap();

        let jobs = reopen(dir.path());
        assert_eq!(jobs.len(), 1);
        let mut names: Vec<_> = fs::read_dir(dir.path())
            .unwrap()
            .map(|entry| entry.unwrap().file_name().into_string().unwrap())
            .collect();
        names.sort();
        let job_file = store.path("a").file_name().unwrap().to_owned();
        let mut expected = vec!["README".to_owned(), job_file.into_string().unwrap()];
        expected.sort();
        assert_eq!(names, expected);
    }
}
