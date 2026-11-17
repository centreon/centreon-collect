//! The scheduler task: owns every host and service, plans checks, runs them
//! with bounded concurrency, applies results and emits BBDO events. Every
//! change goes through this one task, so nothing is locked. Downtimes,
//! comments and acknowledgements are in `core/downtimes.rs`, the retention
//! file in `core/retention.rs`.

mod downtimes;
mod relations;
mod retention;

use std::cmp::Reverse;
use std::collections::{BTreeMap, BinaryHeap, HashMap, VecDeque};
use std::path::PathBuf;
use std::time::Duration;

use tokio::sync::{mpsc, oneshot, watch};
use tokio::time::Instant;
use tokio_util::sync::CancellationToken;

use crate::book::Book;
use crate::cfg::{self, CheckParams, HostConf, MonitoringConfig, ServiceConf};
use crate::client::{Notice, Outgoing, Target};
use crate::crypto::Credentials;
use crate::document::Parsed;
use crate::extcmd::{self, ExtCommand};
use crate::macros::{Context, HostView, ServiceView, StateView};
use crate::recurring::Rule;
use crate::runner::{self, PluginRun};
use crate::state::{self, AckType, CheckState, CheckType, Next, Outcome, StateType, code};
use crate::timeperiod::Calc;
use crate::tz::Zones;
use crate::{Input, Status, broker_conf, cmdline, events, output};

/// `(host_id, service_id)`; `service_id` 0 for a host.
type Key = (u64, u64);

/// Settings of the engine that do not come from its configuration.
#[derive(Debug, Clone)]
pub(crate) struct CoreOptions {
    pub input: Input,
    pub broker_address: Option<String>,
    pub max_concurrent_checks: usize,
    pub max_output_bytes: usize,
    /// Where states, downtimes, comments and acknowledgements survive a
    /// restart; none kept when unset.
    pub retention_file: Option<PathBuf>,
    /// `engine-context.json`: keys of the `encrypt::` values.
    pub credentials_file: Option<PathBuf>,
}

/// What putting a configuration in force did.
#[derive(Debug, Clone)]
pub(crate) struct Installed {
    /// One line for the log and the job.
    pub text: String,
    /// Commands, hosts, services.
    pub counts: (usize, usize, usize),
}

/// Requests to the engine task.
pub(crate) enum Request {
    /// Read the files again (files input).
    Reload(oneshot::Sender<Result<String, String>>),
    /// Put a checked document in force, with the digest it was applied under.
    Install(Box<(Parsed, String)>, oneshot::Sender<Installed>),
    Command(String, oneshot::Sender<Result<String, String>>),
    Shutdown(oneshot::Sender<()>),
}

/// Objects a new configuration adds, changes and removes, by key.
#[derive(Debug, Default)]
struct Changes {
    added: Vec<Key>,
    changed: Vec<Key>,
    /// Configurations of the removed objects, for their last event.
    removed: Vec<ObjConf>,
}

/// Wall-clock seconds derived from the runtime clock, so that a test with a
/// paused clock sees consistent times.
#[derive(Clone, Copy)]
struct Clock {
    base_instant: Instant,
    base_epoch: f64,
}

impl Clock {
    fn new() -> Self {
        let base_epoch = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map_or(0.0, |elapsed| elapsed.as_secs_f64());
        Self {
            base_instant: Instant::now(),
            base_epoch,
        }
    }

    fn epoch_of(self, instant: Instant) -> i64 {
        let offset = if instant >= self.base_instant {
            instant.duration_since(self.base_instant).as_secs_f64()
        } else {
            -self.base_instant.duration_since(instant).as_secs_f64()
        };
        // Whole seconds, as Engine's time_t.
        (self.base_epoch + offset) as i64
    }

    fn now(self) -> i64 {
        self.epoch_of(Instant::now())
    }

    fn instant_of(self, epoch: i64) -> Instant {
        let now = Instant::now();
        let delta = epoch - self.epoch_of(now);
        if delta <= 0 {
            now
        } else {
            now + Duration::from_secs(delta.unsigned_abs())
        }
    }
}

#[derive(Debug)]
enum ObjConf {
    Host(HostConf),
    Service { conf: ServiceConf, host: usize },
}

impl ObjConf {
    /// Same configuration, wherever the host of a service sits.
    fn same(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::Host(a), Self::Host(b)) => a == b,
            (Self::Service { conf: a, .. }, Self::Service { conf: b, .. }) => a == b,
            _ => false,
        }
    }
}

struct Obj {
    key: Key,
    conf: ObjConf,
    state: CheckState,
    running: bool,
    /// Sequence of the valid heap entry; older entries are ignored.
    seq: u64,
    /// When the planned check is due.
    due: Option<Instant>,
    /// A forced check asked while one was running: run again at once.
    rerun: bool,
    /// The planned check is forced: it ignores the check period.
    forced: bool,
    /// Flexible downtimes waiting for a problem (`pending_flex_downtime`).
    pending_flex: u32,
}

impl Obj {
    fn check(&self) -> &CheckParams {
        match &self.conf {
            ObjConf::Host(host) => &host.check,
            ObjConf::Service { conf, .. } => &conf.check,
        }
    }

    fn is_host(&self) -> bool {
        matches!(self.conf, ObjConf::Host(_))
    }

    /// `timezone` of the object (a service has its host's).
    fn timezone(&self) -> &str {
        match &self.conf {
            ObjConf::Host(host) => &host.timezone,
            ObjConf::Service { conf, .. } => &conf.timezone,
        }
    }

    /// OK or UP.
    fn is_ok(&self) -> bool {
        self.state.state == code::OK
    }
}

/// A finished plugin, back to the scheduler.
struct Done {
    key: Key,
    run: PluginRun,
    latency: f64,
}

/// The configuration in force.
struct Loaded {
    config: MonitoringConfig,
    target: Target,
    /// Digest of the structured document, `None` for files.
    digest: Option<String>,
}

/// Timed events (`timed_event`), by epoch second.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
enum Timer {
    /// `EVENT_SCHEDULED_DOWNTIME`: handle downtime `id` (start or stop).
    Downtime(u64),
    /// `EVENT_EXPIRE_DOWNTIME`.
    ExpireDowntimes,
    /// `EVENT_RECURRING_DOWNTIME`, every 60 s.
    Recurring,
    /// `EVENT_RETENTION_SAVE`.
    RetentionSave,
    /// `EVENT_EXPIRE_HOST_ACK`, `EVENT_EXPIRE_SERVICE_ACK`.
    AcknowledgementExpiry(Key),
}

/// Recurring downtimes run every minute (`EVENT_RECURRING_DOWNTIME`).
const RECURRING_PERIOD: i64 = 60;

pub(crate) struct Core {
    options: CoreOptions,
    clock: Clock,
    loaded: Option<Loaded>,
    objs: Vec<Obj>,
    by_key: HashMap<Key, usize>,
    hosts_by_name: HashMap<String, usize>,
    services_by_name: HashMap<(String, String), usize>,
    heap: BinaryHeap<Reverse<(Instant, u64, usize)>>,
    ready: VecDeque<(usize, u64, Instant)>,
    seq: u64,
    in_flight: usize,
    done_tx: mpsc::UnboundedSender<Done>,
    done_rx: mpsc::UnboundedReceiver<Done>,
    out: mpsc::Sender<Outgoing>,
    target: watch::Sender<Option<Target>>,
    status: watch::Sender<Status>,
    connected: bool,
    /// Events are emitted once a first connection happened; before, the
    /// snapshot sent at connection carries the state.
    ever_connected: bool,
    start_time: i64,
    dropped: u64,
    zones: Zones,
    book: Book,
    timers: BinaryHeap<Reverse<(i64, u64, Timer)>>,
    timer_seq: u64,
    /// Recurring downtime rules in force, by id.
    rules: BTreeMap<u64, Rule>,
    /// A configuration was put in force once: rules created later come
    /// from a reload (`recurring_downtime::set_initial_load_done`).
    initial_load_done: bool,
    credentials: Option<Credentials>,
    /// What the retention file kept, until the first configuration uses it.
    retained: Option<retention::Retained>,
    /// Parents, children and dependencies of the objects.
    links: relations::Links,
    /// Event log entries made before the first connection.
    early_logs: Vec<Vec<u8>>,
}

