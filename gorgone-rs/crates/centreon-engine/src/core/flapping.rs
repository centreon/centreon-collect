//! Flap detection, as Engine does it (`service::check_for_flapping`,
//! `engine/src/service.cc:2155-2304`, `host::check_for_flapping`,
//! `engine/src/host.cc:2071-2234`, `set_flap`/`clear_flap`,
//! `handle_flap_detection_disabled`, `engine/src/flapping.cc`): a state
//! history of 21 results, its weighted percent of state change, the
//! thresholds of the object or the global ones, a comment and the
//! notifications while it flaps.

use crate::book::{EntryType, Source};
use crate::cfg::LogLevel;
use crate::objects::notify_on;
use crate::state::{Reason, code};

use super::{Core, ObjConf};

/// Author of the comments the engine adds.
const PROCESS_AUTHOR: &str = "(Centreon Engine Process)";

impl Core {
    /// `enable_flap_detection`, as `ENABLE_FLAP_DETECTION` left it.
    pub(super) fn flap_detection_on(&self) -> bool {
        self.program
            .flap_detection
            .unwrap_or(self.settings().enable_flap_detection)
    }

    /// `flap_detection_enabled` of object `index`, as the commands left it.
    pub(super) fn flap_enabled(&self, index: usize) -> bool {
        self.objs[index]
            .state
            .modified
            .flap_detection_enabled
            .unwrap_or(self.notify_conf(index).flap_detection_enabled)
    }

    /// The low and high thresholds of object `index`: its own, or the
    /// global ones when 0.
    fn thresholds(&self, index: usize) -> (f64, f64) {
        let notify = self.notify_conf(index);
        let settings = self.settings();
        let (low, high) = if self.objs[index].is_host() {
            (
                settings.low_host_flap_threshold,
                settings.high_host_flap_threshold,
            )
        } else {
            (
                settings.low_service_flap_threshold,
                settings.high_service_flap_threshold,
            )
        };
        (
            if notify.low_flap_threshold <= 0.0 {
                low
            } else {
                notify.low_flap_threshold
            },
            if notify.high_flap_threshold <= 0.0 {
                high
            } else {
                notify.high_flap_threshold
            },
        )
    }

    /// `service::check_for_flapping(update, allow_flapstart_notification)`:
    /// the state is recorded when its option is set and the host is UP;
    /// below the low threshold it stops flapping, at the high one it starts
    /// (unless OK and not flapping yet), in between nothing changes.
    pub(super) fn service_flapping(&mut self, index: usize, update: bool, allow_start: bool) {
        let ObjConf::Service { host, .. } = self.objs[index].conf else {
            return;
        };
        let (low, high) = self.thresholds(index);
        let options = self.notify_conf(index).flap_detection_options;
        let host_up = self.objs[host].state.state == code::OK;
        let st = &mut self.objs[index].state;
        let state = st.state;
        let record = update && options & notify_on::state(false, state) != 0 && host_up;
        let percent = st.flap.update(state, record);
        if !self.flap_detection_on() || !self.flap_enabled(index) {
            return;
        }
        if percent > low && percent < high {
            return;
        }
        let was = self.objs[index].state.flap.is_flapping;
        let flapping = percent >= high && (state != code::OK || was);
        if flapping && !was {
            self.set_flap(index, percent, high, allow_start);
        } else if !flapping && was {
            self.clear_flap(index, percent, low);
        }
    }

    /// `host::check_for_flapping(update, actual_check, allow)`: a state
    /// recorded by a service result only once per average check interval
    /// of its services; UNREACHABLE never recorded; it stops at or below
    /// the low threshold.
    pub(super) fn host_flapping(
        &mut self,
        index: usize,
        update: bool,
        actual_check: bool,
        allow_start: bool,
    ) {
        let (low, high) = self.thresholds(index);
        let il = self.interval_length() as i64;
        let host_id = self.objs[index].key.0;
        let (count, total): (i64, i64) = self
            .objs
            .iter()
            .filter(|o| o.key.0 == host_id && o.key.1 != 0)
            .fold((0, 0), |(c, t), o| {
                (c + 1, t + i64::from(o.check().check_interval))
            });
        let wait = if count == 0 {
            i64::from(self.notify_conf(index).notification_interval) * il
        } else {
            total * il / count
        };
        let options = self.notify_conf(index).flap_detection_options;
        let now = self.clock.now();
        let st = &mut self.objs[index].state;
        let state = st.state;
        let mut record =
            update && state != code::UNREACHABLE && options & notify_on::state(true, state) != 0;
        if record && !actual_check && now - st.flap.last_history_update < wait {
            record = false;
        }
        if record {
            st.flap.last_history_update = now;
        }
        let percent = st.flap.update(state, record);
        if !self.flap_detection_on() || !self.flap_enabled(index) {
            return;
        }
        if percent > low && percent < high {
            return;
        }
        let was = self.objs[index].state.flap.is_flapping;
        let flapping = percent > low && percent >= high && (state != code::OK || was);
        if flapping && !was {
            self.set_flap(index, percent, high, allow_start);
        } else if !flapping && was {
            self.clear_flap(index, percent, low);
        }
    }

