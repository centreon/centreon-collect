//! Timeperiods: the `define timeperiod` syntax of
//! `common/engine_conf/timeperiod_helper.cc` and the time computations of
//! `common/inc/com/centreon/common/timeperiod_utils.hh` and
//! `common/src/timeperiod_utils.cc` (next valid time, exceptions,
//! exclusions), in a given time zone. Pure: the clock and the zone are
//! arguments. `-1` is `(time_t)-1` there: no time.

use std::collections::HashMap;

use crate::tz::{Tm, Tz};

const NONE: i64 = -1;
const DAY: i64 = 24 * 60 * 60;

/// A range of one day, in seconds from midnight (`24:00` is 86400).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Timerange {
    pub start: i64,
    pub end: i64,
}

/// Exception types, in their order of precedence (`Daterange::TypeRange`).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DateKind {
    /// `2026-12-25`, `2026-12-24 - 2026-12-26 / 2`.
    CalendarDate = 0,
    /// `december 25`, `february 1 - march 15`.
    MonthDate = 1,
    /// `day 1`, `day 1 - 15`, `day -1` (last day).
    MonthDay = 2,
    /// `monday 1 january`, `monday 1 january - friday -1 january`.
    MonthWeekDay = 3,
    /// `monday 2`, `monday -1`, `monday 1 - thursday 3`.
    WeekDay = 4,
}

/// One exception line (`configuration::Daterange`); months 0 to 11,
/// weekdays 0 (Sunday) to 6.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Daterange {
    pub kind: DateKind,
    pub syear: i32,
    pub smon: i32,
    pub smday: i32,
    pub swday: i32,
    pub swday_offset: i32,
    pub eyear: i32,
    pub emon: i32,
    pub emday: i32,
    pub ewday: i32,
    pub ewday_offset: i32,
    pub skip_interval: i64,
    pub timeranges: Vec<Timerange>,
}

impl Daterange {
    fn new(kind: DateKind) -> Self {
        Self {
            kind,
            syear: 0,
            smon: 0,
            smday: 0,
            swday: 0,
            swday_offset: 0,
            eyear: 0,
            emon: 0,
            emday: 0,
            ewday: 0,
            ewday_offset: 0,
            skip_interval: 0,
            timeranges: Vec::new(),
        }
    }
}

/// A resolved timeperiod.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Timeperiod {
    /// `timeperiod_name`.
    pub name: String,
    /// `alias`.
    pub alias: String,
    /// Ranges of each weekday, Sunday first.
    pub days: [Vec<Timerange>; 7],
    /// Exceptions by [`DateKind`].
    pub exceptions: [Vec<Daterange>; 5],
    /// Names of the excluded timeperiods.
    pub exclude: Vec<String>,
}

const DAYS: [&str; 7] = [
    "sunday",
    "monday",
    "tuesday",
    "wednesday",
    "thursday",
    "friday",
    "saturday",
];
const MONTHS: [&str; 12] = [
    "january",
    "february",
    "march",
    "april",
    "may",
    "june",
    "july",
    "august",
    "september",
    "october",
    "november",
    "december",
];

fn day_id(name: &str) -> Option<i32> {
    DAYS.iter().position(|d| *d == name).map(|i| i as i32)
}

fn month_id(name: &str) -> Option<i32> {
    MONTHS.iter().position(|m| *m == name).map(|i| i as i32)
}

/// `absl::SimpleAtoi`: surrounding blanks allowed.
fn atoi(text: &str) -> Option<i64> {
    text.trim().parse().ok()
}

/// `_build_time_t`: `HH:MM` in seconds.
fn time_of(text: &str) -> Option<i64> {
    let (hours, minutes) = text.split_once(':')?;
    let (hours, minutes) = (atoi(hours)?, atoi(minutes)?);
    (hours >= 0 && minutes >= 0).then_some(hours * 3600 + minutes * 60)
}

/// `_build_timeranges`: `HH:MM-HH:MM[, ...]`.
fn timeranges(text: &str) -> Option<Vec<Timerange>> {
    text.split(',')
        .map(|range| {
            let range = range.trim();
            let (start, end) = range.split_once('-')?;
            Some(Timerange {
                start: time_of(start)?,
                end: time_of(end)?,
            })
        })
        .collect()
}

/// Records the exception of a line: its kind and how to fill it.
type Setter<'a> = dyn FnMut(DateKind, &dyn Fn(&mut Daterange)) + 'a;

/// A value read by [`scan`].
#[derive(Debug, Clone)]
enum Scanned {
    Int(i64),
    Word(String),
}

