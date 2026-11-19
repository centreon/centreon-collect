//! Recurring downtimes (Centreon "downtime cycles"): the `recurringdowntime`
//! object of `common/engine_conf/recurringdowntime_helper.cc` and the rules
//! of `engine/src/downtimes/recurring_downtime.cc` (see its header for the
//! exact rule): occurrences of yesterday, today and tomorrow in the zone of
//! the rule or of the host, scheduled when they start within 600 s or are
//! running, each (rule, target, start) once per rule object, no catch-up of
//! an occurrence started before the process for a rule loaded at start.

use std::collections::BTreeMap;

use crate::cfg::RawObject;
use crate::tz::{Tm, Tz};

/// Author of the downtimes of a rule.
pub const AUTHOR: &str = "Downtime cycle";

/// How long before its start an occurrence is scheduled.
pub const LOOK_AHEAD: i64 = 600;

/// Days a period applies to.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PeriodKind {
    /// `weekly:1,2` (1 is Monday, 7 Sunday).
    Weekly,
    /// `monthly:1,15`.
    Monthly,
    /// `nth:first:1`: the rank-th weekday of the month, -1 the last.
    Nth,
}

/// One `period` line.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Period {
    pub kind: PeriodKind,
    pub days: Vec<u32>,
    pub rank: i32,
    pub start_minute: u32,
    pub end_minute: u32,
    pub fixed: bool,
    pub duration: u32,
}

/// A `recurringdowntime` object.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RecurringConf {
    pub id: u64,
    pub name: String,
    /// `TZ` syntax; the zone of each host when empty.
    pub timezone: String,
    pub host_ids: Vec<u64>,
    pub service_ids: Vec<(u64, u64)>,
    pub periods: Vec<Period>,
}

/// `HH:MM` in minutes; `24:00` accepted.
fn hhmm(value: &str) -> Option<u32> {
    let (h, m) = value.split_once(':')?;
    if h.is_empty() || m.len() != 2 {
        return None;
    }
    let (h, m): (u32, u32) = (h.parse().ok()?, m.parse().ok()?);
    (h <= 24 && m <= 59 && (h < 24 || m == 0)).then_some(h * 60 + m)
}

fn days(value: &str, min: u32, max: u32) -> Option<Vec<u32>> {
    if value.is_empty() {
        return None;
    }
    value
        .split(',')
        .map(|d| d.parse().ok().filter(|d| (min..=max).contains(d)))
        .collect()
}

impl Period {
    /// `recurringdowntime_helper::parse_period`.
    pub fn parse(value: &str) -> Option<Self> {
        let parts: Vec<&str> = value.split([' ', '\t']).filter(|p| !p.is_empty()).collect();
        let [days_part, range, mode] = parts.as_slice() else {
            return None;
        };
        let pieces: Vec<&str> = days_part.split(':').collect();
        let (kind, days, rank) = match pieces.as_slice() {
            ["weekly", list] => (PeriodKind::Weekly, days(list, 1, 7)?, 0),
            ["monthly", list] => (PeriodKind::Monthly, days(list, 1, 31)?, 0),
            ["nth", rank, day] => {
                let rank = match *rank {
                    "first" => 1,
                    "second" => 2,
                    "third" => 3,
                    "fourth" => 4,
                    "last" => -1,
                    _ => return None,
                };
                let day: u32 = day.parse().ok().filter(|d| (1..=7).contains(d))?;
                (PeriodKind::Nth, vec![day], rank)
            }
            _ => return None,
        };
        let (start, end) = range.split_once('-')?;
        let (start_minute, end_minute) = (hhmm(start)?, hhmm(end)?);
        if end_minute <= start_minute {
            return None;
        }
        let (fixed, duration) = if *mode == "fixed" {
            (true, 0)
        } else {
            (false, mode.strip_prefix("flexible:")?.parse().ok()?)
        };
        Some(Self {
            kind,
            days,
            rank,
            start_minute,
            end_minute,
            fixed,
            duration,
        })
    }

    /// `period_matches`: the period applies to the local day `day`.
    fn matches(&self, day: &Tm, tz: &Tz) -> bool {
        let weekday = if day.wday == 0 { 7 } else { day.wday as u32 };
        match self.kind {
            PeriodKind::Weekly => self.days.contains(&weekday),
            PeriodKind::Monthly => self.days.contains(&(day.mday as u32)),
            PeriodKind::Nth => {
                if self.days.first() != Some(&weekday) {
                    return false;
                }
                if self.rank == -1 {
                    let mut next = *day;
                    next.mday += 7;
                    next.hour = 12;
                    tz.mktime(&mut next);
                    return next.mon != day.mon;
                }
                (day.mday - 1) / 7 + 1 == self.rank
            }
        }
    }
}

