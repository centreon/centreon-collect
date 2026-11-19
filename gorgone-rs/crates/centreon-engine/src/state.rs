//! State of a host or service and how a check result changes it: the
//! Nagios 3 rules of `service::handle_async_check_result`
//! (`engine/src/service.cc:1255-2060`); notifications, flapping and event
//! handlers are applied by the core on top of it. Pure: no clock, no I/O.

use std::collections::{BTreeMap, BTreeSet};

use serde::{Deserialize, Serialize};

/// SOFT or HARD.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum StateType {
    /// Not confirmed yet: retried at `retry_interval`.
    Soft,
    /// Confirmed.
    Hard,
}

/// Service states (`ServiceStatus.State`); hosts use 0 (UP), 1 (DOWN) and
/// 2 (UNREACHABLE).
pub mod code {
    /// OK, or UP.
    pub const OK: u32 = 0;
    /// WARNING.
    pub const WARNING: u32 = 1;
    /// CRITICAL, or DOWN for a host.
    pub const CRITICAL: u32 = 2;
    /// UNKNOWN.
    pub const UNKNOWN: u32 = 3;
    /// Host DOWN.
    pub const DOWN: u32 = 1;
    /// Host UNREACHABLE: DOWN with no parent UP.
    pub const UNREACHABLE: u32 = 2;
}

/// Whether a result came from a check the scheduler ran.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum CheckType {
    /// Run by the scheduler.
    Active,
    /// Submitted with `PROCESS_*_CHECK_RESULT`.
    Passive,
}

/// Acknowledgement of the current problem (`AckType`, `bbdo/neb.proto`).
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum AckType {
    /// Not acknowledged.
    #[default]
    None = 0,
    /// Removed by the next change of state.
    Normal = 1,
    /// Removed by the recovery only.
    Sticky = 2,
}

/// Everything a status event reports about one object, and what the
/// retention file keeps of it.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct CheckState {
    /// Current state code.
    pub state: u32,
    /// Current state type.
    pub state_type: StateType,
    /// Current attempt, from 1 to `max_check_attempts`.
    pub attempt: u32,
    /// At least one result was applied.
    pub checked: bool,
    /// Type of the last result.
    pub check_type: CheckType,
    /// Time of the last result, in seconds since the epoch.
    pub last_check: i64,
    /// Next planned check, in seconds since the epoch (0: none).
    pub next_check: i64,
    /// Last change of state, soft or hard.
    pub last_state_change: i64,
    /// Last hard state.
    pub last_hard_state: u32,
    /// Last change of hard state.
    pub last_hard_state_change: i64,
    /// Last time in each state (OK/UP, WARNING/DOWN, CRITICAL, UNKNOWN).
    pub last_time: [i64; 4],
    /// First line of the output.
    pub output: String,
    /// Other lines.
    pub long_output: String,
    /// Performance data.
    pub perfdata: String,
    /// Run time of the plugin, in seconds.
    pub execution_time: f64,
    /// Delay between the planned and the actual start, in seconds.
    pub latency: f64,
    /// The service's host was not UP at its last check (service only).
    host_problem_at_last_check: bool,
    /// Acknowledgement of the current problem.
    #[serde(default)]
    pub acknowledgement: AckType,
    /// Downtimes in effect on the object.
    #[serde(skip)]
    pub downtime_depth: i32,
    /// Notifications sent and their numbers.
    #[serde(default)]
    pub notification: NotificationState,
    /// State history and flapping.
    #[serde(default)]
    pub flap: FlapState,
    /// What the external commands changed (`modified_attributes`).
    #[serde(default)]
    pub modified: Modified,
    /// A freshness check is running (`is_being_freshened`).
    #[serde(skip)]
    pub being_freshened: bool,
}

/// Why a notification is sent (`notifier::reason_type`,
/// `engine/inc/com/centreon/engine/notifier.hh:87-97`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Reason {
    Normal,
    Recovery,
    Acknowledgement,
    FlappingStart,
    FlappingStop,
    FlappingDisabled,
    DowntimeStart,
    DowntimeEnd,
    DowntimeCancelled,
    Custom,
}

impl Reason {
    /// `notification_category` (`notifier.cc:945-953`): normal,
    /// recovery, acknowledgement, flapping, downtime, custom.
    #[must_use]
    pub fn category(self) -> u8 {
        match self {
            Self::Normal => 0,
            Self::Recovery => 1,
            Self::Acknowledgement => 2,
            Self::FlappingStart | Self::FlappingStop | Self::FlappingDisabled => 3,
            Self::DowntimeStart | Self::DowntimeEnd | Self::DowntimeCancelled => 4,
            Self::Custom => 5,
        }
    }

