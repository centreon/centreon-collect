//! Journal of recent jobs and of their events: assigns sequence numbers,
//! keeps events until acknowledged, deduplicates dispatches. Kept in memory,
//! and mirrored on disk when a state directory is configured.

use std::collections::{HashMap, VecDeque};
use std::io;
use std::path::Path;
use std::sync::{Mutex, MutexGuard, PoisonError};
use std::time::{SystemTime, UNIX_EPOCH};

use gorgone_proto::agent::job_event::Body;
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::{JobEvent, JobFinished};
use prost::Message;
use tokio::sync::Notify;
use tokio_util::sync::CancellationToken;

use crate::store::Store;

/// Message of the terminal event closing a job the previous process left open.
pub(crate) const INTERRUPTED: &str = "agent restarted during execution";

/// Result of presenting a dispatched job id to the journal.
#[derive(Debug)]
pub(crate) enum Admission {
    /// First time this job is seen: run it.
    New(CancellationToken),
    /// Already queued or running: nothing to do.
    Running,
    /// Already finished: its events are queued for sending again.
    Replayed,
}

struct StoredEvent {
    event: JobEvent,
    acked: bool,
    size: usize,
}

struct JobRecord {
    events: Vec<StoredEvent>,
    finished: bool,
    cancel: CancellationToken,
}

impl JobRecord {
    fn fully_acked(&self) -> bool {
        self.events.iter().all(|e| e.acked)
    }

    fn bytes(&self) -> usize {
        self.events.iter().map(|e| e.size).sum()
    }
}

struct Inner {
    jobs: HashMap<String, JobRecord>,
    /// Job ids, oldest first.
    order: VecDeque<String>,
    /// Events to send on the current session: `(job id, index in events)`.
    unsent: VecDeque<(String, usize)>,
    bytes: usize,
}

/// Journal shared by the session and the executors.
pub(crate) struct Journal {
    inner: Mutex<Inner>,
    /// Disk copy; written under the lock so that it follows the same order.
    store: Option<Store>,
    max_jobs: usize,
    max_bytes: usize,
    /// Wakes the session when an event is ready to be sent.
    outbound: Notify,
    /// Wakes shutdown when a job finishes or an event is acknowledged.
    progress: Notify,
}

fn now_unix_ms() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .ok()
        .and_then(|d| i64::try_from(d.as_millis()).ok())
        .unwrap_or(0)
}

impl Journal {
    /// Memory-only journal.
    #[cfg(test)]
    pub(crate) fn new(max_jobs: usize, max_bytes: usize) -> Self {
        Self::build(max_jobs, max_bytes, None)
    }

    fn build(max_jobs: usize, max_bytes: usize, store: Option<Store>) -> Self {
        Self {
            inner: Mutex::new(Inner {
                jobs: HashMap::new(),
                order: VecDeque::new(),
                unsent: VecDeque::new(),
                bytes: 0,
            }),
            store,
            max_jobs,
            max_bytes,
            outbound: Notify::new(),
            progress: Notify::new(),
        }
    }

    /// Journal mirrored in `state_dir` when given: the jobs of the previous
    /// process are read back, and those it left running are closed as failed.
    pub(crate) fn open(
        max_jobs: usize,
        max_bytes: usize,
        state_dir: Option<&Path>,
    ) -> io::Result<Self> {
        let Some(dir) = state_dir else {
            return Ok(Self::build(max_jobs, max_bytes, None));
        };
        let (store, loaded) = Store::open(dir)?;
        let journal = Self::build(max_jobs, max_bytes, Some(store));
        let mut interrupted = Vec::new();
        let mut inner = journal.lock();
        for job in loaded {
            if job.events.is_empty() {
                // Never started: forgetting it lets the next dispatch run it.
                journal.unpersist(&job.id);
                continue;
            }
            let events: Vec<StoredEvent> = job
                .events
                .into_iter()
                .map(|(event, acked)| StoredEvent {
                    size: event.encoded_len(),
                    event,
                    acked,
                })
                .collect();
            let finished = events
                .last()
                .is_some_and(|last| matches!(last.event.body, Some(Body::Finished(_))));
            if !finished {
                interrupted.push(job.id.clone());
            }
            inner.bytes += events.iter().map(|e| e.size).sum::<usize>();
            inner.order.push_back(job.id.clone());
            inner.jobs.insert(
                job.id,
                JobRecord {
                    events,
                    finished,
                    cancel: CancellationToken::new(),
                },
            );
        }
        drop(inner);
        for job_id in &interrupted {
            tracing::warn!(%job_id, "job left running by the previous process: closed as failed");
            let closing = JobFinished {
                outcome: Outcome::Failed.into(),
                message: INTERRUPTED.to_owned(),
            };
            journal.record(job_id, Body::Finished(closing));
        }
        journal.evict(&mut journal.lock());
        Ok(journal)
    }

