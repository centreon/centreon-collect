//! Downtimes, comments and acknowledgements of the core, with Engine's
//! rules: `engine/src/downtimes/downtime_manager.cc`, `host_downtime.cc`,
//! `service_downtime.cc`, `engine/src/comment.cc`, the acknowledgement
//! functions of `engine/src/commands/commands.cc:2515-2617` and their
//! automatic removal (`service.cc:1503-1515`, `host.cc:2438-2450`), and the
//! BBDO events `cbmod` writes for them (`broker/neb/src/cbmod.cc:251-372`).

use std::collections::BTreeMap;

use crate::book::{Acknowledgement, Comment, Downtime, EntryType, Key, Source};
use crate::events;
use crate::logs::DowntimeAlert;
use crate::recurring::{self, RecurringConf, Rule, Target};
use crate::state::AckType;
use crate::tz::Tz;

use super::{Core, Timer};

/// Bounds of `schedule_downtime` (1/1/2100, 366 days).
const MAX_TIME: i64 = 4_102_441_200;
const MAX_DURATION: u64 = 31_622_400;

/// A downtime to schedule, as `SCHEDULE_*_DOWNTIME` gives it.
#[derive(Debug, Clone)]
pub(crate) struct Request {
    pub entry_time: i64,
    pub author: String,
    pub comment: String,
    pub start: i64,
    pub end: i64,
    pub fixed: bool,
    pub triggered_by: u64,
    /// Seconds; replaced by `end - start` for a fixed downtime.
    pub duration: u64,
}

/// `get_datetime_string(..., SHORT_DATE_TIME)` with the default `us`
/// date format.
fn short_date_time(zone: &Tz, time: i64) -> String {
    let t = zone.localtime(time);
    format!(
        "{:02}-{:02}-{:04} {:02}:{:02}:{:02}",
        t.mon + 1,
        t.mday,
        t.year + 1900,
        t.hour,
        t.min,
        t.sec
    )
}

impl Core {
    fn emit_downtime(&mut self, id: u64, deletion: Option<i64>) {
        if let Some(downtime) = self.book.downtimes.get(&id) {
            let frame = events::downtime(self.poller_id(), downtime, deletion);
            self.emit(frame);
        }
    }

    /// Zone of the dates in comments: `use_timezone`, else the process one.
    fn comment_zone(&mut self) -> std::sync::Arc<Tz> {
        let zone = self
            .loaded
            .as_ref()
            .map(|l| l.config.settings.use_timezone.clone())
            .unwrap_or_default();
        self.zones.get(&zone)
    }

    /// `downtime_manager::schedule_downtime` then `register_downtime`: the
    /// id of the new downtime.
    pub(crate) fn schedule_downtime(&mut self, key: Key, request: &Request) -> Result<u64, String> {
        let now = self.clock.now();
        // Don't add old or invalid downtimes.
        if request.start >= request.end || request.end <= now {
            return Err(format!(
                "downtime from {} to {} not scheduled: it ends before it starts or is over",
                request.start, request.end
            ));
        }
        if request.triggered_by > 0 && !self.book.downtimes.contains_key(&request.triggered_by) {
            return Err(format!(
                "can not add triggered downtime without a valid parent ({})",
                request.triggered_by
            ));
        }
        let start = request.start.min(MAX_TIME);
        let end = request.end.min(MAX_TIME);
        let duration = if request.fixed {
            // Duration is computed, not given (`cmd_schedule_downtime`).
            u64::try_from(request.end - request.start).unwrap_or(0)
        } else {
            request.duration
        }
        .min(MAX_DURATION);
        let id = self.book.next_downtime_id();
        self.book.downtimes.insert(
            id,
            Downtime {
                id,
                host_id: key.0,
                service_id: key.1,
                entry_time: request.entry_time,
                author: request.author.clone(),
                comment: request.comment.clone(),
                start_time: start,
                end_time: end,
                fixed: request.fixed,
                triggered_by: request.triggered_by,
                duration: u32::try_from(duration).unwrap_or(u32::MAX),
                in_effect: false,
                start_flex: false,
                incremented_pending: false,
                comment_id: 0,
                started: false,
                cancelled: false,
                actual_start: None,
                actual_end: None,
            },
        );
        // NEBTYPE_DOWNTIME_ADD (`cbmod::add_downtime`).
        self.emit_downtime(id, None);
        self.subscribe(id);
        tracing::info!(
            downtime_id = id,
            host_id = key.0,
            service_id = key.1,
            start,
            end,
            fixed = request.fixed,
            "downtime scheduled"
        );
        Ok(id)
    }