    /// `tab_notification_str` (`notifier.cc:37-47`).
    #[must_use]
    pub fn name(self) -> &'static str {
        match self {
            Self::Normal => "NORMAL",
            Self::Recovery => "RECOVERY",
            Self::Acknowledgement => "ACKNOWLEDGEMENT",
            Self::FlappingStart => "FLAPPINGSTART",
            Self::FlappingStop => "FLAPPINGSTOP",
            Self::FlappingDisabled => "FLAPPINGDISABLED",
            Self::DowntimeStart => "DOWNTIMESTART",
            Self::DowntimeEnd => "DOWNTIMEEND",
            Self::DowntimeCancelled => "DOWNTIMECANCELLED",
            Self::Custom => "CUSTOM",
        }
    }

    /// `$NOTIFICATIONTYPE$` (`notification.cc:100-131`): PROBLEM for a
    /// normal notification.
    #[must_use]
    pub fn macro_name(self) -> &'static str {
        match self {
            Self::Normal => "PROBLEM",
            other => other.name(),
        }
    }
}

/// A notification sent, kept per category (`notifier::_notification`):
/// the recovery goes to the contacts the problem went to, the interval of
/// the last one plans the next.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Sent {
    pub reason: Reason,
    pub number: u32,
    /// In units of `interval_length`.
    pub interval: f64,
    pub escalated: bool,
    pub contacts: BTreeSet<String>,
}

/// Notification state of a notifier (`notifier.cc:121-155`).
#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct NotificationState {
    /// `current_notification_number`.
    pub number: u32,
    /// Time of the last notification sent to someone (0: none since the
    /// last change of state).
    pub last: i64,
    /// `next_notification`: informational, as in Engine.
    pub next: i64,
    pub no_more: bool,
    /// `current_notification_id`.
    pub id: u64,
    /// By category ([`Reason::category`]).
    pub sent: BTreeMap<u8, Sent>,
}

/// Number of states kept (`MAX_STATE_HISTORY_ENTRIES`, `common.hh:37`).
pub const HISTORY: usize = 21;

/// State history and flapping (`notifier.cc:1223-1228`).
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct FlapState {
    /// The last [`HISTORY`] states recorded, a ring.
    pub history: Vec<u32>,
    pub index: usize,
    pub percent_state_change: f64,
    pub is_flapping: bool,
    /// The comment of the flapping, 0 for none.
    pub comment_id: u64,
    /// Host: last time a state was recorded by a service result.
    pub last_history_update: i64,
}

impl Default for FlapState {
    fn default() -> Self {
        Self {
            history: vec![0; HISTORY],
            index: 0,
            percent_state_change: 0.0,
            is_flapping: false,
            comment_id: 0,
            last_history_update: 0,
        }
    }
}

impl FlapState {
    /// Records `state` when `record`, then the weighted percent of state
    /// change of the history, oldest first, recent changes weighing more
    /// (`check_for_flapping`, `service.cc:2214-2252`).
    pub fn update(&mut self, state: u32, record: bool) -> f64 {
        if self.history.len() != HISTORY {
            self.history = vec![0; HISTORY];
            self.index = 0;
        }
        if record {
            self.history[self.index] = state;
            self.index = (self.index + 1) % HISTORY;
        }
        let mut curved_changes = 0.0;
        let mut y = self.index;
        let mut last = self.history[y];
        for x in 1..HISTORY {
            y = (y + 1) % HISTORY;
            if self.history[y] != last {
                curved_changes +=
                    ((x as f64 - 1.0) * (1.25 - 0.75)) / (HISTORY as f64 - 2.0) + 0.75;
            }
            last = self.history[y];
        }
        let percent = curved_changes * 100.0 / (HISTORY as f64 - 1.0);
        self.percent_state_change = percent;
        percent
    }
}

/// Settings changed by external commands, kept across reloads and in the
/// retention file (`modified_attributes`); `None`: the configuration's.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct Modified {
    pub notifications_enabled: Option<bool>,
    pub event_handler_enabled: Option<bool>,
    pub flap_detection_enabled: Option<bool>,
}

