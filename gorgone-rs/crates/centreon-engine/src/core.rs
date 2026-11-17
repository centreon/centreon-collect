//! The scheduler task: owns every host and service, plans checks, runs them
//! with bounded concurrency, applies results and emits BBDO events. Every
//! change goes through this one task, so nothing is locked.

use std::cmp::Reverse;
use std::collections::{BinaryHeap, HashMap, VecDeque};
use std::time::Duration;

use tokio::sync::{mpsc, oneshot, watch};
use tokio::time::Instant;
use tokio_util::sync::CancellationToken;

use crate::cfg::{self, CheckParams, HostConf, MonitoringConfig, ServiceConf};
use crate::client::{Notice, Outgoing, Target};
use crate::document::Parsed;
use crate::extcmd::{self, ExtCommand};
use crate::macros::{Context, HostView, ServiceView, StateView};
use crate::runner::{self, PluginRun};
use crate::state::{self, CheckState, CheckType, Next, Outcome, StateType, code};
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

    /// Instance, configuration and every state, as Engine sends them at start.
    fn emit_snapshot(&mut self) {
        let Some((poller_id, name)) = self.poller() else {
            return;
        };
        let now = self.clock.now();
        let mut frames = vec![events::instance(poller_id, &name, true, self.start_time, 0)];
        for obj in &self.objs {
            frames.push(match &obj.conf {
                ObjConf::Host(host) => events::host(poller_id, host, &obj.state, now, true),
                ObjConf::Service { conf, .. } => {
                    events::service(poller_id, conf, &obj.state, now, true)
                }
            });
        }
        frames.push(events::instance_configuration(poller_id));
        for obj in &self.objs {
            frames.push(match &obj.conf {
                ObjConf::Host(host) => events::host_status(host, &obj.state),
                ObjConf::Service { conf, .. } => events::service_status(conf, &obj.state),
            });
        }
        frames.push(events::instance_status(poller_id, now));
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
            poller_id: self.loaded.as_ref().map_or(0, |l| l.target.poller_id),
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
        let same_target = self.loaded.as_ref().map(|l| &l.target) == Some(&target);
        let changes = self.rebuild(&config);
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
        if self.connected && same_target {
            self.emit_changes(&changes);
        }
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
    /// keeps every other object of the poller enabled.
    fn emit_changes(&mut self, changes: &Changes) {
        let Some((poller_id, _)) = self.poller() else {
            return;
        };
        let now = self.clock.now();
        let idle = CheckState::default();
        let mut frames = Vec::new();
        let removed_services = changes
            .removed
            .iter()
            .filter(|conf| matches!(conf, ObjConf::Service { .. }));
        let removed_hosts = changes
            .removed
            .iter()
            .filter(|conf| matches!(conf, ObjConf::Host(_)));
        for conf in removed_services.chain(removed_hosts) {
            frames.push(match conf {
                ObjConf::Host(host) => events::host(poller_id, host, &idle, now, false),
                ObjConf::Service { conf, .. } => {
                    events::service(poller_id, conf, &idle, now, false)
                }
            });
        }
        let mut updated: Vec<usize> = changes
            .added
            .iter()
            .chain(&changes.changed)
            .filter_map(|key| self.by_key.get(key).copied())
            .collect();
        // Hosts first: a service of an unknown host is ignored by the central.
        updated.sort_by_key(|index| (!self.objs[*index].is_host(), *index));
        for index in updated {
            let obj = &self.objs[index];
            frames.push(match &obj.conf {
                ObjConf::Host(host) => events::host(poller_id, host, &obj.state, now, true),
                ObjConf::Service { conf, .. } => {
                    events::service(poller_id, conf, &obj.state, now, true)
                }
            });
        }
        frames.push(events::instance_configuration(poller_id));
        for frame in frames {
            self.emit(frame);
        }
    }

    /// Replaces the objects, keeping the state and the plan of those that
    /// stay, and tells what changed.
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
                    fresh.push(objs.len());
                    Obj {
                        key,
                        conf,
                        state: CheckState::default(),
                        running: false,
                        seq: 0,
                        due: None,
                        rerun: false,
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
        let interval_length = u64::from(config.settings.interval_length.max(1));
        let now = Instant::now();
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
        // New objects: spread over min(interval, max spread), hosts and
        // services apart, as Engine's inter-check delay.
        let (hosts, services): (Vec<usize>, Vec<usize>) = fresh
            .into_iter()
            .partition(|index| self.objs[*index].is_host());
        for (list, max_spread) in [
            (hosts, config.settings.max_host_check_spread),
            (services, config.settings.max_service_check_spread),
        ] {
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
            for (position, index) in schedulable.into_iter().enumerate() {
                let interval = u64::from(self.objs[index].check().check_interval) * interval_length;
                let window_ms = interval.min(u64::from(max_spread) * 60) * 1000;
                let offset = Duration::from_millis(window_ms * position as u64 / count);
                self.schedule(index, now + offset);
            }
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
            self.start_check(index, planned);
        }
    }

    fn state_view(st: &CheckState, host: bool) -> StateView {
        let name = if host {
            match st.state {
                code::OK => "UP",
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

    /// Processed command line of object `index`.
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
        );
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
            match check_type {
                CheckType::Active => state::host_state_of(raw_code),
                // Passive host results are host states: UP, DOWN, UNREACHABLE.
                CheckType::Passive => u32::from(raw_code != code::OK),
            }
        } else {
            raw_code
        };
        let parsed = output::parse(&raw_output);
        let now = self.clock.now();
        let host_up = match &self.objs[index].conf {
            ObjConf::Service { host, .. } => self.objs[*host].state.state == code::OK,
            ObjConf::Host(_) => true,
        };
        let check = self.objs[index].check().clone();
        let obj = &mut self.objs[index];
        let outcome = Outcome {
            state,
            check_type,
            now,
        };
        let next = if is_host {
            obj.state.apply_host(&outcome, check.max_check_attempts)
        } else {
            obj.state
                .apply_service(&outcome, check.max_check_attempts, host_up)
        };
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
            _ if rerun => self.schedule(index, base),
            Next::Unchanged => {}
            Next::Normal | Next::Retry if !schedulable || check.check_interval == 0 => {
                self.unschedule(index);
            }
            Next::Normal => self.schedule(
                index,
                base + Duration::from_secs(u64::from(check.check_interval) * interval_length),
            ),
            Next::Retry => self.schedule(
                index,
                base + Duration::from_secs(
                    u64::from(check.retry_interval.max(1)) * interval_length,
                ),
            ),
        }
        let obj = &self.objs[index];
        let frame = match &obj.conf {
            ObjConf::Host(host) => events::host_status(host, &obj.state),
            ObjConf::Service { conf, .. } => events::service_status(conf, &obj.state),
        };
        tracing::debug!(
            host_id = obj.key.0,
            service_id = obj.key.1,
            state = obj.state.state,
            hard = obj.state.state_type == StateType::Hard,
            attempt = obj.state.attempt,
            "check result applied"
        );
        self.emit(frame);
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
        let command = extcmd::parse(line).map_err(|error| error.to_string())?;
        let find_service = |core: &Self, host: &str, service: &str| {
            core.services_by_name
                .get(&(host.to_owned(), service.to_owned()))
                .copied()
        };
        let find_host = |core: &Self, host: &str| core.hosts_by_name.get(host).copied();
        // Engine accepts a command on an object it does not know and only
        // logs it: so does the scheduler.
        let ignored = |what: String| {
            tracing::warn!(command = line, "{what}: command ignored");
            Ok(format!("{what}: command ignored"))
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
        self.schedule(index, at);
        self.launch();
        Ok(format!("check scheduled at {time}"))
    }

    fn passive(&mut self, index: usize, code: u32, output: String) -> Result<String, String> {
        if !self.objs[index].check().passive_checks {
            return Err("passive checks are disabled".to_owned());
        }
        self.apply(index, Err((code, output)), CheckType::Passive);
        Ok("result processed".to_owned())
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
            let wake = match next_due {
                Some(due) if self.in_flight < self.options.max_concurrent_checks => {
                    due.min(status_due)
                }
                _ => status_due,
            };
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

    /// End of the poller: `pb_instance(running false)`, then `pb_stop`.
    async fn shutdown(&mut self) {
        if let Some((poller_id, name)) = self.poller() {
            let now = self.clock.now();
            self.emit(events::instance(
                poller_id,
                &name,
                false,
                self.start_time,
                now,
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

    fn core(
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
            .command("ACKNOWLEDGE_SVC_PROBLEM;h;s1;1;1;1;me;x")
            .unwrap_err();
        assert!(refused.contains("not supported"), "{refused}");
        let unknown = core.command("SCHEDULE_SVC_CHECK;h;nope;1").unwrap();
        assert!(unknown.contains("unknown service"), "{unknown}");
    }
}