    /// `downtime::subscribe`: the comment of the downtime, and its start
    /// event unless another downtime triggers it.
    fn subscribe(&mut self, id: u64) {
        let Some(downtime) = self.book.downtimes.get(&id).cloned() else {
            return;
        };
        let zone = self.comment_zone();
        let what = if downtime.service_id == 0 {
            "host"
        } else {
            "service"
        };
        let (start, end) = (
            short_date_time(&zone, downtime.start_time),
            short_date_time(&zone, downtime.end_time),
        );
        let text = if downtime.fixed {
            format!(
                "This {what} has been scheduled for fixed downtime from {start} to {end} \
                 Notifications for the {what} will not be sent out during that time period."
            )
        } else {
            let hours = downtime.duration / 3600;
            let minutes = (downtime.duration - hours * 3600) / 60;
            format!(
                "This {what} has been scheduled for flexible downtime starting between {start} \
                 and {end} and lasting for a period of {hours} hours and {minutes} minutes. \
                 Notifications for the {what} will not be sent out during that time period."
            )
        };
        let now = self.clock.now();
        let comment_id = self.add_comment(
            downtime.key(),
            EntryType::Downtime,
            Source::Internal,
            now,
            "(Centreon Engine Process)",
            &text,
            false,
        );
        if let Some(downtime) = self.book.downtimes.get_mut(&id) {
            downtime.comment_id = comment_id;
        }
        if downtime.triggered_by == 0 {
            self.add_timer(downtime.start_time, Timer::Downtime(id));
        }
    }

    /// `downtime::handle`: starts the downtime, or stops it when in effect.
    pub(crate) fn handle_downtime(&mut self, id: u64) {
        let Some(downtime) = self.book.downtimes.get(&id).cloned() else {
            return;
        };
        let Some(&index) = self.by_key.get(&downtime.key()) else {
            return;
        };
        let now = self.clock.now();
        // A flexible downtime waits for a problem.
        if !downtime.fixed && !downtime.start_flex && self.objs[index].is_ok() {
            self.objs[index].pending_flex += 1;
            if let Some(d) = self.book.downtimes.get_mut(&id) {
                d.incremented_pending = true;
            }
            self.add_timer(downtime.end_time.saturating_add(1), Timer::ExpireDowntimes);
            return;
        }
        if downtime.in_effect {
            // NEBTYPE_DOWNTIME_STOP (`cbmod::stop_downtime`).
            if let Some(d) = self.book.downtimes.get_mut(&id) {
                d.cancelled = false;
                d.actual_end = Some(now);
                d.in_effect = false;
            }
            self.emit_downtime(id, None);
            let obj = &mut self.objs[index];
            obj.state.downtime_depth -= 1;
            let stopped = obj.state.downtime_depth == 0;
            if !downtime.fixed && downtime.incremented_pending && obj.pending_flex > 0 {
                obj.pending_flex -= 1;
            }
            if stopped {
                self.log_downtime_alert(index, DowntimeAlert::Stopped);
            }
            self.emit_status(index);
            // Downtimes this one triggered end with it.
            let triggered: Vec<u64> = self
                .book
                .downtimes
                .values()
                .filter(|d| d.triggered_by == id)
                .map(|d| d.id)
                .collect();
            for child in triggered {
                if self.book.downtimes.get(&child).is_some_and(|d| d.in_effect) {
                    self.handle_downtime(child);
                } else {
                    self.delete_downtime(child);
                }
            }
            self.delete_downtime(id);
        } else {
            // NEBTYPE_DOWNTIME_START (`cbmod::start_downtime`).
            if let Some(d) = self.book.downtimes.get_mut(&id) {
                d.started = true;
                d.actual_start = Some(now);
                d.in_effect = true;
            }
            self.emit_downtime(id, None);
            if self.objs[index].state.downtime_depth == 0 {
                self.log_downtime_alert(index, DowntimeAlert::Started);
            }
            let obj = &mut self.objs[index];
            obj.state.downtime_depth += 1;
            self.emit_status(index);
            let stop = if downtime.fixed {
                downtime.end_time.saturating_add(1)
            } else {
                now + i64::from(downtime.duration)
            };
            self.add_timer(stop, Timer::Downtime(id));
            let triggered: Vec<u64> = self
                .book
                .downtimes
                .values()
                .filter(|d| d.triggered_by == id)
                .map(|d| d.id)
                .collect();
            for child in triggered {
                self.handle_downtime(child);
            }
        }
    }