    fn persist(&self, job_id: &str, write: impl FnOnce(&Store) -> io::Result<()>) {
        if let Some(store) = &self.store
            && let Err(error) = write(store)
        {
            // The job goes on from memory; only restart safety is lost.
            tracing::warn!(%job_id, %error, "cannot write the job state to disk");
        }
    }

    fn unpersist(&self, job_id: &str) {
        self.persist(job_id, |store| store.remove(job_id));
    }

    // The state stays consistent across a panic: every update is a few
    // infallible steps, so a poisoned lock is safe to reuse.
    fn lock(&self) -> MutexGuard<'_, Inner> {
        self.inner.lock().unwrap_or_else(PoisonError::into_inner)
    }

    /// Registers a dispatched job, or tells why it must not run again.
    pub(crate) fn admit(&self, job_id: &str) -> Admission {
        let mut inner = self.lock();
        if let Some(record) = inner.jobs.get_mut(job_id) {
            if !record.finished {
                return Admission::Running;
            }
            let count = record.events.len();
            for event in &mut record.events {
                event.acked = false;
            }
            inner.unsent.retain(|(id, _)| id != job_id);
            inner
                .unsent
                .extend((0..count).map(|index| (job_id.to_owned(), index)));
            drop(inner);
            self.outbound.notify_one();
            return Admission::Replayed;
        }
        let cancel = CancellationToken::new();
        inner.jobs.insert(
            job_id.to_owned(),
            JobRecord {
                events: Vec::new(),
                finished: false,
                cancel: cancel.clone(),
            },
        );
        inner.order.push_back(job_id.to_owned());
        self.persist(job_id, |store| store.create(job_id, now_unix_ms()));
        self.evict(&mut inner);
        Admission::New(cancel)
    }

    /// Appends an event to a job; ignored once the job has finished, so a
    /// job never has two terminal events.
    pub(crate) fn record(&self, job_id: &str, body: Body) {
        let mut inner = self.lock();
        let Some(record) = inner.jobs.get_mut(job_id) else {
            return;
        };
        if record.finished {
            return;
        }
        let terminal = matches!(body, Body::Finished(_));
        let index = record.events.len();
        let event = JobEvent {
            job_id: job_id.to_owned(),
            seq: index as u64 + 1,
            time_unix_ms: now_unix_ms(),
            body: Some(body),
        };
        let size = event.encoded_len();
        // On disk before it can be sent: an acknowledged event is never lost.
        self.persist(job_id, |store| store.append_event(&event, terminal));
        record.events.push(StoredEvent {
            event,
            acked: false,
            size,
        });
        record.finished = terminal;
        inner.bytes += size;
        inner.unsent.push_back((job_id.to_owned(), index));
        if terminal {
            self.evict(&mut inner);
        }
        drop(inner);
        self.outbound.notify_one();
        if terminal {
            self.progress.notify_waiters();
        }
    }

    /// Marks one event as committed by the server.
    pub(crate) fn ack(&self, job_id: &str, seq: u64) {
        let mut inner = self.lock();
        let Some(event) = seq
            .checked_sub(1)
            .and_then(|index| usize::try_from(index).ok())
            .and_then(|index| inner.jobs.get_mut(job_id)?.events.get_mut(index))
        else {
            return;
        };
        event.acked = true;
        self.persist(job_id, |store| store.append_ack(job_id, seq));
        drop(inner);
        self.progress.notify_waiters();
    }

    /// Drops a job that was admitted but will not run (agent stopping), so
    /// that a later dispatch runs it.
    pub(crate) fn forget(&self, job_id: &str) {
        let mut inner = self.lock();
        self.remove(&mut inner, job_id);
        drop(inner);
        self.progress.notify_waiters();
    }

    /// Starts a new session: every unacknowledged event must be sent again.
    pub(crate) fn begin_session(&self) {
        let mut inner = self.lock();
        let Inner {
            jobs,
            order,
            unsent,
            ..
        } = &mut *inner;
        unsent.clear();
        for job_id in order.iter() {
            let Some(record) = jobs.get(job_id) else {
                continue;
            };
            unsent.extend(
                record
                    .events
                    .iter()
                    .enumerate()
                    .filter(|(_, event)| !event.acked)
                    .map(|(index, _)| (job_id.clone(), index)),
            );
        }
        drop(inner);
        self.outbound.notify_one();
    }

    /// True when an event waits to be sent on the current session.
    pub(crate) fn has_unsent(&self) -> bool {
        !self.lock().unsent.is_empty()
    }

    /// Next event to send on the current session.
    pub(crate) fn pop_unsent(&self) -> Option<JobEvent> {
        let mut inner = self.lock();
        while let Some((job_id, index)) = inner.unsent.pop_front() {
            let event = inner
                .jobs
                .get(&job_id)
                .and_then(|record| record.events.get(index))
                .filter(|stored| !stored.acked)
                .map(|stored| stored.event.clone());
            if event.is_some() {
                return event;
            }
        }
        None
    }

    /// Resolves once an event may be ready to send.
    pub(crate) async fn outbound_ready(&self) {
        self.outbound.notified().await;
    }

    /// Asks a queued or running job to stop.
    pub(crate) fn cancel(&self, job_id: &str) {
        if let Some(record) = self.lock().jobs.get(job_id) {
            record.cancel.cancel();
        }
    }

    /// Asks every unfinished job to stop.
    pub(crate) fn cancel_all(&self) {
        for record in self.lock().jobs.values().filter(|r| !r.finished) {
            record.cancel.cancel();
        }
    }

    /// Number of admitted jobs that have not finished.
    pub(crate) fn unfinished(&self) -> usize {
        self.lock().jobs.values().filter(|r| !r.finished).count()
    }

    fn is_settled(&self) -> bool {
        self.lock()
            .jobs
            .values()
            .all(|record| record.finished && record.fully_acked())
    }

    /// Resolves once every job has finished and every event is acknowledged.
    pub(crate) async fn settled(&self) {
        self.wait_until(Self::is_settled).await;
    }

    /// Resolves once no job is queued or running.
    pub(crate) async fn idle(&self) {
        self.wait_until(|journal| journal.unfinished() == 0).await;
    }

    async fn wait_until(&self, reached: impl Fn(&Self) -> bool) {
        loop {
            let notified = self.progress.notified();
            tokio::pin!(notified);
            // Register before checking, so that no wake-up is lost.
            notified.as_mut().enable();
            if reached(self) {
                return;
            }
            notified.await;
        }
    }

    fn remove(&self, inner: &mut Inner, job_id: &str) {
        self.unpersist(job_id);
        if let Some(record) = inner.jobs.remove(job_id) {
            inner.bytes = inner.bytes.saturating_sub(record.bytes());
            inner.order.retain(|id| id != job_id);
            inner.unsent.retain(|(id, _)| id != job_id);
        }
    }

    /// Enforces the bounds by forgetting finished jobs, oldest first and
    /// acknowledged ones before the others. Unfinished jobs are never dropped.
    fn evict(&self, inner: &mut Inner) {
        while inner.jobs.len() > self.max_jobs || inner.bytes > self.max_bytes {
            let finished = |id: &&String| inner.jobs.get(*id).is_some_and(|r| r.finished);
            let victim = inner
                .order
                .iter()
                .filter(finished)
                .find(|id| inner.jobs.get(*id).is_some_and(JobRecord::fully_acked))
                .or_else(|| inner.order.iter().find(finished))
                .cloned();
            let Some(victim) = victim else {
                return;
            };
            if inner.jobs.get(&victim).is_some_and(|r| !r.fully_acked()) {
                tracing::warn!(job_id = %victim, "journal full: dropping unacknowledged events");
            }
            self.remove(inner, &victim);
        }
    }
}