/// The subset of `sscanf` the timeperiod helper uses: ` ` skips blanks,
/// `%u`, `%4u`, `%2u`, `%d`, `%[a-z]`, `%n`, other characters match
/// themselves. Returns the conversions done before the first mismatch and
/// the position of `%n`.
fn scan(input: &str, format: &str) -> (Vec<Scanned>, usize) {
    let bytes = input.as_bytes();
    let fmt = format.as_bytes();
    let (mut i, mut f) = (0usize, 0usize);
    let mut values = Vec::new();
    let mut position = 0;
    let skip_blanks = |i: &mut usize| {
        while *i < bytes.len() && bytes[*i].is_ascii_whitespace() {
            *i += 1;
        }
    };
    while f < fmt.len() {
        match fmt[f] {
            b' ' => {
                skip_blanks(&mut i);
                f += 1;
            }
            b'%' => {
                let rest = &format[f + 1..];
                if let Some(after) = rest.strip_prefix("[a-z]") {
                    let len = bytes[i..]
                        .iter()
                        .take_while(|c| c.is_ascii_lowercase())
                        .count();
                    if len == 0 {
                        break;
                    }
                    values.push(Scanned::Word(input[i..i + len].to_owned()));
                    i += len;
                    f = fmt.len() - after.len();
                } else if rest.starts_with('n') {
                    position = i;
                    f += 2;
                } else {
                    // %[width]u or %d
                    let width_len = rest.bytes().take_while(u8::is_ascii_digit).count();
                    let width: usize = rest[..width_len].parse().unwrap_or(usize::MAX);
                    let conversion = rest.as_bytes()[width_len];
                    skip_blanks(&mut i);
                    let start = i;
                    if conversion == b'd'
                        && i < bytes.len()
                        && (bytes[i] == b'-' || bytes[i] == b'+')
                    {
                        i += 1;
                    }
                    let digits = bytes[i..]
                        .iter()
                        .take(width.saturating_sub(i - start))
                        .take_while(|c| c.is_ascii_digit())
                        .count();
                    if digits == 0 {
                        break;
                    }
                    i += digits;
                    let Ok(value) = input[start..i].parse::<i64>() else {
                        break;
                    };
                    values.push(Scanned::Int(value));
                    f += 1 + width_len + 1;
                }
            }
            c => {
                if i < bytes.len() && bytes[i] == c {
                    i += 1;
                    f += 1;
                } else {
                    break;
                }
            }
        }
    }
    (values, position)
}

fn int(values: &[Scanned], index: usize) -> i64 {
    match values.get(index) {
        Some(Scanned::Int(value)) => *value,
        _ => 0,
    }
}

fn word(values: &[Scanned], index: usize) -> &str {
    match values.get(index) {
        Some(Scanned::Word(value)) => value,
        _ => "",
    }
}

impl Timeperiod {
    /// One `key value` line of a `define timeperiod` (other than
    /// `timeperiod_name`, `alias`, `exclude`), as
    /// `timeperiod_helper::hook`: a weekday, else an exception.
    pub fn add_line(&mut self, key: &str, value: &str) -> Result<(), String> {
        let invalid = || format!("invalid timeperiod line '{key} {value}'");
        if let Some(day) = day_id(key) {
            // The weekday ranges, then the same line as a weekday exception
            // (`monday 1 00:00-...`) when they do not parse.
            if let Some(ranges) = timeranges(value) {
                self.days[day as usize].extend(ranges);
                return Ok(());
            }
        }
        let line = format!("{key} {value}");
        if self.add_calendar_date(&line) || self.add_other_date(&line) {
            Ok(())
        } else {
            Err(invalid())
        }
    }

    /// `_add_calendar_date`.
    fn add_calendar_date(&mut self, line: &str) -> bool {
        let formats: [(&str, usize, bool); 4] = [
            ("%4u-%2u-%2u - %4u-%2u-%2u / %u %n", 7, false),
            ("%4u-%2u-%2u - %4u-%2u-%2u %n", 6, false),
            ("%4u-%2u-%2u / %u %n", 4, true),
            ("%4u-%2u-%2u %n", 3, true),
        ];
        for (format, count, fill_missing) in formats {
            let (values, pos) = scan(line, format);
            if values.len() != count {
                continue;
            }
            let mut range = Daterange::new(DateKind::CalendarDate);
            range.syear = int(&values, 0) as i32;
            range.smon = int(&values, 1) as i32 - 1;
            range.smday = int(&values, 2) as i32;
            if fill_missing {
                range.eyear = range.syear;
                range.emon = range.smon;
                range.emday = range.smday;
                range.skip_interval = if count == 4 { int(&values, 3) } else { 0 };
            } else {
                range.eyear = int(&values, 3) as i32;
                range.emon = int(&values, 4) as i32 - 1;
                range.emday = int(&values, 5) as i32;
                range.skip_interval = if count == 7 { int(&values, 6) } else { 0 };
            }
            let Some(ranges) = timeranges(&line[pos..]) else {
                return false;
            };
            range.timeranges = ranges;
            self.exceptions[DateKind::CalendarDate as usize].push(range);
            return true;
        }
        false
    }