    /// `downtime_manager::unschedule_downtime`: cancels a downtime and the
    /// ones it triggered. `false` when it does not exist.
    pub(crate) fn unschedule_downtime(&mut self, id: u64) -> bool {
        let Some(downtime) = self.book.downtimes.get(&id).cloned() else {
            return false;
        };
        let now = self.clock.now();
        if let Some(&index) = self.by_key.get(&downtime.key()) {
            let obj = &mut self.objs[index];
            if !downtime.fixed && downtime.incremented_pending && obj.pending_flex > 0 {
                obj.pending_flex -= 1;
            }
            if downtime.in_effect {
                if let Some(d) = self.book.downtimes.get_mut(&id) {
                    d.cancelled = true;
                    d.actual_end = Some(now);
                    d.in_effect = false;
                }
                self.emit_downtime(id, None);
                self.objs[index].state.downtime_depth -= 1;
                self.emit_status(index);
                if self.objs[index].state.downtime_depth == 0 {
                    self.log_downtime_alert(index, DowntimeAlert::Cancelled);
                }
            }
        }
        self.delete_downtime(id);
        let triggered: Vec<u64> = self
            .book
            .downtimes
            .values()
            .filter(|d| d.triggered_by == id)
            .map(|d| d.id)
            .collect();
        for child in triggered {
            self.unschedule_downtime(child);
        }
        true
    }

    /// The destructor of a downtime: its comment goes, then
    /// NEBTYPE_DOWNTIME_DELETE (`cbmod::remove_downtime`).
    fn delete_downtime(&mut self, id: u64) {
        let Some(mut downtime) = self.book.downtimes.remove(&id) else {
            return;
        };
        self.delete_comment(downtime.comment_id);
        let now = self.clock.now();
        if !downtime.started {
            downtime.cancelled = true;
        }
        if downtime.actual_end.is_none() {
            downtime.actual_end = Some(now);
        }
        let frame = events::downtime(self.poller_id(), &downtime, Some(now));
        self.emit(frame);
    }

    /// `check_for_expired_downtime`: downtimes never started and over.
    pub(crate) fn expire_downtimes(&mut self) {
        let now = self.clock.now();
        let expired: Vec<u64> = self
            .book
            .downtimes
            .values()
            .filter(|d| !d.in_effect && d.end_time < now)
            .map(|d| d.id)
            .collect();
        for id in expired {
            tracing::debug!(downtime_id = id, "expiring downtime");
            self.delete_downtime(id);
        }
    }