impl RecurringConf {
    /// The rule of a raw object, checked as `check_validity`.
    pub(crate) fn from_raw(object: &RawObject) -> Result<Self, String> {
        let field = |key: &str| object.fields.get(key).map(|v| v.trim()).unwrap_or_default();
        let id: u64 = match field("downtime_id") {
            "" => 0,
            value => value
                .parse()
                .map_err(|_| format!("invalid downtime_id {value:?}"))?,
        };
        if id == 0 {
            return Err("recurring downtime has no id (property 'downtime_id')".to_owned());
        }
        let mut host_ids = Vec::new();
        for value in field("host_ids")
            .split(',')
            .map(str::trim)
            .filter(|v| !v.is_empty())
        {
            match value.parse::<u64>() {
                Ok(host_id) if host_id > 0 => host_ids.push(host_id),
                _ => return Err(format!("recurring downtime {id}: invalid host_ids")),
            }
        }
        let mut service_ids = Vec::new();
        for value in field("service_ids")
            .split(',')
            .map(str::trim)
            .filter(|v| !v.is_empty())
        {
            let pair = value.split_once(':').and_then(|(h, s)| {
                let (h, s): (u64, u64) = (h.parse().ok()?, s.parse().ok()?);
                (h > 0 && s > 0).then_some((h, s))
            });
            match pair {
                Some(pair) => service_ids.push(pair),
                None => return Err(format!("recurring downtime {id}: invalid service_ids")),
            }
        }
        let mut periods = Vec::new();
        for (_, value) in object.lines.iter().filter(|(key, _)| key == "period") {
            periods.push(
                Period::parse(value)
                    .ok_or_else(|| format!("recurring downtime {id}: invalid period {value:?}"))?,
            );
        }
        if periods.is_empty() {
            return Err(format!(
                "recurring downtime {id} has no period (property 'period')"
            ));
        }
        if host_ids.is_empty() && service_ids.is_empty() {
            return Err(format!(
                "recurring downtime {id} has no target (properties 'host_ids' and 'service_ids')"
            ));
        }
        Ok(Self {
            id,
            name: field("downtime_name").to_owned(),
            timezone: field("timezone").to_owned(),
            host_ids,
            service_ids,
            periods,
        })
    }
}

/// The comment of the downtimes of rule `id`, the one the web cron used.
#[must_use]
pub fn comment(id: u64) -> String {
    format!("[Downtime cycle #{id}]")
}

/// One occurrence of a period: `[start, end)`.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct Occurrence {
    pub start: i64,
    pub end: i64,
    pub fixed: bool,
    pub duration: u32,
}

/// `compute_occurrences`: occurrences of the local days before, of and
/// after `now`, with `start <= now + look_ahead` and `end > now`, sorted.
#[must_use]
pub fn occurrences(rule: &RecurringConf, tz: &Tz, now: i64, look_ahead: i64) -> Vec<Occurrence> {
    let today = tz.localtime(now);
    let mut found = Vec::new();
    for offset in -1..=1 {
        // Noon is never skipped by a DST change.
        let mut day = Tm {
            year: today.year,
            mon: today.mon,
            mday: today.mday + offset,
            hour: 12,
            ..Tm::default()
        };
        tz.mktime(&mut day);
        for p in &rule.periods {
            if !p.matches(&day, tz) {
                continue;
            }
            let at = |minutes: u32| {
                tz.mktime(&mut Tm {
                    year: day.year,
                    mon: day.mon,
                    mday: day.mday,
                    hour: (minutes / 60) as i32,
                    min: (minutes % 60) as i32,
                    ..Tm::default()
                })
            };
            let occ = Occurrence {
                start: at(p.start_minute),
                end: at(p.end_minute),
                fixed: p.fixed,
                duration: p.duration,
            };
            // A window that DST normalisation makes empty does not exist.
            if occ.start >= occ.end {
                continue;
            }
            if occ.start <= now + look_ahead && occ.end > now {
                found.push(occ);
            }
        }
    }
    found.sort();
    found.dedup();
    found
}