#[cfg(test)]
mod tests {
    use gorgone_proto::agent::{JobFinished, JobProgress, JobStarted};

    use super::*;

    fn progress(message: &str) -> Body {
        Body::Progress(JobProgress {
            message: message.to_owned(),
        })
    }

    fn finished() -> Body {
        Body::Finished(JobFinished::default())
    }

    fn run_job(journal: &Journal, id: &str) {
        assert!(matches!(journal.admit(id), Admission::New(_)));
        journal.record(id, Body::Started(JobStarted {}));
        journal.record(id, finished());
    }

    fn drain(journal: &Journal) -> Vec<(String, u64)> {
        std::iter::from_fn(|| journal.pop_unsent())
            .map(|event| (event.job_id, event.seq))
            .collect()
    }

    fn durable(dir: &Path, max_jobs: usize) -> Journal {
        Journal::open(max_jobs, usize::MAX, Some(dir)).unwrap()
    }

    fn files(dir: &Path) -> Vec<std::path::PathBuf> {
        std::fs::read_dir(dir)
            .unwrap()
            .map(|entry| entry.unwrap().path())
            .collect()
    }

    #[test]
    fn restart_resends_unacknowledged_events_and_replays_finished_jobs() {
        let dir = tempfile::tempdir().unwrap();
        {
            let journal = durable(dir.path(), 8);
            run_job(&journal, "a");
            run_job(&journal, "b");
            journal.ack("a", 1);
            journal.ack("a", 2);
            journal.ack("b", 1);
        }
        let journal = durable(dir.path(), 8);
        assert_eq!(journal.unfinished(), 0);
        journal.begin_session();
        assert_eq!(drain(&journal), [("b".to_owned(), 2)]);
        // A duplicate dispatch after the restart must not run the job again.
        assert!(matches!(journal.admit("a"), Admission::Replayed));
        assert_eq!(drain(&journal), [("a".to_owned(), 1), ("a".to_owned(), 2)]);
    }