    /// `check_pending_flex_*_downtime`: flexible downtimes of a problem
    /// object whose window is open start now.
    fn check_pending_flex(&mut self, index: usize) {
        if self.objs[index].is_ok() {
            return;
        }
        let key = self.objs[index].key;
        let now = self.clock.now();
        let starting: Vec<u64> = self
            .book
            .downtimes_of(key)
            .filter(|d| !d.fixed && !d.in_effect && d.triggered_by == 0)
            .filter(|d| d.start_time <= now && now <= d.end_time)
            .map(|d| d.id)
            .collect();
        for id in starting {
            if let Some(d) = self.book.downtimes.get_mut(&id) {
                d.start_flex = true;
            }
            self.handle_downtime(id);
        }
    }

    /// After a result: a service whose problem appeared or changed, with
    /// flexible downtimes pending, may start them (`service.cc:1974-1980`);
    /// a host on any change (`host.cc:2464-2467`).
    pub(crate) fn flexible_downtimes_after_result(
        &mut self,
        index: usize,
        state_change: bool,
        hard_change: bool,
    ) {
        let obj = &self.objs[index];
        let start = if obj.is_host() {
            state_change
        } else {
            !obj.is_ok() && (state_change || hard_change) && obj.pending_flex > 0
        };
        if start {
            self.check_pending_flex(index);
        }
    }

    /// After a result: a normal acknowledgement ends with any change of
    /// state, a sticky one with the recovery; their non-persistent
    /// comments go (`service.cc:1488-1515`, `host.cc:2409-2450`).
    pub(crate) fn acknowledgement_after_result(
        &mut self,
        index: usize,
        state_change: bool,
        hard_change: bool,
    ) {
        let obj = &mut self.objs[index];
        let ok = obj.is_ok();
        let ack = obj.state.acknowledgement;
        let ends = if obj.is_host() {
            state_change && (ack == AckType::Normal || (ack == AckType::Sticky && ok))
        } else {
            (state_change || hard_change)
                && ((ack == AckType::Normal && (state_change || !hard_change))
                    || (ack == AckType::Sticky && ok))
        };
        if ends {
            obj.state.acknowledgement = AckType::None;
            let key = obj.key;
            self.delete_ack_comments(key);
        } else if ok && !obj.is_host() {
            // "Reset the acknowledgement flag (this should already have
            // been done, but just in case...)" (`service.cc:1574-1576`).
            obj.state.acknowledgement = AckType::None;
        }
    }

    /// `acknowledge_*_problem`.
    pub(crate) fn acknowledge(
        &mut self,
        index: usize,
        author: &str,
        comment: &str,
        sticky: bool,
        notify: bool,
        persistent: bool,
    ) -> String {
        // Cannot acknowledge a non-existent problem.
        if self.objs[index].is_ok() {
            return "no problem to acknowledge".to_owned();
        }
        let now = self.clock.now();
        let obj = &mut self.objs[index];
        obj.state.acknowledgement = if sticky {
            AckType::Sticky
        } else {
            AckType::Normal
        };
        let key = obj.key;
        let ack = Acknowledgement {
            host_id: key.0,
            service_id: key.1,
            author: author.to_owned(),
            comment: comment.to_owned(),
            entry_time: now,
            sticky,
            notify,
            persistent,
            state: obj.state.state,
        };
        let frame = events::acknowledgement(self.poller_id(), &ack, None);
        self.book.acknowledgements.insert(key, ack);
        self.emit(frame);
        self.emit_status(index);
        self.add_comment(
            key,
            EntryType::Acknowledgement,
            Source::Internal,
            now,
            author,
            comment,
            persistent,
        );
        tracing::info!(
            host_id = key.0,
            service_id = key.1,
            sticky,
            "problem acknowledged"
        );
        self.schedule_acknowledgement_expiry(key, now);
        "problem acknowledged".to_owned()
    }