impl Core {
    pub(crate) fn new(
        options: CoreOptions,
        out: mpsc::Sender<Outgoing>,
        target: watch::Sender<Option<Target>>,
        status: watch::Sender<Status>,
    ) -> Self {
        let (done_tx, done_rx) = mpsc::unbounded_channel();
        let clock = Clock::new();
        let retained = options.retention_file.as_deref().and_then(retention::read);
        Self {
            options,
            clock,
            loaded: None,
            objs: Vec::new(),
            by_key: HashMap::new(),
            hosts_by_name: HashMap::new(),
            services_by_name: HashMap::new(),
            heap: BinaryHeap::new(),
            ready: VecDeque::new(),
            seq: 0,
            in_flight: 0,
            done_tx,
            done_rx,
            out,
            target,
            status,
            connected: false,
            ever_connected: false,
            start_time: clock.now(),
            dropped: 0,
            zones: Zones::system(),
            book: Book::default(),
            timers: BinaryHeap::new(),
            timer_seq: 0,
            rules: BTreeMap::new(),
            initial_load_done: false,
            credentials: None,
            retained,
            links: relations::Links::default(),
            early_logs: Vec::new(),
        }
    }

    fn emit(&mut self, frame: Vec<u8>) {
        if !self.ever_connected {
            return;
        }
        if self.out.try_send(Outgoing::Event(frame)).is_err() {
            self.dropped += 1;
            if self.dropped.is_power_of_two() {
                tracing::warn!(dropped = self.dropped, "BBDO channel full: events dropped");
            }
        }
    }

    fn poller(&self) -> Option<(u64, String)> {
        self.loaded
            .as_ref()
            .map(|loaded| (loaded.target.poller_id, loaded.target.poller_name.clone()))
    }

    fn poller_id(&self) -> u64 {
        self.loaded.as_ref().map_or(0, |l| l.target.poller_id)
    }

    fn add_timer(&mut self, at: i64, timer: Timer) {
        self.timer_seq += 1;
        self.timers.push(Reverse((at, self.timer_seq, timer)));
    }

    /// Instance, configuration and every state, as Engine sends them at
    /// start (`send_initial_configuration`, `engine/src/broker.cc:4135`).
    /// Downtimes and comments are sent with their current state: every
    /// connection gets this dump, and its `pb_instance` makes the central
    /// cancel the open downtimes and drop the comments of the poller
    /// (`_clean_tables`), which the upserts restore.
    fn emit_snapshot(&mut self) {
        let Some((poller_id, name)) = self.poller() else {
            return;
        };
        let now = self.clock.now();
        let encryption = self.credentials.is_some();
        let mut frames = vec![events::instance(
            poller_id,
            &name,
            true,
            (self.start_time, 0),
            encryption,
        )];
        let (before, after) = self
            .loaded
            .as_ref()
            .map(|l| relations::snapshot_frames(poller_id, &l.config))
            .unwrap_or_default();
        frames.extend(before);
        for obj in &self.objs {
            frames.push(match &obj.conf {
                ObjConf::Host(host) => events::host(poller_id, host, &obj.state, now, true),
                ObjConf::Service { conf, .. } => {
                    events::service(poller_id, conf, &obj.state, now, true)
                }
            });
        }
        for downtime in self.book.downtimes.values() {
            frames.push(events::downtime(poller_id, downtime, None));
        }
        for comment in self.book.comments.values() {
            frames.push(events::comment(poller_id, comment, None));
        }
        frames.extend(after);
        frames.push(events::instance_configuration(poller_id));
        for obj in &self.objs {
            frames.push(match &obj.conf {
                ObjConf::Host(host) => events::host_status(host, &obj.state),
                ObjConf::Service { conf, .. } => events::service_status(conf, &obj.state),
            });
        }
        frames.push(events::instance_status(poller_id, now));
        // The event log made before the central was reached.
        frames.append(&mut self.early_logs);
        for frame in frames {
            self.emit(frame);
        }
    }

    fn publish_status(&self) {
        let (hosts, services) = self.objs.iter().fold((0, 0), |(h, s), obj| {
            if obj.is_host() {
                (h + 1, s)
            } else {
                (h, s + 1)
            }
        });
        self.status.send_replace(Status {
            loaded: self.loaded.is_some(),
            poller_id: self.poller_id(),
            hosts,
            services,
            digest: self.loaded.as_ref().and_then(|l| l.digest.clone()),
        });
    }

    fn schedule(&mut self, index: usize, at: Instant) {
        self.seq += 1;
        let obj = &mut self.objs[index];
        obj.seq = self.seq;
        obj.due = Some(at);
        obj.state.next_check = self.clock.epoch_of(at);
        self.heap.push(Reverse((at, self.seq, index)));
    }

    fn unschedule(&mut self, index: usize) {
        let obj = &mut self.objs[index];
        self.seq += 1;
        obj.seq = self.seq;
        obj.due = None;
        obj.state.next_check = 0;
    }

    fn interval_length(&self) -> u64 {
        self.loaded
            .as_ref()
            .map_or(60, |l| u64::from(l.config.settings.interval_length.max(1)))
    }