/// A rule applied to this engine: its configuration and the occurrences it
/// has handled (`recurring_downtime` object).
#[derive(Debug, Clone)]
pub struct Rule {
    pub conf: RecurringConf,
    /// Occurrences starting before this time are not scheduled.
    pub not_before: i64,
    /// (start, host_id, service_id) -> end of the handled occurrences.
    handled: BTreeMap<(i64, u64, u64), i64>,
}

/// What a rule puts in downtime, in the zone of its occurrences: a host
/// target (`host`) with all its services, or services alone.
#[derive(Debug, Clone)]
pub struct Target<'a> {
    pub host_id: u64,
    pub host: bool,
    pub services: Vec<u64>,
    pub tz: &'a Tz,
}

/// What a run decided for one occurrence of one target.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Due {
    pub host_id: u64,
    pub service_id: u64,
    pub occurrence: Occurrence,
}

impl Rule {
    #[must_use]
    pub fn new(conf: RecurringConf, not_before: i64) -> Self {
        Self {
            conf,
            not_before,
            handled: BTreeMap::new(),
        }
    }

    /// One run (`recurring_downtime::schedule`): the occurrences to
    /// schedule on `targets` (unknown targets left out); `exists` tells a
    /// downtime with the same target, comment and start already exists.
    pub fn run(
        &mut self,
        now: i64,
        targets: &[Target<'_>],
        exists: impl Fn(u64, u64, i64) -> bool,
    ) -> Vec<Due> {
        self.handled.retain(|_, end| *end > now);
        let mut due = Vec::new();
        for target in targets {
            for occ in occurrences(&self.conf, target.tz, now, LOOK_AHEAD) {
                let host = target.host.then_some(0);
                for service_id in host.into_iter().chain(target.services.iter().copied()) {
                    if self.take(target.host_id, service_id, &occ, &exists) {
                        due.push(Due {
                            host_id: target.host_id,
                            service_id,
                            occurrence: occ,
                        });
                    }
                }
            }
        }
        due
    }

    /// `_schedule_one`, without the scheduling itself.
    fn take(
        &mut self,
        host_id: u64,
        service_id: u64,
        occ: &Occurrence,
        exists: &impl Fn(u64, u64, i64) -> bool,
    ) -> bool {
        // Handled once: a downtime cancelled since then is not created again.
        if self
            .handled
            .insert((occ.start, host_id, service_id), occ.end)
            .is_some()
        {
            return false;
        }
        if occ.start < self.not_before {
            tracing::debug!(
                rule = self.conf.id,
                host_id,
                service_id,
                start = occ.start,
                "recurring downtime occurrence started before the engine: not scheduled"
            );
            return false;
        }
        // SCHEDULE_*_DOWNTIME refuses a flexible downtime without duration.
        if !occ.fixed && occ.duration == 0 {
            return false;
        }
        !exists(host_id, service_id, occ.start)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::tz::tests::PARIS;

    fn rule(periods: &[&str]) -> RecurringConf {
        RecurringConf {
            id: 7,
            name: "r".into(),
            timezone: String::new(),
            host_ids: vec![1],
            service_ids: vec![],
            periods: periods.iter().map(|p| Period::parse(p).unwrap()).collect(),
        }
    }

    fn at(tz: &Tz, y: i32, mo: i32, d: i32, h: i32, mi: i32) -> i64 {
        tz.mktime(&mut Tm {
            year: y - 1900,
            mon: mo - 1,
            mday: d,
            hour: h,
            min: mi,
            ..Tm::default()
        })
    }

    #[test]
    fn parses_periods_as_the_helper() {
        let p = Period::parse("weekly:1,7 22:00-24:00 flexible:3600").unwrap();
        assert_eq!(
            (p.days.clone(), p.start_minute, p.end_minute),
            (vec![1, 7], 1320, 1440)
        );
        assert_eq!((p.fixed, p.duration), (false, 3600));
        let p = Period::parse("nth:last:5\t08:00-09:30 fixed").unwrap();
        assert_eq!(
            (p.kind, p.rank, p.days.clone()),
            (PeriodKind::Nth, -1, vec![5])
        );
        for bad in [
            "weekly:8 08:00-09:00 fixed",
            "monthly:0 08:00-09:00 fixed",
            "weekly:1 09:00-08:00 fixed",
            "weekly:1 08:00-24:30 fixed",
            "nth:fifth:1 08:00-09:00 fixed",
            "weekly:1 08:00-09:00",
            "weekly:1 8:0-09:00 fixed",
        ] {
            assert!(Period::parse(bad).is_none(), "{bad}");
        }
    }

    #[test]
    fn occurrences_of_three_days_across_dst() {
        let tz = Tz::posix(PARIS).unwrap();
        // Every day 02:30-03:30: on 29 March 02:30 does not exist.
        let daily = rule(&["weekly:1,2,3,4,5,6,7 02:30-03:30 fixed"]);
        let now = at(&tz, 2026, 3, 29, 2, 0) - 3600; // 01:00 CET
        let occ = occurrences(&daily, &tz, now, LOOK_AHEAD);
        // mktime moves 02:30 to 03:30: the window is empty, skipped.
        assert!(occ.is_empty(), "{occ:?}");
        let now = at(&tz, 2026, 3, 30, 2, 25);
        let occ = occurrences(&daily, &tz, now, LOOK_AHEAD);
        assert_eq!(occ.len(), 1);
        assert_eq!(occ[0].end - occ[0].start, 3600);
        // Last Friday of the month.
        let last_friday = rule(&["nth:last:5 10:00-11:00 fixed"]);
        let now = at(&tz, 2026, 3, 27, 9, 55);
        assert_eq!(occurrences(&last_friday, &tz, now, LOOK_AHEAD).len(), 1);
        let now = at(&tz, 2026, 3, 20, 9, 55);
        assert!(occurrences(&last_friday, &tz, now, LOOK_AHEAD).is_empty());
        // Running occurrence of yesterday, crossing midnight.
        let night = rule(&["weekly:1,2,3,4,5,6,7 22:00-24:00 fixed"]);
        let now = at(&tz, 2026, 6, 10, 23, 0);
        assert_eq!(occurrences(&night, &tz, now, LOOK_AHEAD).len(), 1);
    }

    #[test]
    fn each_occurrence_is_handled_once_and_never_caught_up() {
        let tz = Tz::utc();
        let conf = rule(&["weekly:1,2,3,4,5,6,7 10:00-11:00 fixed"]);
        let start = at(&tz, 2026, 6, 10, 10, 0);
        // Loaded at start, after the occurrence began: not scheduled.
        let mut loaded = Rule::new(conf.clone(), start + 60);
        let targets = [Target {
            host_id: 1,
            host: true,
            services: vec![5, 6],
            tz: &tz,
        }];
        assert!(
            loaded
                .run(start + 120, &targets, |_, _, _| false)
                .is_empty()
        );
        // Added by a reload: the running occurrence is scheduled, for the
        // host and each of its services, once.
        let mut reloaded = Rule::new(conf.clone(), 0);
        let due = reloaded.run(start + 120, &targets, |_, _, _| false);
        assert_eq!(
            due.iter().map(|d| d.service_id).collect::<Vec<_>>(),
            [0, 5, 6]
        );
        assert!(
            reloaded
                .run(start + 180, &targets, |_, _, _| false)
                .is_empty()
        );
        // Ten minutes ahead, not more; an existing downtime is not doubled.
        let mut fresh = Rule::new(conf, 0);
        assert!(fresh.run(start - 601, &targets, |_, _, _| false).is_empty());
        let due = fresh.run(start - 600, &targets, |_, service, _| service == 5);
        assert_eq!(due.len(), 2);
        // Flexible without duration: never scheduled.
        let mut flexible = Rule::new(rule(&["weekly:3 10:00-11:00 flexible:0"]), 0);
        assert!(flexible.run(start, &targets, |_, _, _| false).is_empty());
    }

    #[test]
    fn from_raw_checks_the_object() {
        let mut object = RawObject::default();
        assert!(
            RecurringConf::from_raw(&object)
                .unwrap_err()
                .contains("no id")
        );
        object.fields.insert("downtime_id".into(), "3".into());
        assert!(
            RecurringConf::from_raw(&object)
                .unwrap_err()
                .contains("no period")
        );
        object
            .lines
            .push(("period".into(), "monthly:1,15 00:00-01:00 fixed".into()));
        assert!(
            RecurringConf::from_raw(&object)
                .unwrap_err()
                .contains("no target")
        );
        object
            .fields
            .insert("service_ids".into(), "4:5, 6:7".into());
        object
            .fields
            .insert("timezone".into(), ":Europe/Paris".into());
        let conf = RecurringConf::from_raw(&object).unwrap();
        assert_eq!(conf.service_ids, [(4, 5), (6, 7)]);
        assert_eq!(conf.timezone, ":Europe/Paris");
        object.fields.insert("host_ids".into(), "x".into());
        assert!(RecurringConf::from_raw(&object).is_err());
        assert_eq!(comment(3), "[Downtime cycle #3]");
    }
}