    #[test]
    fn restart_closes_the_job_that_was_running() {
        let dir = tempfile::tempdir().unwrap();
        {
            let journal = durable(dir.path(), 8);
            assert!(matches!(journal.admit("queued"), Admission::New(_)));
            assert!(matches!(journal.admit("running"), Admission::New(_)));
            journal.record("running", Body::Started(JobStarted {}));
            journal.record("running", progress("half way"));
            journal.ack("running", 1);
        }
        for _ in 0..2 {
            // The closing event is itself durable: a second restart adds nothing.
            let journal = durable(dir.path(), 8);
            assert_eq!(journal.unfinished(), 0);
            journal.begin_session();
            let events: Vec<JobEvent> = std::iter::from_fn(|| journal.pop_unsent()).collect();
            assert_eq!(events.len(), 2);
            assert_eq!((events[0].seq, events[1].seq), (2, 3));
            let Some(Body::Finished(closing)) = &events[1].body else {
                panic!("expected a terminal event, got {:?}", events[1]);
            };
            assert_eq!(closing.outcome(), Outcome::Failed);
            assert_eq!(closing.message, INTERRUPTED);
            assert!(matches!(journal.admit("running"), Admission::Replayed));
        }
        // The job that never started is free to run.
        let journal = durable(dir.path(), 8);
        assert!(matches!(journal.admit("queued"), Admission::New(_)));
    }

    #[test]
    fn restart_survives_a_torn_tail() {
        let dir = tempfile::tempdir().unwrap();
        {
            let journal = durable(dir.path(), 8);
            run_job(&journal, "a");
        }
        // The process died while writing the terminal event.
        let path = files(dir.path()).remove(0);
        let bytes = std::fs::read(&path).unwrap();
        std::fs::write(&path, &bytes[..bytes.len() - 3]).unwrap();

        let journal = durable(dir.path(), 8);
        journal.begin_session();
        let events: Vec<JobEvent> = std::iter::from_fn(|| journal.pop_unsent()).collect();
        assert_eq!(events.len(), 2);
        assert!(matches!(events[0].body, Some(Body::Started(_))));
        let Some(Body::Finished(closing)) = &events[1].body else {
            panic!("expected a terminal event, got {:?}", events[1]);
        };
        assert_eq!(closing.message, INTERRUPTED);

        // Pure garbage is dropped without stopping the agent.
        std::fs::write(&path, b"\xff\xff\xff\xff garbage").unwrap();
        let journal = durable(dir.path(), 8);
        assert!(matches!(journal.admit("a"), Admission::New(_)));
    }