    /// `schedule_acknowledgement_expiration` (`service.cc:935-945`,
    /// `host.cc:1202-1212`): with an `acknowledgement_timeout`, a timer at
    /// the time of the acknowledgement plus the timeout, in units of
    /// `interval_length`.
    pub(crate) fn schedule_acknowledgement_expiry(&mut self, key: Key, acknowledged: i64) {
        let Some(&index) = self.by_key.get(&key) else {
            return;
        };
        let timeout = self.acknowledgement_timeout(index);
        if timeout > 0 && acknowledged != 0 {
            self.add_timer(acknowledged + timeout, Timer::AcknowledgementExpiry(key));
        }
    }

    /// `acknowledgement_timeout` of object `index`, in seconds.
    fn acknowledgement_timeout(&self, index: usize) -> i64 {
        let units = match &self.objs[index].conf {
            super::ObjConf::Host(host) => host.acknowledgement_timeout,
            super::ObjConf::Service { conf, .. } => conf.acknowledgement_timeout,
        };
        i64::from(units) * self.interval_length() as i64
    }

    /// `check_for_expired_acknowledgement` (`service.cc:828-848`,
    /// `host.cc:2340-2354`), its test included: the acknowledgement ends
    /// when its time plus the timeout is not past yet, which is the case
    /// when the timer runs on time.
    pub(crate) fn expire_acknowledgement(&mut self, key: Key) {
        let Some(&index) = self.by_key.get(&key) else {
            return;
        };
        if self.objs[index].state.acknowledgement == AckType::None {
            return;
        }
        let timeout = self.acknowledgement_timeout(index);
        let Some(acknowledged) = self.book.acknowledgements.get(&key).map(|a| a.entry_time) else {
            return;
        };
        let now = self.clock.now();
        if timeout > 0 && acknowledged + timeout >= now {
            self.log_acknowledgement_expired(index);
            self.objs[index].state.acknowledgement = AckType::None;
            self.emit_status(index);
        }
    }

    /// `remove_*_acknowledgement`.
    pub(crate) fn remove_acknowledgement(&mut self, index: usize) {
        self.objs[index].state.acknowledgement = AckType::None;
        self.emit_status(index);
        let key = self.objs[index].key;
        self.delete_ack_comments(key);
    }

    /// `comment::comment` for a new comment: its id.
    #[allow(clippy::too_many_arguments)]
    pub(crate) fn add_comment(
        &mut self,
        key: Key,
        entry_type: EntryType,
        source: Source,
        entry_time: i64,
        author: &str,
        data: &str,
        persistent: bool,
    ) -> u64 {
        let id = self.book.next_comment_id();
        let comment = Comment {
            id,
            host_id: key.0,
            service_id: key.1,
            entry_time,
            author: author.to_owned(),
            data: data.to_owned(),
            persistent,
            source,
            entry_type,
        };
        let frame = events::comment(self.poller_id(), &comment, None);
        self.book.comments.insert(id, comment);
        self.emit(frame);
        id
    }

    /// `comment::delete_comment`.
    pub(crate) fn delete_comment(&mut self, id: u64) -> bool {
        let Some(comment) = self.book.comments.remove(&id) else {
            return false;
        };
        let now = self.clock.now();
        let frame = events::comment(self.poller_id(), &comment, Some(now));
        self.emit(frame);
        true
    }

    /// `delete_host_comments`, `delete_service_comments`.
    pub(crate) fn delete_comments_of(&mut self, key: Key) -> usize {
        let ids: Vec<u64> = self
            .book
            .comments
            .values()
            .filter(|c| (c.host_id, c.service_id) == key)
            .map(|c| c.id)
            .collect();
        for id in &ids {
            self.delete_comment(*id);
        }
        ids.len()
    }

    /// `delete_*_acknowledgement_comments`: the non-persistent ones.
    fn delete_ack_comments(&mut self, key: Key) {
        let ids: Vec<u64> = self
            .book
            .comments
            .values()
            .filter(|c| {
                (c.host_id, c.service_id) == key
                    && c.entry_type == EntryType::Acknowledgement
                    && !c.persistent
            })
            .map(|c| c.id)
            .collect();
        for id in ids {
            self.delete_comment(id);
        }
    }