    /// `_add_other_date`: the same formats in the same order.
    fn add_other_date(&mut self, line: &str) -> bool {
        if line.len() > 1024 {
            return false;
        }
        let mut range: Option<Daterange> = None;
        let mut pos = 0;
        let mut set = |kind: DateKind, fill: &dyn Fn(&mut Daterange)| {
            let mut r = Daterange::new(kind);
            fill(&mut r);
            range = Some(r);
        };
        let try_scan = |format: &str, count: usize| {
            let (values, at) = scan(line, format);
            (values.len() == count).then_some((values, at))
        };
        if let Some((v, at)) = try_scan("%[a-z] %d %[a-z] - %[a-z] %d %[a-z] / %u %n", 7) {
            pos = at;
            // wednesday 1 january - thursday 2 july / 3
            if let (Some(sd), Some(sm), Some(ed), Some(em)) = (
                day_id(word(&v, 0)),
                month_id(word(&v, 2)),
                day_id(word(&v, 3)),
                month_id(word(&v, 5)),
            ) {
                set(DateKind::MonthWeekDay, &|r| {
                    r.swday = sd;
                    r.swday_offset = int(&v, 1) as i32;
                    r.smon = sm;
                    r.ewday = ed;
                    r.ewday_offset = int(&v, 4) as i32;
                    r.emon = em;
                    r.skip_interval = int(&v, 6);
                });
            }
        } else if let Some((v, at)) = try_scan("%[a-z] %d - %[a-z] %d / %u %n", 5) {
            pos = at;
            self.two_names(&v, int(&v, 4), &mut set);
        } else if let Some((v, at)) = try_scan("%[a-z] %d - %d / %u %n", 4) {
            pos = at;
            Self::one_name(&v, int(&v, 3), &mut set);
        } else if let Some((v, at)) = try_scan("%[a-z] %d %[a-z] - %[a-z] %d %[a-z] %n", 6) {
            pos = at;
            if let (Some(sd), Some(sm), Some(ed), Some(em)) = (
                day_id(word(&v, 0)),
                month_id(word(&v, 2)),
                day_id(word(&v, 3)),
                month_id(word(&v, 5)),
            ) {
                set(DateKind::MonthWeekDay, &|r| {
                    r.swday = sd;
                    r.swday_offset = int(&v, 1) as i32;
                    r.smon = sm;
                    r.ewday = ed;
                    r.ewday_offset = int(&v, 4) as i32;
                    r.emon = em;
                });
            }
        } else if let Some((v, at)) = try_scan("%[a-z] %d - %d %n", 3) {
            pos = at;
            Self::one_name(&v, 0, &mut set);
        } else if let Some((v, at)) = try_scan("%[a-z] %d - %[a-z] %d %n", 4) {
            pos = at;
            self.two_names(&v, 0, &mut set);
        } else if let Some((v, at)) = try_scan("%[a-z] %d %[a-z] %n", 3) {
            pos = at;
            // thursday 3 february
            if let (Some(d), Some(m)) = (day_id(word(&v, 0)), month_id(word(&v, 2))) {
                set(DateKind::MonthWeekDay, &|r| {
                    r.swday = d;
                    r.swday_offset = int(&v, 1) as i32;
                    r.smon = m;
                    r.ewday = d;
                    r.ewday_offset = int(&v, 1) as i32;
                    r.emon = m;
                });
            }
        } else if let Some((v, at)) = try_scan("%[a-z] %d %n", 2) {
            pos = at;
            let n = int(&v, 1) as i32;
            if let Some(d) = day_id(word(&v, 0)) {
                // thursday 2
                set(DateKind::WeekDay, &|r| {
                    r.swday = d;
                    r.swday_offset = n;
                    r.ewday = d;
                    r.ewday_offset = n;
                });
            } else if let Some(m) = month_id(word(&v, 0)) {
                // february 3
                set(DateKind::MonthDate, &|r| {
                    r.smon = m;
                    r.smday = n;
                    r.emon = m;
                    r.emday = n;
                });
            } else if word(&v, 0) == "day" {
                // day 1
                set(DateKind::MonthDay, &|r| {
                    r.smday = n;
                    r.emday = n;
                });
            }
        }
        let Some(mut range) = range else {
            return false;
        };
        let Some(ranges) = timeranges(&line[pos..]) else {
            return false;
        };
        range.timeranges = ranges;
        self.exceptions[range.kind as usize].push(range);
        true
    }

    /// `name N - name M`: two weekdays, two months or `day`/`day`.
    fn two_names(&self, v: &[Scanned], skip: i64, set: &mut Setter<'_>) {
        let (a, b) = (int(v, 1) as i32, int(v, 3) as i32);
        if let (Some(sd), Some(ed)) = (day_id(word(v, 0)), day_id(word(v, 2))) {
            // monday 2 - thursday 3
            set(DateKind::WeekDay, &|r| {
                r.swday = sd;
                r.swday_offset = a;
                r.ewday = ed;
                r.ewday_offset = b;
                r.skip_interval = skip;
            });
        } else if let (Some(sm), Some(em)) = (month_id(word(v, 0)), month_id(word(v, 2))) {
            // february 1 - march 15
            set(DateKind::MonthDate, &|r| {
                r.smon = sm;
                r.smday = a;
                r.emon = em;
                r.emday = b;
                r.skip_interval = skip;
            });
        } else if word(v, 0) == "day" && word(v, 2) == "day" {
            // day 1 - day 5
            set(DateKind::MonthDay, &|r| {
                r.smday = a;
                r.emday = b;
                r.skip_interval = skip;
            });
        }
    }

    /// `name N - M`: one weekday, one month or `day`.
    fn one_name(v: &[Scanned], skip: i64, set: &mut Setter<'_>) {
        let (a, b) = (int(v, 1) as i32, int(v, 2) as i32);
        if let Some(d) = day_id(word(v, 0)) {
            // thursday 2 - 4
            set(DateKind::WeekDay, &|r| {
                r.swday = d;
                r.swday_offset = a;
                r.ewday = d;
                r.ewday_offset = b;
                r.skip_interval = skip;
            });
        } else if let Some(m) = month_id(word(v, 0)) {
            // february 3 - 5
            set(DateKind::MonthDate, &|r| {
                r.smon = m;
                r.smday = a;
                r.emon = m;
                r.emday = b;
                r.skip_interval = skip;
            });
        } else if word(v, 0) == "day" {
            // day 1 - 4
            set(DateKind::MonthDay, &|r| {
                r.smday = a;
                r.emday = b;
                r.skip_interval = skip;
            });
        }
    }

    /// Merges template `tmpl` as `parser::_merge` does for a timeperiod:
    /// weekdays the object leaves empty, exceptions it does not have,
    /// exclusions when it has none.
    pub fn merge(&mut self, tmpl: &Self) {
        for (day, from) in self.days.iter_mut().zip(&tmpl.days) {
            if day.is_empty() {
                day.clone_from(from);
            }
        }
        for (list, from) in self.exceptions.iter_mut().zip(&tmpl.exceptions) {
            for range in from {
                if !list.contains(range) {
                    list.push(range.clone());
                }
            }
        }
        if self.exclude.is_empty() {
            self.exclude.clone_from(&tmpl.exclude);
        }
        if self.alias.is_empty() {
            self.alias.clone_from(&tmpl.alias);
        }
    }
}