    #[test]
    fn bounds_and_forget_apply_to_the_disk_copy() {
        let dir = tempfile::tempdir().unwrap();
        {
            let journal = durable(dir.path(), 3);
            for id in ["a", "b", "c"] {
                run_job(&journal, id);
            }
            journal.ack("b", 1);
            journal.ack("b", 2);
            assert_eq!(files(dir.path()).len(), 3);
            // One job too many: the acknowledged one goes, file included.
            run_job(&journal, "d");
            assert_eq!(files(dir.path()).len(), 3);
            assert!(matches!(journal.admit("b"), Admission::New(_)));
            journal.forget("b");
            assert_eq!(files(dir.path()).len(), 2);
        }
        // A smaller bound at restart trims the oldest jobs.
        let journal = durable(dir.path(), 1);
        assert_eq!(files(dir.path()).len(), 1);
        assert!(matches!(journal.admit("d"), Admission::Replayed));
        assert!(matches!(journal.admit("c"), Admission::New(_)));
    }

    #[test]
    fn sequences_start_at_one_and_stop_after_finished() {
        let journal = Journal::new(8, usize::MAX);
        run_job(&journal, "a");
        journal.record("a", progress("late"));
        journal.record("a", finished());
        journal.record("unknown", progress("x"));
        assert_eq!(drain(&journal), [("a".to_owned(), 1), ("a".to_owned(), 2)]);
        assert!(!journal.has_unsent());
    }

    #[test]
    fn unacknowledged_events_are_resent_on_a_new_session() {
        let journal = Journal::new(8, usize::MAX);
        run_job(&journal, "a");
        run_job(&journal, "b");
        assert_eq!(drain(&journal).len(), 4);
        journal.ack("a", 1);
        journal.ack("b", 2);
        journal.ack("b", 99);
        journal.ack("b", 0);
        journal.begin_session();
        assert_eq!(drain(&journal), [("a".to_owned(), 2), ("b".to_owned(), 1)]);
    }

    #[test]
    fn duplicate_dispatch_is_ignored_or_replayed() {
        let journal = Journal::new(8, usize::MAX);
        assert!(matches!(journal.admit("a"), Admission::New(_)));
        assert!(matches!(journal.admit("a"), Admission::Running));
        journal.record("a", Body::Started(JobStarted {}));
        journal.record("a", finished());
        drain(&journal);
        journal.ack("a", 1);
        journal.ack("a", 2);
        assert!(matches!(journal.admit("a"), Admission::Replayed));
        assert_eq!(drain(&journal), [("a".to_owned(), 1), ("a".to_owned(), 2)]);
    }

    #[test]
    fn history_is_bounded_and_keeps_unfinished_jobs() {
        let journal = Journal::new(2, usize::MAX);
        assert!(matches!(journal.admit("running"), Admission::New(_)));
        run_job(&journal, "old");
        run_job(&journal, "new");
        assert!(matches!(journal.admit("running"), Admission::Running));
        assert!(matches!(journal.admit("new"), Admission::Replayed));
        // "old" was forgotten: it would run again.
        assert!(matches!(journal.admit("old"), Admission::New(_)));
    }

    #[test]
    fn byte_budget_drops_acknowledged_jobs_first() {
        let journal = Journal::new(100, 300);
        for id in ["a", "b"] {
            assert!(matches!(journal.admit(id), Admission::New(_)));
            journal.record(id, progress(&"x".repeat(100)));
            journal.record(id, finished());
        }
        journal.ack("b", 1);
        journal.ack("b", 2);
        assert!(matches!(journal.admit("c"), Admission::New(_)));
        journal.record("c", progress(&"x".repeat(100)));
        journal.record("c", finished());
        assert!(matches!(journal.admit("b"), Admission::New(_)));
        assert!(matches!(journal.admit("a"), Admission::Replayed));
    }

    #[test]
    fn cancel_reaches_the_job_token() {
        let journal = Journal::new(8, usize::MAX);
        let Admission::New(token) = journal.admit("a") else {
            panic!("expected a new job");
        };
        assert_eq!(journal.unfinished(), 1);
        journal.cancel("other");
        assert!(!token.is_cancelled());
        journal.cancel("a");
        assert!(token.is_cancelled());
    }

    #[tokio::test]
    async fn settled_waits_for_finish_and_acks() {
        let journal = std::sync::Arc::new(Journal::new(8, usize::MAX));
        journal.settled().await;
        run_job(&journal, "a");
        let waiter = tokio::spawn({
            let journal = journal.clone();
            async move { journal.settled().await }
        });
        tokio::task::yield_now().await;
        assert!(!waiter.is_finished());
        journal.ack("a", 1);
        journal.ack("a", 2);
        tokio::time::timeout(std::time::Duration::from_secs(2), waiter)
            .await
            .unwrap()
            .unwrap();
    }
}