    /// `set_flap`: the alert (a warning of the runtime logger), the
    /// comment, the flag, the FLAPPINGSTART notification.
    fn set_flap(&mut self, index: usize, percent: f64, high: f64, allow: bool) {
        let host = self.objs[index].is_host();
        let (what, op) = if host {
            ("host", ">")
        } else {
            ("service", ">=")
        };
        let line = match &self.objs[index].conf {
            ObjConf::Host(h) => format!(
                "HOST FLAPPING ALERT: {};STARTED; Host appears to have started flapping \
                 ({percent:.1}% change > {high:.1}% threshold)",
                h.name
            ),
            ObjConf::Service { conf, .. } => format!(
                "SERVICE FLAPPING ALERT: {};{};STARTED; Service appears to have started \
                 flapping ({percent:.1}% change >= {high:.1}% threshold)",
                conf.host_name, conf.description
            ),
        };
        if self.settings().log_level_runtime <= LogLevel::Warning {
            self.log(&line);
        }
        let comment = format!(
            "Notifications for this {what} are being suppressed because it was detected as \
             having been flapping between different states ({percent:.1}% change {op} \
             {high:.1}% threshold).  When the {what} state stabilizes and the flapping stops, \
             notifications will be re-enabled."
        );
        let key = self.objs[index].key;
        let now = self.clock.now();
        let id = self.add_comment(
            key,
            EntryType::Flapping,
            Source::Internal,
            now,
            PROCESS_AUTHOR,
            &comment,
            false,
        );
        let flap = &mut self.objs[index].state.flap;
        flap.comment_id = id;
        flap.is_flapping = true;
        tracing::info!(
            host_id = key.0,
            service_id = key.1,
            percent,
            "flapping started"
        );
        if allow {
            self.notify(index, Reason::FlappingStart, "", "", false);
        }
    }

    /// `clear_flap`: the alert, the comment deleted, FLAPPINGSTOP then
    /// RECOVERY notifications.
    fn clear_flap(&mut self, index: usize, percent: f64, low: f64) {
        let line = match &self.objs[index].conf {
            ObjConf::Host(h) => format!(
                "HOST FLAPPING ALERT: {};STOPPED; Host appears to have stopped flapping \
                 ({percent:.1}% change < {low:.1}% threshold)",
                h.name
            ),
            ObjConf::Service { conf, .. } => format!(
                "SERVICE FLAPPING ALERT: {};{};STOPPED; Service appears to have stopped \
                 flapping ({percent:.1}% change < {low:.1}% threshold)",
                conf.host_name, conf.description
            ),
        };
        if self.settings().log_level_events.logs_info() {
            self.log(&line);
        }
        self.end_flap(index);
        self.notify(index, Reason::FlappingStop, "", "", false);
        self.notify(index, Reason::Recovery, "", "", false);
    }

    fn end_flap(&mut self, index: usize) {
        let flap = &mut self.objs[index].state.flap;
        let comment = std::mem::take(&mut flap.comment_id);
        flap.is_flapping = false;
        if comment != 0 {
            self.delete_comment(comment);
        }
        let key = self.objs[index].key;
        tracing::info!(host_id = key.0, service_id = key.1, "flapping stopped");
    }

    /// `handle_flap_detection_disabled`: a flapping object stops flapping
    /// (FLAPPINGDISABLED then RECOVERY notifications); a status follows.
    /// The service line is a debug one in Engine: not in the event log.
    pub(super) fn flap_detection_disabled(&mut self, index: usize) {
        if self.objs[index].state.flap.is_flapping {
            self.end_flap(index);
            if let ObjConf::Host(h) = &self.objs[index].conf {
                let line = format!(
                    "HOST FLAPPING ALERT: {};DISABLED; Flap detection has been disabled",
                    h.name
                );
                if self.settings().log_level_events.logs_info() {
                    self.log(&line);
                }
            }
            self.notify(index, Reason::FlappingDisabled, "", "", false);
            self.notify(index, Reason::Recovery, "", "", false);
        }
        self.emit_status(index);
    }

    /// `enable_flap_detection` of an object (`service.cc:3027-3060`,
    /// `host.cc:2772-2803`): the flapping computed again, no record.
    pub(super) fn enable_flapping(&mut self, index: usize) {
        if self.objs[index].is_host() {
            self.host_flapping(index, false, false, true);
        } else {
            self.service_flapping(index, false, true);
            self.emit_status(index);
        }
    }

    /// `enable_flap_detection_routines` / `disable_…` (`flapping.cc:36-97`).
    pub(super) fn set_global_flap_detection(&mut self, enabled: bool) {
        if self.flap_detection_on() == enabled {
            return;
        }
        self.program.flap_detection = Some(enabled);
        let (hosts, services): (Vec<usize>, Vec<usize>) =
            (0..self.objs.len()).partition(|i| self.objs[*i].is_host());
        for index in hosts.into_iter().chain(services) {
            if enabled {
                if self.objs[index].is_host() {
                    self.host_flapping(index, false, false, true);
                } else {
                    self.service_flapping(index, false, true);
                }
            } else {
                self.flap_detection_disabled(index);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::state::FlapState;

    #[test]
    fn thresholds_decide_as_engine() {
        // 10 changes of state in the last 11 results: well above 30 %.
        let mut flap = FlapState::default();
        let mut percent = 0.0;
        for i in 0..11 {
            percent = flap.update(if i % 2 == 0 { 2 } else { 0 }, true);
        }
        assert!(percent > 30.0, "{percent}");
        // Then stable: it decreases below 20 % after enough results.
        for _ in 0..21 {
            percent = flap.update(0, true);
        }
        assert!(percent < 20.0, "{percent}");
    }
}