    /// Objects removed by a reload take their downtimes, comments and
    /// acknowledgement with them.
    pub(crate) fn forget_objects(&mut self, removed: &[Key]) {
        for key in removed {
            let downtimes: Vec<u64> = self.book.downtimes_of(*key).map(|d| d.id).collect();
            for id in downtimes {
                self.delete_downtime(id);
            }
            self.delete_comments_of(*key);
            self.book.acknowledgements.remove(key);
        }
    }

    /// Puts back what the retention file kept, for the objects that still
    /// exist: comments (not those of downtimes, made again), open
    /// acknowledgements, downtimes not over, registered again as Engine
    /// does with the downtimes of its retention (they start again).
    pub(crate) fn restore_book(&mut self, now: i64) {
        let Some(retained) = self.retained.take() else {
            return;
        };
        let exists = |core: &Self, key: &Key| core.by_key.contains_key(key);
        self.book.set_last_downtime_id(retained.last_downtime_id);
        for comment in retained.comments {
            let key = (comment.host_id, comment.service_id);
            if comment.entry_type != EntryType::Downtime && exists(self, &key) {
                self.book.comments.insert(comment.id, comment);
            }
        }
        for ack in retained.acknowledgements {
            let key = (ack.host_id, ack.service_id);
            if exists(self, &key) {
                let acknowledged = ack.entry_time;
                self.book.acknowledgements.insert(key, ack);
                // `applier::scheduler`: the expirations of the retained
                // acknowledgements (`scheduler.cc:905-915`, `:1015-1022`).
                self.schedule_acknowledgement_expiry(key, acknowledged);
            }
        }
        let mut downtimes: Vec<Downtime> = retained
            .downtimes
            .into_iter()
            .filter(|d| exists(self, &d.key()) && d.end_time >= now)
            .collect();
        downtimes.sort_by_key(|d| d.id);
        let ids: Vec<u64> = downtimes.iter().map(|d| d.id).collect();
        for downtime in downtimes {
            // A triggered downtime without its parent is dropped
            // (`xdddefault_validate_downtime_data`).
            if downtime.triggered_by != 0 && !ids.contains(&downtime.triggered_by) {
                continue;
            }
            self.book.downtimes.insert(downtime.id, downtime);
        }
        let restored: Vec<u64> = self.book.downtimes.keys().copied().collect();
        for id in restored {
            self.emit_downtime(id, None);
            self.subscribe(id);
        }
        tracing::info!(
            downtimes = self.book.downtimes.len(),
            comments = self.book.comments.len(),
            acknowledgements = self.book.acknowledgements.len(),
            "retention applied"
        );
    }

    /// Puts the recurring downtime rules of a configuration in force: an
    /// unchanged rule keeps what it handled, a changed or removed one
    /// loses its downtimes not started yet, a new or changed one starts
    /// with nothing handled, limited to the occurrences after the start of
    /// the process when it comes with the first configuration
    /// (`applier/recurringdowntime.cc`, `recurring_downtime.hh`).
    pub(crate) fn apply_rules(&mut self, confs: BTreeMap<u64, RecurringConf>, now: i64) {
        let not_before = if self.initial_load_done {
            0
        } else {
            self.start_time
        };
        let old = std::mem::take(&mut self.rules);
        for (id, rule) in &old {
            if confs.get(id) != Some(&rule.conf) {
                self.delete_future_downtimes(*id, now);
            }
        }
        let mut old = old;
        for (id, conf) in confs {
            let rule = match old.remove(&id) {
                Some(rule) if rule.conf == conf => rule,
                _ => Rule::new(conf, not_before),
            };
            self.rules.insert(id, rule);
        }
        self.run_rules(now);
    }