impl Default for CheckState {
    /// Engine's initial state: OK/UP, HARD, attempt 1, never checked.
    fn default() -> Self {
        Self {
            state: code::OK,
            state_type: StateType::Hard,
            attempt: 1,
            checked: false,
            check_type: CheckType::Active,
            last_check: 0,
            next_check: 0,
            last_state_change: 0,
            last_hard_state: code::OK,
            last_hard_state_change: 0,
            last_time: [0; 4],
            output: String::new(),
            long_output: String::new(),
            perfdata: String::new(),
            execution_time: 0.0,
            latency: 0.0,
            host_problem_at_last_check: false,
            acknowledgement: AckType::None,
            downtime_depth: 0,
            notification: NotificationState::default(),
            flap: FlapState::default(),
            modified: Modified::default(),
            being_freshened: false,
        }
    }
}

/// Which interval comes next after a result.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Next {
    /// `check_interval`.
    Normal,
    /// `retry_interval`.
    Retry,
    /// Do not move the planned check (passive result without change).
    Unchanged,
}

/// One result to apply.
#[derive(Debug, Clone)]
pub struct Outcome {
    /// State code of the result (already mapped to UP/DOWN for hosts).
    pub state: u32,
    /// Active or passive.
    pub check_type: CheckType,
    /// Time of the result.
    pub now: i64,
}

impl CheckState {
    fn touch_last_time(&mut self, now: i64) {
        if let Some(slot) = self.last_time.get_mut(self.state as usize) {
            *slot = now;
        }
    }

    fn init_changes(&mut self) {
        if self.last_state_change == 0 {
            self.last_state_change = self.last_check;
        }
        if self.last_hard_state_change == 0 {
            self.last_hard_state_change = self.last_check;
        }
    }

    /// Applies a service result; `host_up` is the current state of its
    /// host. Returns which interval plans the next check.
    #[cfg(test)]
    pub fn apply_service(&mut self, outcome: &Outcome, max_attempts: u32, host_up: bool) -> Next {
        self.apply_service_logged(outcome, max_attempts, host_up).0
    }

    /// [`Self::apply_service`], and whether Engine calls `log_event` for
    /// this result (`service::handle_async_check_result`,
    /// `engine/src/service.cc:1620-1980`): a hard change, a soft recovery,
    /// or a retry while the host is up. `log_event` itself drops the SOFT
    /// ones unless `log_service_retries`.
    pub fn apply_service_logged(
        &mut self,
        outcome: &Outcome,
        max_attempts: u32,
        host_up: bool,
    ) -> (Next, bool) {
        let max_attempts = max_attempts.max(1);
        let last_state = self.state;
        self.state = outcome.state;
        self.check_type = outcome.check_type;
        self.last_check = outcome.now;
        self.checked = true;
        if last_state == code::OK && self.state != last_state {
            self.attempt = 1;
        } else if self.state_type == StateType::Soft && self.attempt < max_attempts {
            self.attempt += 1;
        }
        let state_change = self.state != last_state;
        let mut hard_change = (self.host_problem_at_last_check && self.state == code::OK)
            || (self.attempt >= max_attempts
                && (self.state != self.last_hard_state
                    || self.last_state_change > self.last_hard_state_change));
        self.init_changes();
        if state_change {
            self.last_state_change = outcome.now;
        }
        if hard_change {
            self.last_hard_state_change = outcome.now;
        }
        self.touch_last_time(outcome.now);
        // Engine reschedules an active result always, a passive one on a change.
        let moved = outcome.check_type == CheckType::Active || state_change || hard_change;
        let next = |interval| if moved { interval } else { Next::Unchanged };

        if self.state == code::OK {
            if hard_change {
                self.state_type = StateType::Hard;
                self.attempt = 1;
            } else if state_change {
                // Soft recovery: hard at the next OK result.
                self.state_type = StateType::Soft;
                self.attempt = max_attempts.saturating_sub(1).max(1);
            }
            self.host_problem_at_last_check = false;
            self.last_hard_state = code::OK;
            return (next(Next::Normal), hard_change || state_change);
        }

        if !host_up {
            // The host is down: the problem is hard at once.
            if self.last_hard_state != self.state {
                hard_change = true;
            }
            if state_change || hard_change {
                self.last_state_change = outcome.now;
            }
            if hard_change {
                self.last_hard_state_change = outcome.now;
                self.state_type = StateType::Hard;
                self.last_hard_state = self.state;
            }
            self.host_problem_at_last_check = true;
        } else if self.host_problem_at_last_check {
            self.host_problem_at_last_check = false;
            if self.state_type == StateType::Soft {
                self.attempt = 1;
            }
        }
        if self.attempt < max_attempts {
            if !host_up {
                return (next(Next::Normal), hard_change);
            }
            self.state_type = StateType::Soft;
            (next(Next::Retry), true)
        } else {
            self.state_type = StateType::Hard;
            self.last_hard_state = self.state;
            (next(Next::Normal), hard_change)
        }
    }