    /// Calculator of the timeperiods of object `index`, in its zone (its
    /// `timezone`, else `use_timezone`, else the zone of the process), and
    /// its check period.
    fn in_zone_of<T>(
        &mut self,
        index: usize,
        work: impl FnOnce(&Calc<'_>, Option<&crate::timeperiod::Timeperiod>) -> T,
    ) -> T {
        let obj = &self.objs[index];
        let loaded = self.loaded.as_ref();
        let zone = match obj.timezone() {
            "" => loaded.map_or("", |l| l.config.settings.use_timezone.as_str()),
            zone => zone,
        }
        .to_owned();
        let tz = self.zones.get(&zone);
        let empty = HashMap::new();
        let periods = loaded.map_or(&empty, |l| &l.config.timeperiods);
        let period = periods.get(&obj.check().check_period);
        work(&Calc { tz: &tz, periods }, period)
    }

    /// `get_next_valid_time` of the check period of object `index`.
    fn next_valid_time(&mut self, index: usize, preferred: i64) -> i64 {
        let now = self.clock.now();
        self.in_zone_of(index, |calc, period| {
            calc.next_valid(preferred, now, period)
        })
    }

    /// `check_time_against_period` of the check period of object `index`.
    fn in_check_period(&mut self, index: usize, time: i64) -> bool {
        self.in_zone_of(index, |calc, period| calc.contains(time, period))
    }

    /// Plans the check of `index` at `at` or at the next valid time of its
    /// check period after it.
    fn schedule_valid(&mut self, index: usize, at: Instant) {
        if self.objs[index].check().check_period.is_empty() {
            return self.schedule(index, at);
        }
        let preferred = self.clock.epoch_of(at);
        let valid = self.next_valid_time(index, preferred);
        let at = if valid > preferred {
            self.clock.instant_of(valid)
        } else {
            at
        };
        self.schedule(index, at);
    }

    /// Reads the keys of the `encrypt::` values, as each Engine apply does
    /// (`applier/state.cc`): an absent file keeps the keys in force, an
    /// invalid one is logged (never its content) and changes nothing.
    fn load_credentials(&mut self) {
        let Some(path) = self.options.credentials_file.clone() else {
            return;
        };
        match crate::crypto::Credentials::from_file(&path) {
            Ok(Some(keys)) => self.credentials = Some(keys),
            Ok(None) => {}
            Err(error) => tracing::error!("cannot read the credentials keys: {error}"),
        }
    }

    /// Reads the Engine files and puts them in force; the running
    /// configuration stays when they cannot be read.
    fn load(&mut self) -> Result<String, String> {
        let Input::Files {
            main_file,
            broker_config,
        } = &self.options.input
        else {
            return Err("this engine takes structured configurations: nothing to read".to_owned());
        };
        let (config, report) = cfg::read(main_file);
        let counts = (
            config.commands.len(),
            config.hosts.len(),
            config.services.len(),
        );
        if !report.errors.is_empty() {
            return Err(report.render(Some(counts)));
        }
        let broker_file = broker_config
            .clone()
            .or_else(|| config.settings.broker_module_cfg_file.clone())
            .ok_or_else(|| {
                "no broker module configuration: neither broker_module_cfg_file in the \
                 main file nor scheduler.broker_config"
                    .to_owned()
            })?;
        let broker = broker_conf::read(&broker_file)?;
        let address = self
            .options
            .broker_address
            .clone()
            .or(broker.address.clone())
            .ok_or_else(|| {
                format!(
                    "{}: no TCP output to the central broker, and no scheduler.broker_address",
                    broker_file.display()
                )
            })?;
        let target = Target {
            address,
            poller_id: broker.poller_id,
            poller_name: broker.poller_name,
            broker_name: broker.broker_name,
        };
        let installed = self.install(config, target, None);
        Ok(format!(
            "{}\n{}",
            installed.text,
            report.render(Some(counts))
        ))
    }

    /// Puts a checked document in force.
    fn install_document(&mut self, parsed: Parsed, digest: String) -> Installed {
        let target = Target {
            address: self
                .options
                .broker_address
                .clone()
                .unwrap_or(parsed.broker.address),
            poller_id: parsed.poller.id,
            poller_name: parsed.poller.name,
            broker_name: parsed.broker.name,
        };
        self.install(parsed.config, target, Some(digest))
    }

    /// Puts a configuration in force. On the same connection, the central
    /// gets what changed as Engine sends it on a reload; a new connection
    /// gets everything once welcomed.
    fn install(
        &mut self,
        config: MonitoringConfig,
        target: Target,
        digest: Option<String>,
    ) -> Installed {
        let counts = (
            config.commands.len(),
            config.hosts.len(),
            config.services.len(),
        );
        self.load_credentials();
        let same_target = self.loaded.as_ref().map(|l| &l.target) == Some(&target);
        let first = self.loaded.is_none();
        let changes = self.rebuild(&config);
        let relation_frames = self
            .loaded
            .as_ref()
            .filter(|_| self.connected && same_target)
            .map(|l| relations::change_frames(target.poller_id, &l.config, &config, &changes));
        let text = format!(
            "configuration loaded: {} commands, {} hosts, {} services, poller {} ({}); \
             {} added, {} changed, {} removed",
            counts.0,
            counts.1,
            counts.2,
            target.poller_id,
            target.poller_name,
            changes.added.len(),
            changes.changed.len(),
            changes.removed.len(),
        );
        let recurring = config.recurring.clone();
        let retention_interval = i64::from(config.settings.retention_update_interval) * 60;
        self.loaded = Some(Loaded {
            config,
            target: target.clone(),
            digest,
        });
        self.target.send_if_modified(|current| {
            let changed = current.as_ref() != Some(&target);
            *current = Some(target);
            changed
        });
        if let Some(relation_frames) = relation_frames {
            self.emit_changes(&changes, relation_frames);
        }
        self.log_initial_states(&changes.added);
        let now = self.clock.now();
        if first {
            // Downtimes, comments and acknowledgements of the retention
            // file, now that their objects exist.
            self.restore_book(now);
            self.add_timer(now, Timer::Recurring);
            if retention_interval > 0 {
                self.add_timer(now + retention_interval, Timer::RetentionSave);
            }
        }
        self.apply_rules(recurring, now);
        self.initial_load_done = true;
        self.publish_status();
        self.launch();
        Installed { text, counts }
    }

    /// What Engine sends on a reload (`engine/src/configuration/applier/`):
    /// `pb_service` then `pb_host` with `enabled` false for removed objects
    /// (`host.cc:428-436`, `service.cc:464-466`), `pb_host`/`pb_service` for
    /// added and changed ones (`host.cc:134`, `:387`, `service.cc:152`,
    /// `:407`), then `pb_instance_configuration` (`cbmod::reload`,
    /// `broker/neb/src/cbmod.cc:376-391`). No `pb_instance`: the central
    /// keeps every other object of the poller enabled. Severities, tags,
    /// groups and parents go in the order of `applier::state::_processing`
    /// (`state.cc:1477-1530`): severities and tags, hosts, host groups,
    /// services, service groups, then the resolution (parents, members).
    fn emit_changes(&mut self, changes: &Changes, mut relation: relations::RelationFrames) {
        let Some((poller_id, _)) = self.poller() else {
            return;
        };
        let now = self.clock.now();
        let idle = CheckState::default();
        let mut frames = relation.before_objects;
        frames.extend(relation.removed_services);
        for conf in &changes.removed {
            if let ObjConf::Service { conf, .. } = conf {
                frames.push(events::service(poller_id, conf, &idle, now, false));
            }
        }
        frames.extend(relation.removed_hosts);
        for conf in &changes.removed {
            if let ObjConf::Host(host) = conf {
                frames.push(events::host(poller_id, host, &idle, now, false));
            }
        }
        frames.extend(relation.changed_parents);
        let mut updated: Vec<usize> = changes
            .added
            .iter()
            .chain(&changes.changed)
            .filter_map(|key| self.by_key.get(key).copied())
            .collect();
        // Hosts first: a service of an unknown host is ignored by the central.
        updated.sort_by_key(|index| (!self.objs[*index].is_host(), *index));
        let mut groups_sent = false;
        for index in updated {
            let obj = &self.objs[index];
            if !obj.is_host() && !groups_sent {
                frames.append(&mut relation.hostgroups);
                groups_sent = true;
            }
            frames.push(match &obj.conf {
                ObjConf::Host(host) => events::host(poller_id, host, &obj.state, now, true),
                ObjConf::Service { conf, .. } => {
                    events::service(poller_id, conf, &obj.state, now, true)
                }
            });
        }
        if !groups_sent {
            frames.extend(relation.hostgroups);
        }
        frames.extend(relation.servicegroups);
        frames.extend(relation.resolve);
        frames.push(events::instance_configuration(poller_id));
        for frame in frames {
            self.emit(frame);
        }
    }

    /// Replaces the objects, keeping the state and the plan of those that
    /// stay, and tells what changed. New objects take their retained state
    /// when the retention file has one.
    fn rebuild(&mut self, config: &MonitoringConfig) -> Changes {
        // Due but not started yet: they keep their time.
        for (index, seq, planned) in self.ready.drain(..) {
            if let Some(obj) = self.objs.get_mut(index).filter(|obj| obj.seq == seq) {
                obj.due = Some(planned);
            }
        }
        let mut old: HashMap<Key, Obj> = self.objs.drain(..).map(|obj| (obj.key, obj)).collect();
        self.by_key.clear();
        self.hosts_by_name.clear();
        self.services_by_name.clear();
        self.heap.clear();
        self.ready.clear();
        let mut fresh = Vec::new();
        let mut changes = Changes::default();
        let retained_states = self
            .retained
            .as_mut()
            .map(|r| std::mem::take(&mut r.states));
        let mut retained_states = retained_states.unwrap_or_default();
        // Retained next checks still to come, used when Engine would.
        let mut retained_next: Vec<(usize, i64)> = Vec::new();
        let use_retained_next = config.settings.use_retained_scheduling_info;
        let now_epoch = self.clock.now();
        let mut make = |key: Key, conf: ObjConf, objs: &mut Vec<Obj>| {
            let obj = match old.remove(&key) {
                Some(mut previous) => {
                    if !previous.conf.same(&conf) {
                        changes.changed.push(key);
                    }
                    previous.conf = conf;
                    previous
                }
                None => {
                    changes.added.push(key);
                    let mut state = CheckState::default();
                    if let Some(mut kept) = retained_states.remove(&key) {
                        if use_retained_next && kept.next_check > now_epoch {
                            retained_next.push((objs.len(), kept.next_check));
                        }
                        kept.next_check = 0;
                        kept.downtime_depth = 0;
                        state = kept;
                    }
                    fresh.push(objs.len());
                    Obj {
                        key,
                        conf,
                        state,
                        running: false,
                        seq: 0,
                        due: None,
                        rerun: false,
                        forced: false,
                        pending_flex: 0,
                    }
                }
            };
            objs.push(obj);
        };
        let mut objs = Vec::with_capacity(config.hosts.len() + config.services.len());
        for host in &config.hosts {
            make((host.host_id, 0), ObjConf::Host(host.clone()), &mut objs);
        }
        let host_index: HashMap<u64, usize> = objs
            .iter()
            .enumerate()
            .map(|(index, obj)| (obj.key.0, index))
            .collect();
        for service in &config.services {
            let host = host_index[&service.host_id];
            make(
                (service.host_id, service.service_id),
                ObjConf::Service {
                    conf: service.clone(),
                    host,
                },
                &mut objs,
            );
        }
        self.objs = objs;
        let removed: Vec<Key> = old.keys().copied().collect();
        changes.removed = old.into_values().map(|obj| obj.conf).collect();
        for (index, obj) in self.objs.iter().enumerate() {
            self.by_key.insert(obj.key, index);
            match &obj.conf {
                ObjConf::Host(host) => {
                    self.hosts_by_name.insert(host.name.clone(), index);
                }
                ObjConf::Service { conf, .. } => {
                    self.services_by_name
                        .insert((conf.host_name.clone(), conf.description.clone()), index);
                }
            }
        }
        self.index_links(config);
        // The configuration is needed by the timeperiod computations below.
        let interval_length = u64::from(config.settings.interval_length.max(1));
        let now = Instant::now();
        // Downtimes and comments of removed objects go with them.
        self.forget_objects(&removed);
        // Kept objects: same plan, brought forward if the interval shrank.
        for index in 0..self.objs.len() {
            if fresh.contains(&index) {
                continue;
            }
            let check = self.objs[index].check().clone();
            let interval = Duration::from_secs(u64::from(check.check_interval) * interval_length);
            let schedulable =
                check.active_checks && check.check_interval > 0 && !check.check_command.is_empty();
            match self.objs[index].due {
                Some(due) if schedulable => self.schedule(index, due.min(now + interval)),
                None if schedulable && !self.objs[index].running => {
                    fresh.push(index);
                }
                _ if !schedulable => self.unschedule(index),
                _ => {}
            }
        }
        for (index, next_check) in retained_next {
            fresh.retain(|f| *f != index);
            let check = self.objs[index].check();
            if check.active_checks && check.check_interval > 0 && !check.check_command.is_empty() {
                let at = self.clock.instant_of(next_check);
                self.schedule(index, at);
            }
        }
        // New objects: spread over min(interval, max spread), hosts and
        // services apart, as Engine's inter-check delay, then moved into
        // their check period (`applier/scheduler.cc`).
        let (hosts, services): (Vec<usize>, Vec<usize>) = fresh
            .into_iter()
            .partition(|index| self.objs[*index].is_host());
        let pending: Vec<(usize, Instant)> = [
            (hosts, config.settings.max_host_check_spread),
            (services, config.settings.max_service_check_spread),
        ]
        .into_iter()
        .flat_map(|(list, max_spread)| {
            let schedulable: Vec<usize> = list
                .into_iter()
                .filter(|index| {
                    let check = self.objs[*index].check();
                    check.active_checks
                        && check.check_interval > 0
                        && !check.check_command.is_empty()
                })
                .collect();
            let count = schedulable.len().max(1) as u64;
            schedulable
                .into_iter()
                .enumerate()
                .map(|(position, index)| {
                    let interval =
                        u64::from(self.objs[index].check().check_interval) * interval_length;
                    let window_ms = interval.min(u64::from(max_spread) * 60) * 1000;
                    let offset = Duration::from_millis(window_ms * position as u64 / count);
                    (index, now + offset)
                })
                .collect::<Vec<_>>()
        })
        .collect();
        // The timeperiods of this configuration, before it is in force.
        let previous = self
            .loaded
            .as_mut()
            .map(|l| std::mem::replace(&mut l.config.timeperiods, config.timeperiods.clone()));
        let previous_zone = self.loaded.as_mut().map(|l| {
            std::mem::replace(
                &mut l.config.settings.use_timezone,
                config.settings.use_timezone.clone(),
            )
        });
        if self.loaded.is_none() {
            for (index, at) in pending {
                // No configuration in force yet: periods are checked when due.
                self.schedule(index, at);
            }
        } else {
            for (index, at) in pending {
                self.schedule_valid(index, at);
            }
        }
        if let (Some(loaded), Some(previous), Some(zone)) =
            (self.loaded.as_mut(), previous, previous_zone)
        {
            loaded.config.timeperiods = previous;
            loaded.config.settings.use_timezone = zone;
        }
        changes
    }

    /// Moves the due checks to the ready queue.
    fn collect_due(&mut self) {
        let now = Instant::now();
        while let Some(Reverse((due, seq, index))) = self.heap.peek().copied() {
            if due > now {
                break;
            }
            self.heap.pop();
            let Some(obj) = self.objs.get_mut(index) else {
                continue;
            };
            if obj.seq != seq {
                continue;
            }
            obj.due = None;
            self.ready.push_back((index, seq, due));
        }
    }

    /// Starts ready checks while there is room.
    fn launch(&mut self) {
        self.collect_due();
        while self.in_flight < self.options.max_concurrent_checks {
            let Some((index, seq, planned)) = self.ready.pop_front() else {
                return;
            };
            if self.objs.get(index).is_none_or(|obj| obj.seq != seq) {
                continue;
            }
            if self.objs[index].running {
                self.objs[index].rerun = true;
                continue;
            }
            if !std::mem::take(&mut self.objs[index].forced) && !self.viable(index) {
                continue;
            }
            self.start_check(index, planned);
        }
    }

    /// `verify_check_viability` (`service.cc:3119-3196`, rescheduling
    /// `service.cc:2471-2560`, the same for hosts): a check due outside its
    /// period, or whose execution dependencies fail, is not run. Outside
    /// the period it is planned at the next valid time after one check
    /// interval, or a week later when there is none; a failing dependency
    /// plans it one interval later (the retry interval while SOFT).
    fn viable(&mut self, index: usize) -> bool {
        let now = self.clock.now();
        let in_period = self.in_check_period(index, now);
        let authorized = self.authorized_by_dependencies(index, 0);
        if in_period && authorized {
            return true;
        }
        let check = self.objs[index].check().clone();
        let length = self.interval_length() as i64;
        let interval = if check.check_interval == 0 {
            300
        } else {
            i64::from(check.check_interval) * length
        };
        let mut preferred = now + interval;
        if !authorized {
            let st = &self.objs[index].state;
            let units = if st.state_type == StateType::Soft && st.state != code::OK {
                check.retry_interval
            } else {
                check.check_interval
            };
            preferred = now + i64::from(units) * length;
            if preferred <= now {
                preferred = now + interval;
            }
            tracing::debug!(
                host_id = self.objs[index].key.0,
                service_id = self.objs[index].key.1,
                "Execution dependencies for this object failed, so it will not be actively \
                 checked."
            );
        }
        let mut next = self.next_valid_time(index, preferred);
        if !in_period && !self.in_check_period(index, next) {
            next += 7 * 24 * 60 * 60;
            tracing::warn!(
                host_id = self.objs[index].key.0,
                service_id = self.objs[index].key.1,
                "check could not be rescheduled properly: scheduling it for next week"
            );
        }
        let at = self.clock.instant_of(next);
        self.schedule(index, at);
        tracing::debug!(
            host_id = self.objs[index].key.0,
            service_id = self.objs[index].key.1,
            next_check = next,
            "outside the check period: check not run"
        );
        self.emit_status(index);
        false
    }

    fn state_view(st: &CheckState, host: bool) -> StateView {
        let name = if host {
            match st.state {
                code::OK => "UP",
                code::UNREACHABLE => "UNREACHABLE",
                _ => "DOWN",
            }
        } else {
            match st.state {
                code::OK => "OK",
                code::WARNING => "WARNING",
                code::CRITICAL => "CRITICAL",
                _ => "UNKNOWN",
            }
        };
        StateView {
            state: name,
            state_id: st.state,
            state_type: if st.state_type == StateType::Hard {
                "HARD"
            } else {
                "SOFT"
            },
            attempt: st.attempt,
        }
    }

    /// Processed command line of object `index`. It may hold decrypted
    /// secrets: never logged.
    fn command_line(&self, index: usize) -> Result<Vec<String>, String> {
        let loaded = self.loaded.as_ref().ok_or("no configuration")?;
        let obj = &self.objs[index];
        let (host_obj, service) = match &obj.conf {
            ObjConf::Host(_) => (obj, None),
            ObjConf::Service { conf, host } => (&self.objs[*host], Some((conf, obj))),
        };
        let ObjConf::Host(host) = &host_obj.conf else {
            return Err("service without host".to_owned());
        };
        let check_command = &obj.check().check_command;
        let name = cfg::command_name(check_command);
        let line = loaded
            .config
            .commands
            .get(name)
            .ok_or_else(|| format!("unknown command '{name}'"))?;
        let secrets = if loaded.config.secret_prefixes {
            self.credentials.as_ref()
        } else {
            None
        };
        let mut ctx = Context::new(
            &loaded.config.users,
            HostView {
                name: &host.name,
                alias: &host.alias,
                address: &host.address,
                macros: Some(&host.macros),
                state: Self::state_view(&host_obj.state, true),
            },
            service.map(|(conf, obj)| ServiceView {
                description: &conf.description,
                macros: Some(&conf.macros),
                state: Self::state_view(&obj.state, false),
            }),
        )
        .with_secrets(secrets);
        let processed = ctx.check_command_line(check_command, line);
        cmdline::split(&processed).map_err(|error| format!("command '{name}': {error}"))
    }

    fn start_check(&mut self, index: usize, planned: Instant) {
        let key = self.objs[index].key;
        let argv = self.command_line(index);
        let timeout = self.loaded.as_ref().map_or(60, |l| {
            if self.objs[index].is_host() {
                l.config.settings.host_check_timeout
            } else {
                l.config.settings.service_check_timeout
            }
        });
        let latency = Instant::now()
            .saturating_duration_since(planned)
            .as_secs_f64();
        let argv = match argv {
            Ok(argv) => argv,
            Err(error) => {
                tracing::warn!(host_id = key.0, service_id = key.1, %error, "check not run");
                let run = PluginRun {
                    exit_code: Some(3),
                    stdout: format!("(Command error: {error})"),
                    ..PluginRun::default()
                };
                self.apply(index, Ok((run, latency)), CheckType::Active);
                return;
            }
        };
        self.objs[index].running = true;
        self.in_flight += 1;
        let done = self.done_tx.clone();
        let max_output = self.options.max_output_bytes;
        tracing::debug!(host_id = key.0, service_id = key.1, "check started");
        tokio::spawn(async move {
            let run = runner::run(
                &argv,
                Duration::from_secs(u64::from(timeout.max(1))),
                max_output,
            )
            .await;
            let _ = done.send(Done { key, run, latency });
        });
    }

    /// Status event of object `index`, then what `cbmod` does with an
    /// acknowledgement that ended (`forward_pb_*_status`,
    /// `engine/src/broker.cc:2672-2690`, `:4672-4690`): its deletion is
    /// sent unless the object recovered or a normal acknowledgement saw
    /// its state change.
    fn emit_status(&mut self, index: usize) {
        let obj = &self.objs[index];
        let frame = match &obj.conf {
            ObjConf::Host(host) => events::host_status(host, &obj.state),
            ObjConf::Service { conf, .. } => events::service_status(conf, &obj.state),
        };
        self.emit(frame);
        let obj = &self.objs[index];
        if obj.state.acknowledgement != AckType::None {
            return;
        }
        let Some(ack) = self.book.acknowledgements.remove(&obj.key) else {
            return;
        };
        let state = if obj.state.checked {
            obj.state.state
        } else {
            4
        };
        if !(state == code::OK || (!ack.sticky && state != ack.state)) {
            let now = self.clock.now();
            let frame = events::acknowledgement(self.poller_id(), &ack, Some(now));
            self.emit(frame);
        }
    }

    /// Applies an active run (`Ok`) or a passive result (`Err((code, output))`).
    fn apply(
        &mut self,
        index: usize,
        result: Result<(PluginRun, f64), (u32, String)>,
        check_type: CheckType,
    ) {
        let is_host = self.objs[index].is_host();
        let (raw_code, raw_output, execution_time, latency) = match result {
            Ok((run, latency)) => {
                let (code, text) = runner::result_of(&run);
                (code, text, run.duration.as_secs_f64(), latency)
            }
            Err((code, text)) => (code, text, 0.0, 0.0),
        };
        let state = if is_host {
            let up = match check_type {
                CheckType::Active => state::host_state_of(raw_code) == code::OK,
                // Passive host results are host states: UP, DOWN,
                // UNREACHABLE; a problem goes through the reachability too.
                CheckType::Passive => raw_code == code::OK,
            };
            if up {
                code::OK
            } else {
                self.reachability(index)
            }
        } else {
            raw_code
        };
        let mut parsed = output::parse(&raw_output);
        // `handle_async_check_result` (`service.cc:1328-1340`,
        // `host.cc:1449-1460`): a placeholder for an empty output, and no
        // `;` in it, the separator of the event log.
        if parsed.output.is_empty() {
            parsed.output = if is_host {
                "(No output returned from host check)"
            } else {
                "(No output returned from plugin)"
            }
            .to_owned();
        }
        parsed.output = parsed.output.replace(';', ":");
        if !is_host && check_type == CheckType::Passive {
            self.log_passive_service_check(index, state, &parsed.output);
        }
        let now = self.clock.now();
        let host_up = match &self.objs[index].conf {
            ObjConf::Service { host, .. } => self.objs[*host].state.state == code::OK,
            ObjConf::Host(_) => true,
        };
        let check = self.objs[index].check().clone();
        let obj = &mut self.objs[index];
        let before = (obj.state.state, obj.state.last_hard_state_change);
        let before_hard_state = obj.state.last_hard_state;
        let outcome = Outcome {
            state,
            check_type,
            now,
        };
        let (next, service_logged) = if is_host {
            (
                obj.state.apply_host(&outcome, check.max_check_attempts),
                false,
            )
        } else {
            obj.state
                .apply_service_logged(&outcome, check.max_check_attempts, host_up)
        };
        let state_change = obj.state.state != before.0;
        let hard_change = obj.state.last_hard_state_change != before.1;
        obj.state.output = parsed.output;
        obj.state.long_output = parsed.long_output;
        obj.state.perfdata = parsed.perfdata;
        obj.state.execution_time = execution_time;
        obj.state.latency = latency;
        let rerun = std::mem::take(&mut obj.rerun);
        let interval_length = self.interval_length();
        let schedulable = check.active_checks && !check.check_command.is_empty();
        let base = Instant::now();
        match next {
            _ if rerun => {
                self.objs[index].forced = true;
                self.schedule(index, base);
            }
            Next::Unchanged => {}
            Next::Normal | Next::Retry if !schedulable || check.check_interval == 0 => {
                self.unschedule(index);
            }
            Next::Normal => self.schedule_valid(
                index,
                base + Duration::from_secs(u64::from(check.check_interval) * interval_length),
            ),
            Next::Retry => self.schedule_valid(
                index,
                base + Duration::from_secs(
                    u64::from(check.retry_interval.max(1)) * interval_length,
                ),
            ),
        }
        self.acknowledgement_after_result(index, state_change, hard_change);
        let obj = &self.objs[index];
        tracing::debug!(
            host_id = obj.key.0,
            service_id = obj.key.1,
            state = obj.state.state,
            hard = obj.state.state_type == StateType::Hard,
            attempt = obj.state.attempt,
            "check result applied"
        );
        if is_host {
            self.log_host_alert(index, before.0, before_hard_state);
        } else if service_logged {
            self.log_service_alert(index);
        }
        self.emit_status(index);
        self.flexible_downtimes_after_result(index, state_change, hard_change);
        if is_host {
            self.propagate_host_checks(index, before.0 == code::OK);
        } else {
            self.propagate_service_checks(index, state_change);
        }
    }

    fn finished(&mut self, done: Done) {
        self.in_flight = self.in_flight.saturating_sub(1);
        let Some(&index) = self.by_key.get(&done.key) else {
            // Removed by a reload meanwhile.
            return;
        };
        self.objs[index].running = false;
        self.apply(index, Ok((done.run, done.latency)), CheckType::Active);
    }

    fn command(&mut self, line: &str) -> Result<String, String> {
        if self.loaded.is_none() {
            return Err("no configuration loaded yet".to_owned());
        }
        let (stamp, command) = extcmd::parse_stamped(line).map_err(|error| error.to_string())?;
        let entry_time = stamp.unwrap_or_else(|| self.clock.now());
        let (name, args) = extcmd::name_and_args(line);
        self.log_external_command(name, args);
        let find_service = |core: &Self, host: &str, service: &str| {
            core.services_by_name
                .get(&(host.to_owned(), service.to_owned()))
                .copied()
        };
        let find_host = |core: &Self, host: &str| core.hosts_by_name.get(host).copied();
        // Engine accepts a command on an object it does not know and only
        // logs it: so does the scheduler. The line is not logged: it may
        // carry a comment.
        let ignored = |what: String| {
            tracing::warn!("{what}: command ignored");
            Ok(format!("{what}: command ignored"))
        };
        let find = |core: &Self, host: &str, service: Option<&str>| match service {
            Some(service) => find_service(core, host, service)
                .ok_or_else(|| format!("unknown service '{service}' on host '{host}'")),
            None => find_host(core, host).ok_or_else(|| format!("unknown host '{host}'")),
        };
        match command {
            ExtCommand::ScheduleService {
                host,
                service,
                time,
                forced,
            } => {
                let Some(index) = find_service(self, &host, &service) else {
                    return ignored(format!("unknown service '{service}' on host '{host}'"));
                };
                self.schedule_check(index, time, forced)
            }
            ExtCommand::ScheduleHost { host, time, forced } => {
                let Some(index) = find_host(self, &host) else {
                    return ignored(format!("unknown host '{host}'"));
                };
                if self.objs[index].check().check_command.is_empty() {
                    return Err(format!("host '{host}' has no check command"));
                }
                self.schedule_check(index, time, forced)
            }
            ExtCommand::ServiceResult {
                host,
                service,
                code,
                output,
            } => {
                let Some(index) = find_service(self, &host, &service) else {
                    return ignored(format!("unknown service '{service}' on host '{host}'"));
                };
                self.passive(index, code, output)
            }
            ExtCommand::HostResult { host, code, output } => {
                let Some(index) = find_host(self, &host) else {
                    return ignored(format!("unknown host '{host}'"));
                };
                self.passive(index, code, output)
            }
            ExtCommand::ScheduleDowntime {
                target,
                start,
                end,
                fixed,
                triggered_by,
                duration,
                author,
                comment,
            } => {
                let keys: Vec<Key> = match &target {
                    extcmd::DowntimeTarget::Host(host) => match find(self, host, None) {
                        Ok(index) => vec![self.objs[index].key],
                        Err(what) => return ignored(what),
                    },
                    extcmd::DowntimeTarget::Service(host, service) => {
                        match find(self, host, Some(service)) {
                            Ok(index) => vec![self.objs[index].key],
                            Err(what) => return ignored(what),
                        }
                    }
                    extcmd::DowntimeTarget::HostServices(host) => match find(self, host, None) {
                        Ok(index) => {
                            let host_id = self.objs[index].key.0;
                            self.services_of(&[host_id])
                        }
                        Err(what) => return ignored(what),
                    },
                    extcmd::DowntimeTarget::HostGroup { name, services } => {
                        let Some(group) = self
                            .loaded
                            .as_ref()
                            .and_then(|l| l.config.hostgroups.get(name))
                        else {
                            return ignored(format!("unknown host group '{name}'"));
                        };
                        let hosts = group.members.clone();
                        if *services {
                            self.services_of(&hosts)
                        } else {
                            hosts.into_iter().map(|host| (host, 0)).collect()
                        }
                    }
                    extcmd::DowntimeTarget::ServiceGroup { name, services } => {
                        let Some(group) = self
                            .loaded
                            .as_ref()
                            .and_then(|l| l.config.servicegroups.get(name))
                        else {
                            return ignored(format!("unknown service group '{name}'"));
                        };
                        if *services {
                            group.members.clone()
                        } else {
                            // One downtime per host, as the `last_host` of
                            // `cmd_schedule_downtime` gives on sorted members.
                            let mut hosts: Vec<Key> =
                                group.members.iter().map(|(host, _)| (*host, 0)).collect();
                            hosts.dedup();
                            hosts
                        }
                    }
                };
                let request = downtimes::Request {
                    entry_time,
                    author,
                    comment,
                    start,
                    end,
                    fixed,
                    triggered_by,
                    duration,
                };
                let mut ids = Vec::new();
                for key in keys {
                    ids.push(self.schedule_downtime(key, &request)?);
                }
                Ok(format!("downtime scheduled: {ids:?}"))
            }
            ExtCommand::DeleteDowntime { id } => {
                if self.unschedule_downtime(id) {
                    Ok(format!("downtime {id} deleted"))
                } else {
                    ignored(format!("unknown downtime {id}"))
                }
            }
            ExtCommand::Acknowledge {
                host,
                service,
                sticky,
                notify,
                persistent,
                author,
                comment,
            } => {
                let index = match find(self, &host, service.as_deref()) {
                    Ok(index) => index,
                    Err(what) => return ignored(what),
                };
                Ok(self.acknowledge(index, &author, &comment, sticky, notify, persistent))
            }
            ExtCommand::RemoveAcknowledgement { host, service } => {
                let index = match find(self, &host, service.as_deref()) {
                    Ok(index) => index,
                    Err(what) => return ignored(what),
                };
                self.remove_acknowledgement(index);
                Ok("acknowledgement removed".to_owned())
            }
            ExtCommand::AddComment {
                host,
                service,
                persistent,
                author,
                comment,
            } => {
                let index = match find(self, &host, service.as_deref()) {
                    Ok(index) => index,
                    Err(what) => return ignored(what),
                };
                let key = self.objs[index].key;
                let id = self.add_comment(
                    key,
                    crate::book::EntryType::User,
                    crate::book::Source::External,
                    entry_time,
                    &author,
                    &comment,
                    persistent,
                );
                Ok(format!("comment {id} added"))
            }
            ExtCommand::DeleteComment { id } => {
                if self.delete_comment(id) {
                    Ok(format!("comment {id} deleted"))
                } else {
                    ignored(format!("unknown comment {id}"))
                }
            }
            ExtCommand::DeleteAllComments { host, service } => {
                let index = match find(self, &host, service.as_deref()) {
                    Ok(index) => index,
                    Err(what) => return ignored(what),
                };
                let key = self.objs[index].key;
                let count = self.delete_comments_of(key);
                Ok(format!("{count} comments deleted"))
            }
        }
    }

    fn schedule_check(&mut self, index: usize, time: i64, forced: bool) -> Result<String, String> {
        let at = self.clock.instant_of(time);
        let obj = &self.objs[index];
        if !forced {
            if !obj.check().active_checks {
                return Ok("active checks are disabled: nothing scheduled".to_owned());
            }
            if obj.due.is_some_and(|due| due <= at) {
                return Ok("a check is already planned earlier".to_owned());
            }
        }
        if obj.running {
            self.objs[index].rerun = true;
            return Ok("check running: it runs again once finished".to_owned());
        }
        self.objs[index].forced = forced;
        self.schedule(index, at);
        self.launch();
        Ok(format!("check scheduled at {time}"))
    }

    fn passive(&mut self, index: usize, code: u32, output: String) -> Result<String, String> {
        if !self.objs[index].check().passive_checks {
            return Err("passive checks are disabled".to_owned());
        }
        self.apply(index, Err((code, output)), CheckType::Passive);
        // On-demand checks the result asked for.
        self.launch();
        Ok("result processed".to_owned())
    }

    /// Keys of the services of the hosts `host_ids`, in configuration order.
    fn services_of(&self, host_ids: &[u64]) -> Vec<Key> {
        self.objs
            .iter()
            .filter(|obj| obj.key.1 != 0 && host_ids.contains(&obj.key.0))
            .map(|obj| obj.key)
            .collect()
    }

    /// Runs the timed events that are due.
    fn run_timers(&mut self) {
        let now = self.clock.now();
        while let Some(Reverse((at, _, timer))) = self.timers.peek().copied() {
            if at > now {
                break;
            }
            self.timers.pop();
            match timer {
                Timer::Downtime(id) => self.handle_downtime(id),
                Timer::AcknowledgementExpiry(key) => self.expire_acknowledgement(key),
                Timer::ExpireDowntimes => self.expire_downtimes(),
                Timer::Recurring => {
                    self.run_rules(now);
                    self.add_timer(now + RECURRING_PERIOD, Timer::Recurring);
                }
                Timer::RetentionSave => {
                    self.save_retention(false);
                    let minutes = self
                        .loaded
                        .as_ref()
                        .map_or(0, |l| l.config.settings.retention_update_interval);
                    if minutes > 0 {
                        self.add_timer(now + i64::from(minutes) * 60, Timer::RetentionSave);
                    }
                }
            }
        }
    }

    /// Runs until shut down or cancelled.
    pub(crate) async fn run(
        mut self,
        mut requests: mpsc::UnboundedReceiver<Request>,
        mut notices: mpsc::UnboundedReceiver<Notice>,
        cancel: CancellationToken,
    ) {
        // A structured input is loaded by the handle, off this task.
        if matches!(self.options.input, Input::Files { .. }) {
            match self.load() {
                Ok(summary) => tracing::info!("{}", crate::first_line(&summary)),
                Err(error) => tracing::warn!(
                    "the engine waits for a usable configuration: {}",
                    crate::first_error(&error)
                ),
            }
        }
        self.publish_status();
        let mut status_due = Instant::now() + self.status_period();
        loop {
            let next_due = self.heap.peek().map(|Reverse((due, _, _))| *due);
            let mut wake = match next_due {
                Some(due) if self.in_flight < self.options.max_concurrent_checks => {
                    due.min(status_due)
                }
                _ => status_due,
            };
            if let Some(Reverse((at, _, _))) = self.timers.peek() {
                wake = wake.min(self.clock.instant_of(*at));
            }
            tokio::select! {
                () = cancel.cancelled() => return,
                request = requests.recv() => match request {
                    Some(Request::Reload(reply)) => {
                        let result = self.load();
                        match &result {
                            Ok(summary) => tracing::info!("{}", crate::first_line(summary)),
                            Err(_) => tracing::warn!("reload refused: the configuration in force is kept"),
                        }
                        let _ = reply.send(result);
                    }
                    Some(Request::Install(install, reply)) => {
                        let (parsed, digest) = *install;
                        let installed = self.install_document(parsed, digest);
                        tracing::info!("{}", installed.text);
                        let _ = reply.send(installed);
                    }
                    Some(Request::Command(line, reply)) => {
                        let _ = reply.send(self.command(&line));
                    }
                    Some(Request::Shutdown(reply)) => {
                        self.shutdown().await;
                        let _ = reply.send(());
                        return;
                    }
                    None => return,
                },
                done = self.done_rx.recv() => {
                    if let Some(done) = done {
                        self.finished(done);
                        // Take every result already there before launching.
                        while let Ok(done) = self.done_rx.try_recv() {
                            self.finished(done);
                        }
                        self.launch();
                    }
                }
                notice = notices.recv() => match notice {
                    Some(Notice::Connected) => {
                        self.connected = true;
                        self.ever_connected = true;
                        self.emit_snapshot();
                    }
                    Some(Notice::Disconnected) => self.connected = false,
                    None => {}
                },
                () = tokio::time::sleep_until(wake) => {
                    if Instant::now() >= status_due {
                        if let Some((poller_id, _)) = self.poller() {
                            let now = self.clock.now();
                            self.emit(events::instance_status(poller_id, now));
                        }
                        status_due = Instant::now() + self.status_period();
                    }
                    self.run_timers();
                    self.launch();
                }
            }
        }
    }

    fn status_period(&self) -> Duration {
        let seconds = self
            .loaded
            .as_ref()
            .map_or(60, |l| l.config.settings.status_update_interval.max(1));
        Duration::from_secs(u64::from(seconds))
    }

    /// End of the poller: retention saved, `pb_instance(running false)`,
    /// then `pb_stop`.
    async fn shutdown(&mut self) {
        self.save_retention(true);
        if let Some((poller_id, name)) = self.poller() {
            let now = self.clock.now();
            let encryption = self.credentials.is_some();
            self.emit(events::instance(
                poller_id,
                &name,
                false,
                (self.start_time, now),
                encryption,
            ));
        }
        let (reply, done) = oneshot::channel();
        if self.out.send(Outgoing::Stop(reply)).await.is_ok() {
            let _ = tokio::time::timeout(Duration::from_secs(10), done).await;
        }
    }
}

#[cfg(test)]
mod tests {
    use std::path::{Path, PathBuf};