    /// `delete_future_downtimes`: the downtimes of rule `id` not started.
    fn delete_future_downtimes(&mut self, id: u64, now: i64) {
        let comment = recurring::comment(id);
        let ids: Vec<u64> = self
            .book
            .downtimes
            .values()
            .filter(|d| d.start_time > now && !d.in_effect && d.comment == comment)
            .map(|d| d.id)
            .collect();
        for downtime in &ids {
            self.unschedule_downtime(*downtime);
        }
        if !ids.is_empty() {
            tracing::info!(
                rule = id,
                deleted = ids.len(),
                "recurring downtime: future downtimes deleted"
            );
        }
    }

    /// `recurring_downtime::schedule_all`.
    pub(crate) fn run_rules(&mut self, now: i64) {
        let mut rules = std::mem::take(&mut self.rules);
        let default_zone = self
            .loaded
            .as_ref()
            .map(|l| l.config.settings.use_timezone.clone())
            .unwrap_or_default();
        for rule in rules.values_mut() {
            // Targets known here, each in the zone of the rule, else of
            // its host: (host_id, host target, services, zone).
            let mut found: Vec<(u64, bool, Vec<u64>, String)> = Vec::new();
            let zone_of = |core: &Self, host_index: usize| -> String {
                if !rule.conf.timezone.is_empty() {
                    return rule.conf.timezone.clone();
                }
                match core.objs[host_index].timezone() {
                    "" => default_zone.clone(),
                    zone => zone.to_owned(),
                }
            };
            for host_id in &rule.conf.host_ids {
                let Some(&index) = self.by_key.get(&(*host_id, 0)) else {
                    continue;
                };
                let services = self
                    .objs
                    .iter()
                    .filter(|o| o.key.0 == *host_id && o.key.1 != 0)
                    .map(|o| o.key.1)
                    .collect();
                found.push((*host_id, true, services, zone_of(self, index)));
            }
            for (host_id, service_id) in &rule.conf.service_ids {
                let Some(&index) = self.by_key.get(&(*host_id, 0)) else {
                    continue;
                };
                if self.by_key.contains_key(&(*host_id, *service_id)) {
                    found.push((*host_id, false, vec![*service_id], zone_of(self, index)));
                }
            }
            let zones: Vec<std::sync::Arc<Tz>> = found
                .iter()
                .map(|(_, _, _, zone)| self.zones.get(zone))
                .collect();
            let targets: Vec<Target<'_>> = found
                .into_iter()
                .zip(&zones)
                .map(|((host_id, host, services, _), tz)| Target {
                    host_id,
                    host,
                    services,
                    tz,
                })
                .collect();
            let comment = recurring::comment(rule.conf.id);
            let book = &self.book;
            let due = rule.run(now, &targets, |host_id, service_id, start| {
                book.downtimes_of((host_id, service_id))
                    .any(|d| d.comment == comment && d.start_time == start)
            });
            for item in due {
                let occurrence = item.occurrence;
                let request = Request {
                    entry_time: now,
                    author: recurring::AUTHOR.to_owned(),
                    comment: comment.clone(),
                    start: occurrence.start,
                    end: occurrence.end,
                    fixed: occurrence.fixed,
                    triggered_by: 0,
                    duration: if occurrence.fixed {
                        u64::try_from(occurrence.end - occurrence.start).unwrap_or(0)
                    } else {
                        u64::from(occurrence.duration)
                    },
                };
                let what = if item.service_id == 0 {
                    "host"
                } else {
                    "service"
                };
                match self.schedule_downtime((item.host_id, item.service_id), &request) {
                    Ok(id) => tracing::info!(
                        "Recurring downtime #{}: {what} downtime {id} scheduled on {}/{} from {} to {}",
                        rule.conf.id,
                        item.host_id,
                        item.service_id,
                        occurrence.start,
                        occurrence.end
                    ),
                    Err(error) => tracing::warn!(
                        "Recurring downtime #{}: could not schedule the downtime of {}/{}: {error}",
                        rule.conf.id,
                        item.host_id,
                        item.service_id
                    ),
                }
            }
        }
        self.rules = rules;
    }
}