/// Time information of one step (`detail::time_info`).
struct TimeInfo {
    preferred_time: i64,
    preftime: Tm,
    midnight: i64,
}

/// The computations on the timeperiods of one configuration, in zone `tz`.
pub struct Calc<'a> {
    pub tz: &'a Tz,
    pub periods: &'a HashMap<String, Timeperiod>,
}

impl Calc<'_> {
    fn local(&self, t: i64) -> Tm {
        self.tz.localtime(t)
    }

    fn mktime(&self, tm: &mut Tm) -> i64 {
        self.tz.mktime(tm)
    }

    /// `add_round_days_to_midnight`.
    fn add_round_days_to_midnight(&self, mut midnight: i64, skip: i64) -> i64 {
        let mut t = self.local(midnight);
        if t.hour != 0 || t.min != 0 || t.sec != 0 {
            t.hour = 0;
            t.min = 0;
            t.sec = 0;
            midnight = self.mktime(&mut t);
        }
        let mut next_day_time = midnight + skip;
        let next_day = self.local(next_day_time);
        if next_day.hour != 0 || next_day.min != 0 || next_day.sec != 0 {
            next_day_time += 12 * 60 * 60;
            let mut next_day = self.local(next_day_time);
            next_day.hour = 0;
            next_day.min = 0;
            next_day.sec = 0;
            next_day_time = self.mktime(&mut next_day);
        }
        next_day_time
    }

    /// `calculate_time_from_day_of_month`.
    fn time_from_day_of_month(&self, year: i32, month: i32, monthday: i32) -> i64 {
        let mut t = Tm::default();
        if monthday > 0 {
            t.year = year;
            t.mon = month;
            t.mday = monthday;
            let r = self.mktime(&mut t);
            return if t.mon == month { r } else { NONE };
        }
        let mut day = 32;
        loop {
            day -= 1;
            t = Tm {
                year,
                mon: month,
                mday: day,
                ..Tm::default()
            };
            self.mktime(&mut t);
            if t.mon == month {
                break;
            }
        }
        if -monthday >= t.mday {
            t.mday = 1;
        } else {
            t.mday += monthday + 1;
        }
        self.mktime(&mut t)
    }

    /// `calculate_time_from_weekday_of_month`.
    fn time_from_weekday_of_month(&self, year: i32, month: i32, weekday: i32, offset: i32) -> i64 {
        let mut t = Tm {
            year,
            mon: month,
            mday: 1,
            ..Tm::default()
        };
        self.mktime(&mut t);
        let mut days = weekday - t.wday;
        if days < 0 {
            days += 7;
        }
        if offset > 0 {
            let weeks = offset.min(5);
            days += (weeks - 1) * 7;
            t = Tm {
                year,
                mon: month,
                mday: days + 1,
                ..Tm::default()
            };
            let r = self.mktime(&mut t);
            return if t.mon == month { r } else { NONE };
        }
        days += 5 * 7;
        loop {
            days -= 7;
            t = Tm {
                year,
                mon: month,
                mday: days + 1,
                ..Tm::default()
            };
            self.mktime(&mut t);
            if t.mon == month {
                break;
            }
        }
        let days = (offset + 1) * 7;
        t.mon = month;
        t.year = year;
        if -days >= t.mday {
            t.mday %= 7;
        } else {
            t.mday += days;
        }
        self.mktime(&mut t)
    }

    fn calendar_date(&self, r: &Daterange) -> Option<(i64, i64)> {
        let mut t = Tm {
            year: r.syear - 1900,
            mon: r.smon,
            mday: r.smday,
            ..Tm::default()
        };
        let start = self.mktime(&mut t);
        let end = if r.eyear != 0 {
            let mut t = Tm {
                year: r.eyear - 1900,
                mon: r.emon,
                mday: r.emday,
                ..Tm::default()
            };
            let end = self.mktime(&mut t);
            self.add_round_days_to_midnight(end, DAY)
        } else {
            NONE
        };
        Some((start, end))
    }

    fn month_date(&self, r: &Daterange, ti: &TimeInfo) -> Option<(i64, i64)> {
        let end_before_start = r.smon > r.emon || (r.smon == r.emon && r.smday > r.emday);
        let first_year = if end_before_start {
            ti.preftime.year - 1
        } else {
            ti.preftime.year
        };
        let (mut start, mut end) = (NONE, NONE);
        for year in first_year..first_year + 3 {
            start = self.time_from_day_of_month(year, r.smon, r.smday);
            end = self.time_from_day_of_month(year + i32::from(end_before_start), r.emon, r.emday);
            if end != NONE {
                end = self.add_round_days_to_midnight(end, DAY);
                if ti.preferred_time < end {
                    break;
                }
            }
        }
        (start < end).then_some((start, end))
    }

    fn month_day(&self, r: &Daterange, ti: &TimeInfo) -> Option<(i64, i64)> {
        let decay = if r.smday >= 0 {
            r.emday >= 0 && r.emday < r.smday
        } else {
            r.emday < 0
        };
        let (year, mon) = (ti.preftime.year, ti.preftime.mon);
        let (start, mut end);
        if decay {
            let (py, pm) = if mon == 0 {
                (year - 1, 11)
            } else {
                (year, mon - 1)
            };
            let mut s = self.time_from_day_of_month(py, pm, r.smday);
            let mut e = self.time_from_day_of_month(year, mon, r.emday);
            if s == NONE || e == NONE {
                return None;
            }
            e = self.add_round_days_to_midnight(e, DAY);
            if ti.preferred_time >= e {
                let (ny, nm) = if mon == 11 {
                    (year + 1, 0)
                } else {
                    (year, mon + 1)
                };
                s = self.time_from_day_of_month(year, mon, r.smday);
                e = self.time_from_day_of_month(ny, nm, r.emday);
                if s == NONE || e == NONE {
                    return None;
                }
                e = self.add_round_days_to_midnight(e, DAY);
            }
            start = s;
            end = e;
        } else {
            start = self.time_from_day_of_month(year, mon, r.smday);
            end = self.time_from_day_of_month(year, mon, r.emday);
            if start == NONE || end == NONE {
                return None;
            }
            end = self.add_round_days_to_midnight(end, DAY);
        }
        (start < end).then_some((start, end))
    }

    fn month_week_day(&self, r: &Daterange, ti: &TimeInfo) -> Option<(i64, i64)> {
        let year = ti.preftime.year;
        let at = |y: i32, m: i32, d: i32, o: i32| self.time_from_weekday_of_month(y, m, d, o);
        let (mut start, mut end);
        if r.smon > r.emon {
            start = at(year - 1, r.smon, r.swday, r.swday_offset);
            end = at(year, r.emon, r.ewday, r.ewday_offset);
            if start == NONE || end == NONE {
                return None;
            }
            end = self.add_round_days_to_midnight(end, DAY);
            if ti.preferred_time >= end {
                start = at(year, r.smon, r.swday, r.swday_offset);
                end = at(year + 1, r.emon, r.ewday, r.ewday_offset);
                if start == NONE || end == NONE {
                    return None;
                }
                end = self.add_round_days_to_midnight(end, DAY);
            }
        } else {
            start = at(year, r.smon, r.swday, r.swday_offset);
            end = at(year, r.emon, r.ewday, r.ewday_offset);
            if start == NONE || end == NONE {
                return None;
            }
            end = self.add_round_days_to_midnight(end, DAY);
        }
        (start < end).then_some((start, end))
    }

    fn week_day(&self, r: &Daterange, ti: &TimeInfo) -> Option<(i64, i64)> {
        let (mut year, mut month) = (ti.preftime.year, ti.preftime.mon);
        // Bounded where the C++ loops: a month at most past the preferred one.
        for _ in 0..24 {
            let start = self.time_from_weekday_of_month(year, month, r.swday, r.swday_offset);
            let mut end = self.time_from_weekday_of_month(year, month, r.ewday, r.ewday_offset);
            if end == NONE {
                if r.ewday_offset < 0 {
                    return None;
                }
                let (ey, em) = if month == 11 {
                    (year + 1, 0)
                } else {
                    (year, month + 1)
                };
                end = self.time_from_day_of_month(ey, em, 0);
            } else {
                end = self.add_round_days_to_midnight(end, DAY);
            }
            if start == NONE || end == NONE || start > end {
                return None;
            }
            if ti.preferred_time < end {
                return Some((start, end));
            }
            month = ti.preftime.mon;
            if month == 11 {
                month = 0;
                year += 1;
            } else {
                month += 1;
            }
        }
        None
    }

    /// `daterange_to_time_t`.
    fn daterange_to_time_t(&self, r: &Daterange, ti: &TimeInfo) -> Option<(i64, i64)> {
        let (mut start, end) = match r.kind {
            DateKind::CalendarDate => self.calendar_date(r)?,
            DateKind::MonthDate => self.month_date(r, ti)?,
            DateKind::MonthDay => self.month_day(r, ti)?,
            DateKind::MonthWeekDay => self.month_week_day(r, ti)?,
            DateKind::WeekDay => self.week_day(r, ti)?,
        };
        if r.skip_interval > 1 && start < ti.preferred_time {
            let days = (ti.midnight - start).max(0) / DAY;
            if days % r.skip_interval == 0 {
                start = self.add_round_days_to_midnight(start, days * DAY);
            } else {
                start = self.add_round_days_to_midnight(
                    start,
                    (days - days % r.skip_interval + r.skip_interval) * DAY,
                );
            }
        }
        Some((start, end))
    }

    /// `earliest_midnight_in_daterange`.
    fn earliest_midnight_in_daterange(
        &self,
        pref: i64,
        dr: &Daterange,
        mut dr_start: i64,
        dr_end: i64,
    ) -> i64 {
        while dr_start < dr_end || dr_end == NONE {
            let next_day = self.add_round_days_to_midnight(dr_start, DAY);
            if pref < dr_start || pref < next_day {
                return dr_start;
            }
            dr_start = if dr.skip_interval <= 1 {
                next_day
            } else {
                self.add_round_days_to_midnight(dr_start, dr.skip_interval * DAY)
            };
        }
        NONE
    }

    /// `timerange_to_time_t`.
    fn timerange_to_time_t(&self, tr: &Timerange, midnight: &Tm) -> Option<(i64, i64)> {
        let mut t = *midnight;
        t.hour = (tr.start / 3600) as i32;
        t.min = ((tr.start / 60) % 60) as i32;
        t.sec = 0;
        let start = self.mktime(&mut t);
        let mut t = *midnight;
        t.hour = (tr.end / 3600) as i32;
        t.min = ((tr.end / 60) % 60) as i32;
        t.sec = 0;
        let end = self.mktime(&mut t);
        (start <= end).then_some((start, end))
    }

    /// `get_next_valid_time_in_timeranges`.
    fn next_valid_in_timeranges(&self, pref: i64, ranges: &[Timerange]) -> i64 {
        let mut earliest = NONE;
        let mut midnight = self.local(pref);
        midnight.hour = 0;
        midnight.min = 0;
        midnight.sec = 0;
        for tr in ranges {
            let Some((start, end)) = self.timerange_to_time_t(tr, &midnight) else {
                continue;
            };
            if start >= pref {
                if earliest == NONE || start < earliest {
                    earliest = start;
                }
            } else if pref < end {
                return pref;
            }
        }
        earliest
    }

    fn time_info(&self, preferred_time: i64) -> TimeInfo {
        let mut preftime = self.local(preferred_time);
        preftime.sec = 0;
        preftime.min = 0;
        preftime.hour = 0;
        let midnight = self.mktime(&mut preftime);
        TimeInfo {
            preferred_time,
            preftime,
            midnight,
        }
    }

    /// `get_next_invalid_time_per_timeperiod`.
    fn next_invalid(&self, tp: &Timeperiod, mut preferred_time: i64, depth: u32) -> i64 {
        let orig = preferred_time;
        let mut earliest = preferred_time;
        let limit = preferred_time + 366 * DAY;
        while earliest != NONE && preferred_time < limit {
            preferred_time = earliest;
            earliest = NONE;
            let ti = self.time_info(preferred_time);
            for list in &tp.exceptions {
                if earliest != NONE {
                    break;
                }
                for dr in list {
                    if earliest != NONE {
                        break;
                    }
                    let Some((ds, de)) = self.daterange_to_time_t(dr, &ti) else {
                        continue;
                    };
                    if de != NONE && preferred_time >= de {
                        continue;
                    }
                    let em = self.earliest_midnight_in_daterange(preferred_time, dr, ds, de);
                    if em == NONE {
                        continue;
                    }
                    let mid = self.local(em);
                    for tr in &dr.timeranges {
                        if let Some((start, end)) = self.timerange_to_time_t(tr, &mid)
                            && preferred_time >= start
                            && preferred_time < end
                        {
                            earliest = end;
                        }
                    }
                }
            }
            let mut weekday = ti.preftime.wday;
            let mut days_into_future = 0;
            while days_into_future <= 7 && earliest == NONE {
                if weekday >= 7 {
                    weekday -= 7;
                }
                let day_start =
                    self.add_round_days_to_midnight(ti.midnight, days_into_future * DAY);
                let day_midnight = self.local(day_start);
                for tr in &tp.days[weekday as usize] {
                    if let Some((start, end)) = self.timerange_to_time_t(tr, &day_midnight)
                        && preferred_time >= start
                        && preferred_time < end
                    {
                        earliest = end;
                    }
                }
                weekday += 1;
                days_into_future += 1;
            }
            let mut next_exclusion = NONE;
            for name in &tp.exclude {
                let Some(etp) = self.periods.get(name) else {
                    continue;
                };
                let valid = self.next_valid_impl(etp, preferred_time, false, depth + 1);
                if valid != NONE && (next_exclusion == NONE || valid < next_exclusion) {
                    next_exclusion = valid;
                }
            }
            let next_day = self.add_round_days_to_midnight(ti.midnight, DAY);
            if next_exclusion != NONE
                && next_exclusion < next_day
                && (earliest == NONE || next_exclusion <= earliest)
            {
                earliest = NONE;
                preferred_time = next_exclusion;
                break;
            }
            if earliest != NONE {
                preferred_time = earliest;
                earliest = NONE;
            }
        }
        if earliest != NONE {
            orig
        } else {
            preferred_time
        }
    }

    /// `get_next_valid_time_per_timeperiod_impl`. `depth` bounds the
    /// recursion of exclusions that exclude each other.
    fn next_valid_impl(
        &self,
        tp: &Timeperiod,
        preferred_time: i64,
        notif: bool,
        depth: u32,
    ) -> i64 {
        if depth > 8 {
            return if notif { NONE } else { preferred_time };
        }
        let orig = preferred_time;
        let mut earliest = NONE;
        let mut pref = preferred_time;
        let limit = preferred_time + 366 * DAY;
        while earliest == NONE && pref < limit {
            let ti = self.time_info(pref);
            let mut skip_day = false;
            for list in &tp.exceptions {
                if earliest != NONE || skip_day {
                    break;
                }
                for dr in list {
                    let Some((ds, de)) = self.daterange_to_time_t(dr, &ti) else {
                        continue;
                    };
                    if ds != NONE && ds > ti.midnight {
                        continue;
                    }
                    if de != NONE && ti.midnight >= de {
                        continue;
                    }
                    let potential =
                        self.next_valid_in_timeranges(ti.preferred_time, &dr.timeranges);
                    if potential != NONE {
                        if earliest == NONE || potential < earliest {
                            earliest = potential;
                        }
                        skip_day = false;
                    } else {
                        skip_day = true;
                        break;
                    }
                }
            }
            if !skip_day && earliest == NONE {
                let potential = self.next_valid_in_timeranges(
                    ti.preferred_time,
                    &tp.days[ti.preftime.wday as usize],
                );
                if potential != NONE {
                    earliest = potential;
                }
            }
            let mut skipped = false;
            if earliest != NONE {
                let mut max_invalid = NONE;
                for name in &tp.exclude {
                    let Some(etp) = self.periods.get(name) else {
                        continue;
                    };
                    let invalid = self.next_invalid(etp, earliest, depth + 1);
                    if invalid != NONE && (max_invalid == NONE || invalid > max_invalid) {
                        max_invalid = invalid;
                    }
                }
                if max_invalid != NONE && max_invalid != earliest {
                    earliest = NONE;
                    pref = max_invalid;
                    skipped = true;
                }
            }
            if !skipped {
                pref = self.add_round_days_to_midnight(ti.midnight, DAY);
            }
        }
        if earliest == NONE && !notif {
            orig
        } else {
            earliest
        }
    }

    /// `check_time_against_period`: `t` is in the period.
    #[must_use]
    pub fn contains(&self, t: i64, tp: Option<&Timeperiod>) -> bool {
        let Some(tp) = tp else {
            return true;
        };
        self.next_valid_impl(tp, t, false, 0) == t
    }

    /// `get_next_valid_time`: the first time of the period at or after
    /// `max(pref, now)`; that time itself when the period has none.
    #[must_use]
    pub fn next_valid(&self, pref: i64, now: i64, tp: Option<&Timeperiod>) -> i64 {
        let preferred = pref.max(now);
        match tp {
            None => preferred,
            Some(tp) => self.next_valid_impl(tp, preferred, false, 0),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::tz::tests::PARIS;

    fn period(name: &str, lines: &[(&str, &str)]) -> Timeperiod {
        let mut tp = Timeperiod {
            name: name.to_owned(),
            alias: name.to_owned(),
            ..Timeperiod::default()
        };
        for (key, value) in lines {
            tp.add_line(key, value).unwrap();
        }
        tp
    }

    /// Epoch of a Paris wall time.
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
    fn parses_the_engine_syntax() {
        let tp = period(
            "p",
            &[
                ("monday", "09:00-12:00,14:00-18:00"),
                ("2026-12-25", "00:00-24:00"),
                ("2026-12-24", "- 2026-12-26 / 2 08:00-09:00"),
                ("december", "25 00:00-01:00"),
                ("february", "1 - march 15 / 3 00:00-24:00"),
                ("day", "-1 00:00-24:00"),
                ("day", "1 - 15 10:00-11:00"),
                ("monday", "1 january 00:00-24:00"),
                ("monday", "-1 may - friday 2 june 00:00-24:00"),
                ("thursday", "-1 00:00-24:00"),
                ("monday", "2 - thursday 3 00:00-24:00"),
            ],
        );
        assert_eq!(
            tp.days[1],
            [
                Timerange {
                    start: 9 * 3600,
                    end: 12 * 3600
                },
                Timerange {
                    start: 14 * 3600,
                    end: 18 * 3600
                }
            ]
        );
        let calendar = &tp.exceptions[DateKind::CalendarDate as usize];
        assert_eq!(calendar.len(), 2);
        assert_eq!(
            (calendar[0].syear, calendar[0].smon, calendar[0].emday),
            (2026, 11, 25)
        );
        assert_eq!((calendar[1].emday, calendar[1].skip_interval), (26, 2));
        let month_date = &tp.exceptions[DateKind::MonthDate as usize];
        assert_eq!(
            (month_date[1].smon, month_date[1].emon, month_date[1].emday),
            (1, 2, 15)
        );
        assert_eq!(month_date[1].skip_interval, 3);
        let month_day = &tp.exceptions[DateKind::MonthDay as usize];
        assert_eq!((month_day[0].smday, month_day[0].emday), (-1, -1));
        assert_eq!((month_day[1].smday, month_day[1].emday), (1, 15));
        let mwd = &tp.exceptions[DateKind::MonthWeekDay as usize];
        assert_eq!(
            (mwd[1].swday_offset, mwd[1].smon, mwd[1].ewday, mwd[1].emon),
            (-1, 4, 5, 5)
        );
        let wd = &tp.exceptions[DateKind::WeekDay as usize];
        assert_eq!((wd[0].swday, wd[0].swday_offset), (4, -1));
        assert_eq!((wd[1].swday, wd[1].ewday, wd[1].ewday_offset), (1, 4, 3));
        assert!(period("x", &[]).add_line("someday", "00:00-24:00").is_err());
        assert!(period("x", &[]).add_line("monday", "9h-10h").is_err());
    }

    #[test]
    fn next_valid_time_follows_the_week_across_dst() {
        let tz = Tz::posix(PARIS).unwrap();
        let work = period(
            "work",
            &[
                ("monday", "09:00-17:00"),
                ("tuesday", "09:00-17:00"),
                ("saturday", "10:00-11:00"),
            ],
        );
        let periods = HashMap::from([("work".to_owned(), work.clone())]);
        let calc = Calc {
            tz: &tz,
            periods: &periods,
        };
        // Saturday 28 March 2026 noon, CET: next is Monday 09:00, CEST.
        let saturday = at(&tz, 2026, 3, 28, 12, 0);
        let monday = calc.next_valid(saturday, saturday, Some(&work));
        assert_eq!(monday, at(&tz, 2026, 3, 30, 9, 0));
        assert_eq!(monday, 1_774_854_000);
        assert!(!calc.contains(saturday, Some(&work)));
        assert!(calc.contains(monday, Some(&work)));
        assert!(calc.contains(at(&tz, 2026, 3, 28, 10, 30), Some(&work)));
        // In the period: the time itself; never before now.
        let inside = at(&tz, 2026, 3, 31, 16, 59);
        assert_eq!(calc.next_valid(inside, inside, Some(&work)), inside);
        assert_eq!(calc.next_valid(inside - 3600, inside, Some(&work)), inside);
        // Tuesday 17:00 -> Saturday 10:00.
        let evening = at(&tz, 2026, 3, 31, 17, 0);
        assert_eq!(
            calc.next_valid(evening, evening, Some(&work)),
            at(&tz, 2026, 4, 4, 10, 0)
        );
        // No period: always valid.
        assert!(calc.contains(saturday, None));
        // An empty period has no valid time: the preferred one is returned.
        let never = period("never", &[]);
        assert_eq!(calc.next_valid(saturday, saturday, Some(&never)), saturday);
    }

    #[test]
    fn exceptions_take_precedence_and_exclusions_cut() {
        let tz = Tz::posix(PARIS).unwrap();
        let all_week: Vec<(&str, &str)> = DAYS.iter().map(|d| (*d, "00:00-24:00")).collect();
        let mut always = period("24x7", &all_week);
        let holidays = period(
            "holidays",
            &[
                ("december", "25 00:00-24:00"),
                ("2026-05-01", "00:00-24:00"),
            ],
        );
        always.exclude = vec!["holidays".to_owned()];
        let night = period("night", &[("2026-12-24", "20:00-24:00")]);
        let periods = HashMap::from([
            ("24x7".to_owned(), always.clone()),
            ("holidays".to_owned(), holidays.clone()),
            ("night".to_owned(), night.clone()),
        ]);
        let calc = Calc {
            tz: &tz,
            periods: &periods,
        };
        let christmas = at(&tz, 2026, 12, 25, 10, 0);
        assert!(!calc.contains(christmas, Some(&always)));
        assert_eq!(
            calc.next_valid(christmas, christmas, Some(&always)),
            at(&tz, 2026, 12, 26, 0, 0)
        );
        let may_day = at(&tz, 2026, 5, 1, 8, 0);
        assert_eq!(
            calc.next_valid(may_day, may_day, Some(&always)),
            at(&tz, 2026, 5, 2, 0, 0)
        );
        assert!(calc.contains(at(&tz, 2026, 12, 24, 23, 0), Some(&always)));
        // A calendar exception alone: valid that evening only.
        let morning = at(&tz, 2026, 12, 24, 8, 0);
        assert_eq!(
            calc.next_valid(morning, morning, Some(&night)),
            at(&tz, 2026, 12, 24, 20, 0)
        );
        // An exception of the day replaces the weekday ranges.
        let mut work = period(
            "work",
            &[("thursday", "09:00-17:00"), ("2026-12-24", "09:00-12:00")],
        );
        work.name = "work".to_owned();
        let afternoon = at(&tz, 2026, 12, 24, 13, 0);
        assert_eq!(
            calc.next_valid(afternoon, afternoon, Some(&work)),
            at(&tz, 2026, 12, 31, 9, 0)
        );
        // A day of the month, the last Thursday.
        let last = period("last", &[("day", "15 10:00-11:00")]);
        let start = at(&tz, 2026, 2, 3, 0, 0);
        assert_eq!(
            calc.next_valid(start, start, Some(&last)),
            at(&tz, 2026, 2, 15, 10, 0)
        );
        let thursday = period("thursday", &[("thursday", "-1 10:00-11:00")]);
        assert_eq!(
            calc.next_valid(start, start, Some(&thursday)),
            at(&tz, 2026, 2, 26, 10, 0)
        );
    }

    /// `engine/tests/timeperiod/get_next_valid_time/dst_forward.cc`: at
    /// 02:00 of the day the clocks go forward, every kind of range covering
    /// or skipping the missing hour gives 03:00.
    #[test]
    fn engine_dst_forward_cases() {
        let tz = Tz::posix(PARIS).unwrap();
        let periods = HashMap::new();
        let calc = Calc {
            tz: &tz,
            periods: &periods,
        };
        let now = at(&tz, 2017, 3, 26, 1, 59) + 60;
        let expected = at(&tz, 2017, 3, 26, 3, 0);
        assert_eq!(now, expected);
        for days in [
            "2017-03-26",
            "march 26",
            "day 26",
            "sunday 4 march",
            "sunday 4",
        ] {
            for ranges in ["01:00-04:00", "01:00-02:00,03:00-04:00"] {
                let (key, rest) = days.split_once(' ').unwrap_or((days, ""));
                let value = format!("{rest} {ranges}").trim().to_owned();
                let tp = period("dst", &[(key, &value)]);
                assert_eq!(
                    calc.next_valid(now, now, Some(&tp)),
                    expected,
                    "{days} {ranges}"
                );
            }
        }
        for ranges in ["01:00-04:00", "01:00-02:00,03:00-04:00"] {
            let tp = period("dst", &[("sunday", ranges)]);
            assert_eq!(calc.next_valid(now, now, Some(&tp)), expected, "{ranges}");
        }
    }

    #[test]
    fn a_day_of_dst_change_keeps_its_wall_times() {
        let tz = Tz::posix(PARIS).unwrap();
        let early = period("early", &[("sunday", "01:00-04:00")]);
        let periods = HashMap::new();
        let calc = Calc {
            tz: &tz,
            periods: &periods,
        };
        // 29 March 2026: 02:00-03:00 does not exist; the range is 2 h long.
        let midnight = at(&tz, 2026, 3, 29, 0, 0);
        let start = calc.next_valid(midnight, midnight, Some(&early));
        assert_eq!(start, at(&tz, 2026, 3, 29, 1, 0));
        assert!(calc.contains(start + 2 * 3600 - 1, Some(&early)));
        assert!(!calc.contains(start + 2 * 3600, Some(&early)));
        // 25 October 2026: 02:00-03:00 occurs twice; the range is 4 h long.
        let midnight = at(&tz, 2026, 10, 25, 0, 0);
        let start = calc.next_valid(midnight, midnight, Some(&early));
        assert!(calc.contains(start + 4 * 3600 - 1, Some(&early)));
        assert!(!calc.contains(start + 4 * 3600, Some(&early)));
    }

    #[test]
    fn templates_merge_as_engine() {
        let mut tp = period(
            "child",
            &[("monday", "08:00-09:00"), ("december", "25 00:00-24:00")],
        );
        let tmpl = period(
            "parent",
            &[
                ("monday", "10:00-11:00"),
                ("friday", "10:00-11:00"),
                ("december", "25 00:00-24:00"),
                ("january", "1 00:00-24:00"),
            ],
        );
        tp.merge(&tmpl);
        assert_eq!(tp.days[1].len(), 1);
        assert_eq!(tp.days[5].len(), 1);
        assert_eq!(tp.exceptions[DateKind::MonthDate as usize].len(), 2);
    }
}