    use super::*;

    fn write_config(dir: &Path, service_interval: u32) -> PathBuf {
        let write = |name: &str, text: String| std::fs::write(dir.join(name), text).unwrap();
        write(
            "centengine.cfg",
            "cfg_file=objects.cfg\ninterval_length=60\nmax_service_check_spread=5\n".to_owned(),
        );
        let mut objects = String::from(
            "define command {\n command_name ok\n command_line /bin/true\n}\n\
             define host {\n host_name h\n _HOST_ID 1\n address 127.0.0.1\n}\n",
        );
        for id in 1..=4 {
            objects.push_str(&format!(
                "define service {{\n host_name h\n service_description s{id}\n \
                 check_command ok\n check_interval {service_interval}\n retry_interval 1\n \
                 max_check_attempts 2\n _SERVICE_ID {id}\n}}\n"
            ));
        }
        write("objects.cfg", objects);
        write(
            "module.json",
            r#"{"centreonBroker":{"poller_id":7,"poller_name":"p","broker_name":"p-module",
               "output":[{"type":"ipv4","host":"127.0.0.1","port":"1"}]}}"#
                .to_owned(),
        );
        dir.join("centengine.cfg")
    }

    pub(super) fn core(
        dir: &Path,
        main: PathBuf,
    ) -> (
        Core,
        mpsc::Receiver<Outgoing>,
        watch::Receiver<Option<Target>>,
    ) {
        let (out, out_rx) = mpsc::channel(1024);
        let (target, target_rx) = watch::channel(None);
        let (status, _) = watch::channel(Status::default());
        let options = CoreOptions {
            input: Input::Files {
                main_file: main,
                broker_config: Some(dir.join("module.json")),
            },
            broker_address: None,
            // No check is started by these tests.
            max_concurrent_checks: 0,
            max_output_bytes: 1024,
            retention_file: Some(dir.join("retention.json")),
            credentials_file: None,
        };
        (Core::new(options, out, target, status), out_rx, target_rx)
    }

    /// Delay of the planned check of service `id`, from now.
    fn due_in(core: &Core, id: u64) -> Option<Duration> {
        let index = core.by_key[&(1, id)];
        core.objs[index]
            .due
            .map(|due| due.saturating_duration_since(Instant::now()))
    }

    fn plan(core: &Core, ids: std::ops::RangeInclusive<u64>) -> Vec<u64> {
        ids.map(|id| due_in(core, id).unwrap().as_secs()).collect()
    }

    #[tokio::test(start_paused = true)]
    async fn first_checks_are_spread_and_reload_keeps_the_plan() {
        let dir = tempfile::tempdir().unwrap();
        let main = write_config(dir.path(), 5);
        let (mut core, _out, target) = core(dir.path(), main.clone());
        core.load().unwrap();
        assert_eq!(
            target
                .borrow()
                .as_ref()
                .map(|t| (t.poller_id, t.address.clone())),
            Some((7, "127.0.0.1:1".to_owned()))
        );
        // min(5 min interval, 5 min spread) over 4 services: 0, 75, 150 and
        // 225 s; the first one is due at once.
        assert_eq!(core.ready.len(), 1);
        assert_eq!(plan(&core, 2..=4), [75, 150, 225]);
        // No check command: the host is never scheduled.
        assert_eq!(core.objs[core.by_key[&(1, 0)]].due, None);

        // The first two are due: waiting for room to start, they stay due
        // across a reload.
        tokio::time::advance(Duration::from_secs(100)).await;
        core.load().unwrap();
        assert_eq!(core.ready.len(), 2);
        assert_eq!(plan(&core, 3..=4), [50, 125]);

        // A shorter interval brings the planned checks forward.
        write_config(dir.path(), 1);
        core.load().unwrap();
        assert_eq!(core.ready.len(), 2);
        assert_eq!(plan(&core, 3..=4), [50, 60]);

        // An invalid configuration is refused and the running one stays.
        std::fs::write(dir.path().join("objects.cfg"), "define host {\n").unwrap();
        let report = core.load().unwrap_err();
        assert!(report.contains("Total Errors:"), "{report}");
        assert_eq!(core.objs.len(), 5);
    }

    #[tokio::test(start_paused = true)]
    async fn passive_results_go_soft_then_hard_and_reschedule() {
        let dir = tempfile::tempdir().unwrap();
        let main = write_config(dir.path(), 5);
        let (mut core, _out, _target) = core(dir.path(), main);
        core.load().unwrap();
        let line = "[1] PROCESS_SERVICE_CHECK_RESULT;h;s1;2;CRITICAL - down|value=2\nmore";
        core.command(line).unwrap();
        let state = core.objs[core.by_key[&(1, 1)]].state.clone();
        assert_eq!(
            (state.state, state.state_type, state.attempt),
            (2, StateType::Soft, 1)
        );
        assert_eq!(state.output, "CRITICAL - down");
        assert_eq!(state.perfdata, "value=2");
        assert_eq!(state.long_output, "more");
        assert_eq!(state.check_type, CheckType::Passive);
        // A state change moves the plan to the retry interval.
        assert_eq!(due_in(&core, 1).unwrap().as_secs(), 60);
        core.command("PROCESS_SERVICE_CHECK_RESULT;h;s1;2;CRITICAL")
            .unwrap();
        let state = &core.objs[core.by_key[&(1, 1)]].state;
        assert_eq!((state.state_type, state.attempt), (StateType::Hard, 2));

        // Forced check: planned at the asked time.
        let at = core.clock.now() + 30;
        core.command(&format!("SCHEDULE_FORCED_SVC_CHECK;h;s2;{at}"))
            .unwrap();
        assert_eq!(due_in(&core, 2).unwrap().as_secs(), 30);
        // Not forced and later than the plan: nothing changes.
        let later = core.clock.now() + 1000;
        let answer = core
            .command(&format!("SCHEDULE_SVC_CHECK;h;s2;{later}"))
            .unwrap();
        assert!(answer.contains("earlier"), "{answer}");

        let refused = core
            .command("DEL_SVC_DOWNTIME_FULL;h;s1;;;;;;")
            .unwrap_err();
        assert!(refused.contains("not supported"), "{refused}");
        let unknown = core.command("SCHEDULE_SVC_CHECK;h;nope;1").unwrap();
        assert!(unknown.contains("unknown service"), "{unknown}");
    }
}