    /// Applies a host result (state already UP, DOWN or UNREACHABLE). A
    /// recovery is always hard; problems go SOFT then HARD after
    /// `max_attempts`, DOWN and UNREACHABLE being the same problem for the
    /// attempts (`host::process_check_result_3x`).
    pub fn apply_host(&mut self, outcome: &Outcome, max_attempts: u32) -> Next {
        let max_attempts = max_attempts.max(1);
        let last_state = self.state;
        self.state = outcome.state;
        self.check_type = outcome.check_type;
        self.last_check = outcome.now;
        self.checked = true;
        let state_change = self.state != last_state;
        self.init_changes();
        if state_change {
            self.last_state_change = outcome.now;
        }
        self.touch_last_time(outcome.now);
        let moved = outcome.check_type == CheckType::Active || state_change;
        let next = |interval| if moved { interval } else { Next::Unchanged };
        if self.state == code::OK {
            if self.last_hard_state != code::OK {
                self.last_hard_state_change = outcome.now;
            }
            self.state_type = StateType::Hard;
            self.attempt = 1;
            self.last_hard_state = code::OK;
            return next(Next::Normal);
        }
        if last_state == code::OK {
            self.attempt = 1;
        } else if self.state_type == StateType::Soft && self.attempt < max_attempts {
            self.attempt += 1;
        }
        if self.attempt >= max_attempts {
            if self.state_type != StateType::Hard || self.last_hard_state != self.state {
                self.last_hard_state_change = outcome.now;
            }
            self.state_type = StateType::Hard;
            self.last_hard_state = self.state;
            next(Next::Normal)
        } else {
            self.state_type = StateType::Soft;
            next(Next::Retry)
        }
    }
}

