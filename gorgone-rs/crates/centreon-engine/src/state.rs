//! State of a host or service and how a check result changes it: the
//! Nagios 3 rules of `service::handle_async_check_result`
//! (`engine/src/service.cc:1255-2060`), without notifications, flapping,
//! acknowledgements nor event handlers. Pure: no clock, no I/O.

use serde::{Deserialize, Serialize};

/// SOFT or HARD.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum StateType {
    /// Not confirmed yet: retried at `retry_interval`.
    Soft,
    /// Confirmed.
    Hard,
}

/// Service states (`ServiceStatus.State`); hosts use 0 (UP) and 1 (DOWN).
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
    pub fn apply_service(&mut self, outcome: &Outcome, max_attempts: u32, host_up: bool) -> Next {
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
            return next(Next::Normal);
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
                return next(Next::Normal);
            }
            self.state_type = StateType::Soft;
            next(Next::Retry)
        } else {
            self.state_type = StateType::Hard;
            self.last_hard_state = self.state;
            next(Next::Normal)
        }
    }

    /// Applies a host result (state already UP or DOWN). A recovery is
    /// always hard; problems go SOFT then HARD after `max_attempts`.
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
        if state_change {
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
/// (Nagios 3, no parents so never UNREACHABLE).
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
