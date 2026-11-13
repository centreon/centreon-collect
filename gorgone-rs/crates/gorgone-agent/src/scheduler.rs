//! Job scheduling: `RunCommands` jobs share a bounded pool, the other kinds
//! go through one queue so that they run in dispatch order.

use std::sync::Arc;
use std::time::{Duration, Instant};

use gorgone_proto::agent::action::Kind;
use gorgone_proto::agent::job_event::Body;
use gorgone_proto::agent::job_finished::Outcome;
use gorgone_proto::agent::{Job, JobFinished, JobStarted};
use tokio::sync::{Semaphore, mpsc};
use tokio_util::sync::CancellationToken;

use crate::client::GrpcBlobFetcher;
use crate::executor::{
    ApplyConfigExecutor, EngineCommandsExecutor, EventSink, Executor, JobContext,
    RunCommandsExecutor, ServiceControlExecutor, finished,
};
use crate::journal::{Admission, Journal};

/// The executors of the action kinds; `apply_config` also runs `ValidateConfig`.
pub(crate) struct Executors {
    pub run_commands: RunCommandsExecutor,
    pub engine_commands: EngineCommandsExecutor,
    pub apply_config: ApplyConfigExecutor<GrpcBlobFetcher>,
    pub service_control: ServiceControlExecutor,
}

struct JournalSink<'a> {
    journal: &'a Journal,
    job_id: &'a str,
}

impl EventSink for JournalSink<'_> {
    fn emit(&self, body: Body) {
        self.journal.record(self.job_id, body);
    }
}

struct Shared {
    journal: Arc<Journal>,
    executors: Executors,
    /// Set when the agent stops: no job starts any more.
    draining: CancellationToken,
}

struct Admitted {
    job: Job,
    cancel: CancellationToken,
    /// When the dispatch was received: the origin of the job time to live.
    received: Instant,
}

/// Entry point of dispatched jobs.
pub(crate) struct Scheduler {
    shared: Arc<Shared>,
    pool: Arc<Semaphore>,
    ordered: mpsc::UnboundedSender<Admitted>,
}

/// Time left before the deadline of a job: `None` without deadline, zero
/// once it has passed. Counted from reception on the monotonic clock, so the
/// wall clocks of the poller and of the central need not agree.
fn time_left(ttl_ms: u64, received: Instant) -> Option<Duration> {
    (ttl_ms > 0).then(|| Duration::from_millis(ttl_ms).saturating_sub(received.elapsed()))
}

impl Shared {
    async fn run(&self, admitted: Admitted) {
        let Admitted {
            job,
            cancel,
            received,
        } = admitted;
        if self.draining.is_cancelled() {
            // Never started: without event the server queues it again.
            self.journal.forget(&job.id);
            return;
        }
        let sink = JournalSink {
            journal: &self.journal,
            job_id: &job.id,
        };
        sink.emit(Body::Started(JobStarted {}));
        let left = time_left(job.ttl_ms, received);
        let done = if left.is_some_and(|left| left.is_zero()) {
            finished(Outcome::Timeout, "deadline passed before the job started")
        } else if cancel.is_cancelled() {
            finished(Outcome::Cancelled, "cancelled before start")
        } else {
            let ctx = JobContext {
                sink: &sink,
                cancel: &cancel,
            };
            let execution = self.execute(job.action.and_then(|action| action.kind), &ctx);
            tokio::pin!(execution);
            let expiry = async {
                match left {
                    Some(left) => tokio::time::sleep(left).await,
                    None => std::future::pending().await,
                }
            };
            tokio::select! {
                done = &mut execution => done,
                () = expiry => {
                    // Same path as a cancellation: processes are stopped, then
                    // the executor returns.
                    cancel.cancel();
                    let done = execution.await;
                    if done.outcome() == Outcome::Cancelled {
                        finished(Outcome::Timeout, "deadline passed during execution")
                    } else {
                        done
                    }
                }
            }
        };
        tracing::info!(
            job_id = %job.id,
            outcome = ?done.outcome(),
            message = %done.message,
            "job finished"
        );
        sink.emit(Body::Finished(done));
    }

    async fn execute(&self, kind: Option<Kind>, ctx: &JobContext<'_>) -> JobFinished {
        let executors = &self.executors;
        match kind {
            Some(Kind::RunCommands(action)) => executors.run_commands.execute(action, ctx).await,
            Some(Kind::EngineCommands(action)) => {
                executors.engine_commands.execute(action, ctx).await
            }
            Some(Kind::ApplyConfig(action)) => executors.apply_config.execute(action, ctx).await,
            Some(Kind::ValidateConfig(action)) => {
                executors.apply_config.validate(action, ctx).await
            }
            Some(Kind::ServiceControl(action)) => {
                executors.service_control.execute(action, ctx).await
            }
            None => finished(Outcome::Rejected, "action kind not supported by this agent"),
        }
    }
}

impl Scheduler {
    /// Creates the scheduler and starts the worker of the ordered kinds.
    pub(crate) fn new(journal: Arc<Journal>, executors: Executors, max_concurrent: usize) -> Self {
        let shared = Arc::new(Shared {
            journal,
            executors,
            draining: CancellationToken::new(),
        });
        let (ordered, mut queue) = mpsc::unbounded_channel::<Admitted>();
        tokio::spawn({
            let shared = shared.clone();
            async move {
                while let Some(admitted) = queue.recv().await {
                    shared.run(admitted).await;
                }
            }
        });
        Self {
            shared,
            pool: Arc::new(Semaphore::new(max_concurrent)),
            ordered,
        }
    }

    /// Takes a dispatched job, unless it is a duplicate or the agent stops.
    pub(crate) fn submit(&self, job: Job) {
        let received = Instant::now();
        if job.id.is_empty() {
            tracing::warn!("ignoring a job without id");
            return;
        }
        if self.shared.draining.is_cancelled() {
            tracing::info!(job_id = %job.id, "agent stopping: job not taken");
            return;
        }
        let cancel = match self.shared.journal.admit(&job.id) {
            Admission::New(cancel) => cancel,
            Admission::Running => {
                tracing::debug!(job_id = %job.id, "duplicate dispatch of a running job ignored");
                return;
            }
            Admission::Replayed => {
                tracing::debug!(job_id = %job.id, "duplicate dispatch: events replayed");
                return;
            }
        };
        tracing::info!(job_id = %job.id, "job accepted");

        let pooled = matches!(
            job.action.as_ref().and_then(|action| action.kind.as_ref()),
            Some(Kind::RunCommands(_))
        );
        let admitted = Admitted {
            job,
            cancel,
            received,
        };
        if !pooled {
            if let Err(rejected) = self.ordered.send(admitted) {
                // Only possible if the worker died; do not leave the job open.
                self.shared.journal.forget(&rejected.0.job.id);
            }
            return;
        }
        let shared = self.shared.clone();
        let pool = self.pool.clone();
        tokio::spawn(async move {
            let permit = tokio::select! {
                permit = pool.acquire_owned() => permit.ok(),
                () = shared.draining.cancelled() => None,
            };
            // Without permit `run` sees the drain and forgets the job.
            shared.run(admitted).await;
            drop(permit);
        });
    }

    /// Stops starting jobs; the running ones continue.
    pub(crate) fn drain(&self) {
        self.shared.draining.cancel();
    }
}