/// Host state of a plugin exit code: 0 and 1 are UP, the others DOWN
/// (`handle_async_check_result_3x`); DOWN becomes UNREACHABLE when no
/// parent is up, which the core decides.
#[must_use]
pub fn host_state_of(exit_code: u32) -> u32 {
    if exit_code <= code::WARNING {
        code::OK
    } else {
        code::DOWN
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn active(state: u32, now: i64) -> Outcome {
        Outcome {
            state,
            check_type: CheckType::Active,
            now,
        }
    }

    /// (state, type, attempt, next)
    fn step(s: &mut CheckState, state: u32, now: i64) -> (u32, StateType, u32, Next) {
        let next = s.apply_service(&active(state, now), 3, true);
        (s.state, s.state_type, s.attempt, next)
    }

    #[test]
    fn service_goes_soft_then_hard_and_recovers_hard() {
        use StateType::{Hard, Soft};
        let mut s = CheckState::default();
        assert_eq!(step(&mut s, 0, 100), (0, Hard, 1, Next::Normal));
        assert_eq!(s.last_state_change, 100);
        assert_eq!(step(&mut s, 2, 200), (2, Soft, 1, Next::Retry));
        assert_eq!(s.last_state_change, 200);
        assert_eq!(s.last_hard_state, 0);
        assert_eq!(step(&mut s, 2, 260), (2, Soft, 2, Next::Retry));
        assert_eq!(step(&mut s, 2, 320), (2, Hard, 3, Next::Normal));
        assert_eq!((s.last_hard_state, s.last_hard_state_change), (2, 320));
        // Stays hard; a change between problems is a hard change too.
        assert_eq!(step(&mut s, 2, 620), (2, Hard, 3, Next::Normal));
        assert_eq!(s.last_hard_state_change, 320);
        assert_eq!(step(&mut s, 1, 920), (1, Hard, 3, Next::Normal));
        assert_eq!((s.last_hard_state, s.last_hard_state_change), (1, 920));
        // Recovery from a hard problem is hard.
        assert_eq!(step(&mut s, 0, 1220), (0, Hard, 1, Next::Normal));
        assert_eq!((s.last_hard_state, s.last_hard_state_change), (0, 1220));
        assert_eq!(s.last_time, [1220, 920, 620, 0]);
    }

    #[test]
    fn soft_recovery_becomes_hard_at_the_next_ok() {
        use StateType::{Hard, Soft};
        let mut s = CheckState::default();
        step(&mut s, 0, 100);
        assert_eq!(step(&mut s, 2, 200), (2, Soft, 1, Next::Retry));
        assert_eq!(step(&mut s, 0, 260), (0, Soft, 2, Next::Normal));
        assert_eq!(step(&mut s, 0, 560), (0, Hard, 1, Next::Normal));
        assert_eq!(s.last_hard_state_change, 560);
    }

    #[test]
    fn single_attempt_and_host_down_are_hard_at_once() {
        let mut s = CheckState::default();
        s.apply_service(&active(2, 10), 1, true);
        assert_eq!((s.state_type, s.last_hard_state), (StateType::Hard, 2));

        let mut s = CheckState::default();
        let next = s.apply_service(&active(2, 10), 3, false);
        assert_eq!((s.state_type, next), (StateType::Hard, Next::Normal));
        // Host back up, same problem: it is retried again, as in Engine.
        let mut s2 = s.clone();
        let next = s2.apply_service(&active(2, 20), 3, true);
        assert_eq!(
            (s2.state_type, s2.attempt, next),
            (StateType::Soft, 1, Next::Retry)
        );
    }

    #[test]
    fn passive_result_moves_the_schedule_only_on_a_change() {
        let mut s = CheckState::default();
        let passive = |state, now| Outcome {
            state,
            check_type: CheckType::Passive,
            now,
        };
        assert_eq!(s.apply_service(&passive(0, 5), 3, true), Next::Unchanged);
        assert_eq!(s.apply_service(&passive(2, 6), 1, true), Next::Normal);
        assert_eq!(s.check_type, CheckType::Passive);
    }

    #[test]
    fn down_and_unreachable_share_the_attempts() {
        let mut h = CheckState::default();
        h.apply_host(&active(code::DOWN, 10), 3);
        assert_eq!((h.state_type, h.attempt), (StateType::Soft, 1));
        // The parent went down meanwhile: UNREACHABLE, next attempt.
        h.apply_host(&active(code::UNREACHABLE, 20), 3);
        assert_eq!((h.state, h.state_type, h.attempt), (2, StateType::Soft, 2));
        h.apply_host(&active(code::UNREACHABLE, 30), 3);
        assert_eq!((h.state_type, h.last_hard_state), (StateType::Hard, 2));
        assert_eq!(h.last_time[2], 30);
    }

    #[test]
    fn services_log_their_alerts_as_engine() {
        let mut s = CheckState::default();
        // A soft problem, its retry, the hard state, a stable hard state.
        let logged: Vec<bool> = [2, 2, 2, 2]
            .iter()
            .enumerate()
            .map(|(i, state)| s.apply_service_logged(&active(*state, i as i64), 3, true).1)
            .collect();
        assert_eq!(logged, [true, true, true, false]);
        // Recovery is logged, a stable OK is not.
        assert!(s.apply_service_logged(&active(0, 10), 3, true).1);
        assert!(!s.apply_service_logged(&active(0, 11), 3, true).1);
        // Host down: only the hard change is logged.
        assert!(s.apply_service_logged(&active(2, 12), 3, false).1);
        assert!(!s.apply_service_logged(&active(2, 13), 3, false).1);
    }

    #[test]
    fn percent_state_change_weighs_recent_changes_more() {
        let mut flap = FlapState::default();
        // Stable: no change.
        assert_eq!(flap.update(0, true), 0.0);
        // One change, the most recent one: weight 1.25 of 20.
        let percent = flap.update(2, true);
        assert!((percent - 1.25 * 100.0 / 20.0).abs() < 1e-9, "{percent}");
        // Alternating over the whole history: every change counts.
        let mut flap = FlapState::default();
        let mut percent = 0.0;
        for i in 0..HISTORY {
            percent = flap.update(if i % 2 == 0 { 2 } else { 0 }, true);
        }
        assert!((percent - 100.0).abs() < 1e-9, "{percent}");
        // Not recorded: computed on the same history.
        assert_eq!(flap.update(2, false), percent);
    }

    #[test]
    fn host_soft_then_hard_then_hard_recovery() {
        let mut h = CheckState::default();
        let down = host_state_of(2);
        assert_eq!(h.apply_host(&active(down, 10), 2), Next::Retry);
        assert_eq!((h.state, h.state_type, h.attempt), (1, StateType::Soft, 1));
        assert_eq!(h.apply_host(&active(down, 20), 2), Next::Normal);
        assert_eq!((h.state_type, h.attempt), (StateType::Hard, 2));
        assert_eq!(h.last_hard_state_change, 20);
        h.apply_host(&active(host_state_of(1), 30), 2);
        assert_eq!((h.state, h.state_type, h.attempt), (0, StateType::Hard, 1));
        assert_eq!(h.last_hard_state_change, 30);
    }
}
