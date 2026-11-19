//! Time zones for the timeperiod and recurring downtime computations: the
//! `localtime_r` and `mktime` (with `tm_isdst = -1`) of glibc that Engine
//! calls after switching `TZ` (`engine/src/timezone_manager.cc`), without
//! touching the environment of a multi-threaded process. Zones come from
//! the TZif files of the system (`/usr/share/zoneinfo`) or from a POSIX TZ
//! string, as glibc reads the `TZ` variable.

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::sync::Arc;

/// Broken-down time, as `struct tm`: `year` since 1900, `mon` 0 to 11.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Tm {
    pub year: i32,
    pub mon: i32,
    pub mday: i32,
    pub hour: i32,
    pub min: i32,
    pub sec: i32,
    /// 0 is Sunday.
    pub wday: i32,
    pub yday: i32,
    pub isdst: bool,
}

/// Days since 1970-01-01 of a proleptic Gregorian date (`mon` 1 to 12).
fn days_from_civil(year: i64, mon: i64, day: i64) -> i64 {
    let year = if mon <= 2 { year - 1 } else { year };
    let era = year.div_euclid(400);
    let yoe = year - era * 400;
    let mp = (mon + 9) % 12;
    let doy = (153 * mp + 2) / 5 + day - 1;
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    era * 146_097 + doe - 719_468
}

/// Date of a day number: (year, month 1 to 12, day).
fn civil_from_days(days: i64) -> (i64, i64, i64) {
    let z = days + 719_468;
    let era = z.div_euclid(146_097);
    let doe = z - era * 146_097;
    let yoe = (doe - doe / 1460 + doe / 36_524 - doe / 146_096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let day = doy - (153 * mp + 2) / 5 + 1;
    let mon = if mp < 10 { mp + 3 } else { mp - 9 };
    (
        if mon <= 2 {
            yoe + era * 400 + 1
        } else {
            yoe + era * 400
        },
        mon,
        day,
    )
}

fn is_leap(year: i64) -> bool {
    (year % 4 == 0 && year % 100 != 0) || year % 400 == 0
}

/// Broken-down UTC time of `t` shifted by `offset`.
fn breakdown(t: i64, offset: i32, isdst: bool) -> Tm {
    let local = t + i64::from(offset);
    let days = local.div_euclid(86_400);
    let secs = local.rem_euclid(86_400);
    let (year, mon, day) = civil_from_days(days);
    let yday = days - days_from_civil(year, 1, 1);
    Tm {
        year: (year - 1900) as i32,
        mon: (mon - 1) as i32,
        mday: day as i32,
        hour: (secs / 3600) as i32,
        min: (secs / 60 % 60) as i32,
        sec: (secs % 60) as i32,
        // 1970-01-01 was a Thursday.
        wday: (days + 4).rem_euclid(7) as i32,
        yday: yday as i32,
        isdst,
    }
}

/// Seconds since the epoch of a wall time read as UTC, fields normalized as
/// `mktime` does (month 13 is January of the next year, day 0 the last day
/// of the previous month...).
fn wall_seconds(tm: &Tm) -> i64 {
    let mon = i64::from(tm.mon);
    let year = i64::from(tm.year) + 1900 + mon.div_euclid(12);
    let days = days_from_civil(year, mon.rem_euclid(12) + 1, 1) + i64::from(tm.mday) - 1;
    days * 86_400 + i64::from(tm.hour) * 3600 + i64::from(tm.min) * 60 + i64::from(tm.sec)
}

/// Abbreviation of a local time type (`tm_zone`), at most 8 bytes.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
struct Abbr([u8; 8]);

impl Abbr {
    fn of(text: &[u8]) -> Self {
        let mut abbr = [0u8; 8];
        for (slot, byte) in abbr.iter_mut().zip(text) {
            *slot = *byte;
        }
        Self(abbr)
    }

    fn text(&self) -> String {
        let len = self.0.iter().position(|b| *b == 0).unwrap_or(8);
        String::from_utf8_lossy(&self.0[..len]).into_owned()
    }
}

/// One local time type: offset east of UTC, DST flag and abbreviation.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct Kind {
    offset: i32,
    isdst: bool,
    abbr: Abbr,
}

/// When a POSIX rule switches: `Jn` (1 to 365, no Feb 29), `n` (0 to 365)
/// or `Mm.w.d`, and the local time of the switch in seconds.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Day {
    Julian1(i64),
    Julian0(i64),
    Month { mon: i64, week: i64, wday: i64 },
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct Switch {
    day: Day,
    time: i64,
}

/// A POSIX TZ string: standard time, and DST with its rules.
#[derive(Debug, Clone, PartialEq, Eq)]
struct Posix {
    std: Kind,
    dst: Option<(Kind, Switch, Switch)>,
}

impl Switch {
    /// Local wall seconds (as UTC) of the switch in `year`.
    fn wall(&self, year: i64) -> i64 {
        let jan1 = days_from_civil(year, 1, 1);
        let day = match self.day {
            Day::Julian1(n) => {
                // February 29 is never counted.
                let day = jan1 + n - 1;
                if is_leap(year) && n >= 60 {
                    day + 1
                } else {
                    day
                }
            }
            Day::Julian0(n) => jan1 + n,
            Day::Month { mon, week, wday } => {
                let first = days_from_civil(year, mon, 1);
                let first_wday = (first + 4).rem_euclid(7);
                let mut day = first + (wday - first_wday).rem_euclid(7) + (week - 1) * 7;
                let next_month = if mon == 12 {
                    days_from_civil(year + 1, 1, 1)
                } else {
                    days_from_civil(year, mon + 1, 1)
                };
                while day >= next_month {
                    day -= 7;
                }
                day
            }
        };
        day * 86_400 + self.time
    }
}

impl Posix {
    fn kind_at(&self, t: i64) -> Kind {
        let Some((dst, start, end)) = &self.dst else {
            return self.std;
        };
        let year = civil_from_days((t + i64::from(self.std.offset)).div_euclid(86_400)).0;
        // The year of `t` and its neighbours: a switch near new year counts.
        let mut in_dst = false;
        let mut latest = i64::MIN;
        for y in [year - 1, year, year + 1] {
            let on = start.wall(y) - i64::from(self.std.offset);
            let off = end.wall(y) - i64::from(dst.offset);
            for (at, value) in [(on, true), (off, false)] {
                if at <= t && at > latest {
                    latest = at;
                    in_dst = value;
                }
            }
        }
        if in_dst { *dst } else { self.std }
    }
}

/// Parser of a POSIX TZ string (`CET-1CEST,M3.5.0,M10.5.0/3`).
struct PosixParser<'a> {
    rest: &'a [u8],
}

impl PosixParser<'_> {
    fn name(&mut self) -> Option<Abbr> {
        if self.rest.first() == Some(&b'<') {
            let end = self.rest.iter().position(|c| *c == b'>')?;
            let abbr = Abbr::of(&self.rest[1..end]);
            self.rest = &self.rest[end + 1..];
            return Some(abbr);
        }
        let len = self
            .rest
            .iter()
            .take_while(|c| c.is_ascii_alphabetic())
            .count();
        if len < 3 {
            return None;
        }
        let abbr = Abbr::of(&self.rest[..len]);
        self.rest = &self.rest[len..];
        Some(abbr)
    }

    fn number(&mut self) -> Option<i64> {
        let len = self.rest.iter().take_while(|c| c.is_ascii_digit()).count();
        if len == 0 {
            return None;
        }
        let value = std::str::from_utf8(&self.rest[..len]).ok()?.parse().ok()?;
        self.rest = &self.rest[len..];
        Some(value)
    }

    fn eat(&mut self, c: u8) -> bool {
        if self.rest.first() == Some(&c) {
            self.rest = &self.rest[1..];
            true
        } else {
            false
        }
    }

    /// `[+-]hh[:mm[:ss]]` in seconds.
    fn time(&mut self) -> Option<i64> {
        let sign = if self.eat(b'-') {
            -1
        } else {
            self.eat(b'+');
            1
        };
        let mut seconds = self.number()? * 3600;
        if self.eat(b':') {
            seconds += self.number()? * 60;
            if self.eat(b':') {
                seconds += self.number()?;
            }
        }
        Some(sign * seconds)
    }

    fn switch(&mut self) -> Option<Switch> {
        let day = if self.eat(b'M') {
            let mon = self.number()?;
            self.eat(b'.').then_some(())?;
            let week = self.number()?;
            self.eat(b'.').then_some(())?;
            let wday = self.number()?;
            if !(1..=12).contains(&mon) || !(1..=5).contains(&week) || wday > 6 {
                return None;
            }
            Day::Month { mon, week, wday }
        } else if self.eat(b'J') {
            Day::Julian1(self.number()?)
        } else {
            Day::Julian0(self.number()?)
        };
        let time = if self.eat(b'/') { self.time()? } else { 7200 };
        Some(Switch { day, time })
    }

    fn parse(text: &str) -> Option<Posix> {
        let mut p = PosixParser {
            rest: text.as_bytes(),
        };
        let std_abbr = p.name()?;
        // POSIX offsets are west of UTC.
        let std_offset = -p.time()?;
        let std = Kind {
            offset: i32::try_from(std_offset).ok()?,
            isdst: false,
            abbr: std_abbr,
        };
        if p.rest.is_empty() {
            return Some(Posix { std, dst: None });
        }
        let dst_abbr = p.name()?;
        let dst_offset = if p.rest.first().is_some_and(|c| *c != b',') {
            -p.time()?
        } else {
            std_offset + 3600
        };
        let dst = Kind {
            offset: i32::try_from(dst_offset).ok()?,
            isdst: true,
            abbr: dst_abbr,
        };
        let (start, end) = if p.eat(b',') {
            let start = p.switch()?;
            p.eat(b',').then_some(())?;
            (start, p.switch()?)
        } else {
            // glibc's default rule.
            (
                Switch {
                    day: Day::Month {
                        mon: 3,
                        week: 2,
                        wday: 0,
                    },
                    time: 7200,
                },
                Switch {
                    day: Day::Month {
                        mon: 11,
                        week: 1,
                        wday: 0,
                    },
                    time: 7200,
                },
            )
        };
        p.rest.is_empty().then_some(Posix {
            std,
            dst: Some((dst, start, end)),
        })
    }
}

/// A time zone: transitions of a TZif file, then its POSIX footer.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Tz {
    transitions: Vec<(i64, Kind)>,
    /// Before the first transition.
    initial: Kind,
    footer: Option<Posix>,
}

impl Tz {
    /// UTC.
    #[must_use]
    pub fn utc() -> Self {
        Self::posix("UTC0").unwrap_or(Self {
            transitions: Vec::new(),
            initial: Kind {
                offset: 0,
                isdst: false,
                abbr: Abbr::of(b"UTC"),
            },
            footer: None,
        })
    }

    /// A zone from a POSIX TZ string.
    #[must_use]
    pub fn posix(text: &str) -> Option<Self> {
        let posix = PosixParser::parse(text)?;
        Some(Self {
            transitions: Vec::new(),
            initial: posix.std,
            footer: Some(posix),
        })
    }

    /// A zone from the content of a TZif file (versions 1 to 4).
    #[must_use]
    pub fn tzif(data: &[u8]) -> Option<Self> {
        let header = |data: &[u8]| -> Option<[usize; 6]> {
            if data.len() < 44 || &data[..4] != b"TZif" {
                return None;
            }
            let mut counts = [0usize; 6];
            for (i, count) in counts.iter_mut().enumerate() {
                let at = 20 + i * 4;
                *count = u32::from_be_bytes(data[at..at + 4].try_into().ok()?) as usize;
            }
            Some(counts)
        };
        let counts = header(data)?;
        let version = data[4];
        let block = |counts: [usize; 6], time_size: usize| {
            let [isut, isstd, leap, time, types, chars] = counts;
            time * time_size + time + types * 6 + chars + leap * (time_size + 4) + isstd + isut
        };
        let (body, counts, time_size) = if version >= b'2' {
            let second = &data[44 + block(counts, 4)..];
            (&second[44..], header(second)?, 8)
        } else {
            (&data[44..], counts, 4)
        };
        let [_, _, _, time_count, type_count, _] = counts;
        if body.len() < block(counts, time_size) || type_count == 0 {
            return None;
        }
        let read_time = |at: usize| -> i64 {
            if time_size == 8 {
                i64::from_be_bytes(body[at..at + 8].try_into().unwrap_or_default())
            } else {
                i64::from(i32::from_be_bytes(
                    body[at..at + 4].try_into().unwrap_or_default(),
                ))
            }
        };
        let types_at = time_count * time_size + time_count;
        let chars_at = types_at + type_count * 6;
        let chars = &body[chars_at..chars_at + counts[5]];
        let kinds: Vec<Kind> = (0..type_count)
            .map(|i| {
                let at = types_at + i * 6;
                let index = usize::from(body[at + 5]);
                let abbr = chars.get(index..).map_or(Abbr::default(), |rest| {
                    let end = rest.iter().position(|b| *b == 0).unwrap_or(rest.len());
                    Abbr::of(&rest[..end])
                });
                Kind {
                    offset: i32::from_be_bytes(body[at..at + 4].try_into().unwrap_or_default()),
                    isdst: body[at + 4] != 0,
                    abbr,
                }
            })
            .collect();
        let mut transitions = Vec::with_capacity(time_count);
        for i in 0..time_count {
            let index = usize::from(body[time_count * time_size + i]);
            let kind = *kinds.get(index)?;
            transitions.push((read_time(i * time_size), kind));
        }
        // Before the first transition: the first standard type (glibc).
        let initial = kinds.iter().copied().find(|k| !k.isdst).unwrap_or(kinds[0]);
        let footer = if version >= b'2' {
            let rest = &body[block(counts, time_size)..];
            std::str::from_utf8(rest)
                .ok()
                .and_then(|text| text.trim_matches('\n').lines().next())
                .and_then(PosixParser::parse)
        } else {
            None
        };
        Some(Self {
            transitions,
            initial,
            footer,
        })
    }

    fn kind_at(&self, t: i64) -> Kind {
        match self.transitions.partition_point(|(at, _)| *at <= t) {
            0 => match &self.footer {
                Some(footer) if self.transitions.is_empty() => footer.kind_at(t),
                _ => self.initial,
            },
            n if n == self.transitions.len() => match &self.footer {
                Some(footer) => footer.kind_at(t),
                None => self.transitions[n - 1].1,
            },
            n => self.transitions[n - 1].1,
        }
    }

    /// `tm_zone` of `localtime_r` at `t`: the abbreviation of the zone.
    #[must_use]
    pub fn abbreviation(&self, t: i64) -> String {
        self.kind_at(t).abbr.text()
    }

    /// `localtime_r`.
    #[must_use]
    pub fn localtime(&self, t: i64) -> Tm {
        let kind = self.kind_at(t);
        breakdown(t, kind.offset, kind.isdst)
    }

    /// `mktime` with `tm_isdst = -1`: the time of a wall time, `tm`
    /// normalized to it. As glibc, a time skipped by a DST change is read
    /// with the offset before the change (02:30 becomes 03:30), and a time
    /// that occurs twice is the first one.
    pub fn mktime(&self, tm: &mut Tm) -> i64 {
        let wall = wall_seconds(tm);
        let before = self.kind_at(wall - 86_400).offset;
        let after = self.kind_at(wall + 86_400).offset;
        let first = wall - i64::from(before);
        let t = if before == after || self.kind_at(first).offset == before {
            first
        } else {
            let second = wall - i64::from(after);
            if self.kind_at(second).offset == after {
                second
            } else {
                first
            }
        };
        *tm = self.localtime(t);
        t
    }
}

/// The zones the engine uses, by the name `TZ` would take (`:Europe/Paris`,
/// `Europe/Paris`, a POSIX string, or empty for the zone of the process),
/// each read once.
#[derive(Debug)]
pub struct Zones {
    dir: PathBuf,
    local: Arc<Tz>,
    by_name: HashMap<String, Arc<Tz>>,
}

impl Zones {
    /// Zones read from `/usr/share/zoneinfo`; the zone of the process is
    /// `TZ`, else `/etc/localtime`, else UTC, as for glibc.
    #[must_use]
    pub fn system() -> Self {
        let dir = PathBuf::from("/usr/share/zoneinfo");
        let local = std::env::var("TZ")
            .ok()
            .filter(|name| !name.is_empty())
            .and_then(|name| load(&dir, &name))
            .or_else(|| {
                std::fs::read("/etc/localtime")
                    .ok()
                    .and_then(|d| Tz::tzif(&d))
            })
            .unwrap_or_else(Tz::utc);
        Self::with(dir, local)
    }

    /// Zones read from `dir`, `local` for the empty name.
    #[must_use]
    pub fn with(dir: PathBuf, local: Tz) -> Self {
        Self {
            dir,
            local: Arc::new(local),
            by_name: HashMap::new(),
        }
    }

    /// The zone `name`; an unknown zone is UTC, as glibc does.
    pub fn get(&mut self, name: &str) -> Arc<Tz> {
        if name.is_empty() {
            return self.local.clone();
        }
        if let Some(zone) = self.by_name.get(name) {
            return zone.clone();
        }
        let zone = Arc::new(load(&self.dir, name).unwrap_or_else(|| {
            tracing::warn!(timezone = name, "unknown time zone: UTC is used");
            Tz::utc()
        }));
        self.by_name.insert(name.to_owned(), zone.clone());
        zone
    }
}

/// A zone as glibc reads `TZ`: a file name (after an optional `:`) under
/// `dir` or absolute, else a POSIX string.
fn load(dir: &Path, name: &str) -> Option<Tz> {
    let file = name.strip_prefix(':').unwrap_or(name);
    let path = Path::new(file);
    let path = if path.is_absolute() {
        Some(path.to_path_buf())
    } else if path
        .components()
        .all(|c| matches!(c, std::path::Component::Normal(_)))
    {
        Some(dir.join(path))
    } else {
        None
    };
    if let Some(zone) = path
        .and_then(|path| std::fs::read(path).ok())
        .and_then(|data| Tz::tzif(&data))
    {
        return Some(zone);
    }
    Tz::posix(file)
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;

    /// Europe/Paris as a POSIX string: no dependency on the tzdata of the
    /// machine running the tests.
    pub(crate) const PARIS: &str = "CET-1CEST,M3.5.0,M10.5.0/3";

    fn tm(year: i32, mon: i32, mday: i32, hour: i32, min: i32) -> Tm {
        Tm {
            year: year - 1900,
            mon: mon - 1,
            mday,
            hour,
            min,
            ..Tm::default()
        }
    }

    #[test]
    fn mktime_matches_glibc_around_dst_changes() {
        // Expected values from glibc (Python time.mktime, TZ=PARIS).
        let paris = Tz::posix(PARIS).unwrap();
        let cases = [
            ((2026, 3, 29, 2, 30), 1_774_747_800, (3, 30, true)),
            ((2026, 3, 29, 1, 30), 1_774_744_200, (1, 30, false)),
            ((2026, 3, 29, 3, 0), 1_774_746_000, (3, 0, true)),
            ((2026, 10, 25, 2, 30), 1_792_888_200, (2, 30, true)),
            ((2026, 10, 25, 1, 59), 1_792_886_340, (1, 59, true)),
            ((2026, 10, 25, 3, 0), 1_792_893_600, (3, 0, false)),
            ((2026, 3, 29, 0, 0), 1_774_738_800, (0, 0, false)),
            ((2026, 3, 30, 0, 0), 1_774_821_600, (0, 0, true)),
        ];
        for ((y, mo, d, h, mi), expected, (hour, min, dst)) in cases {
            let mut t = tm(y, mo, d, h, mi);
            assert_eq!(paris.mktime(&mut t), expected, "{y}-{mo}-{d} {h}:{mi}");
            assert_eq!((t.hour, t.min, t.isdst), (hour, min, dst));
        }
        // Out of range fields are normalized.
        let mut t = tm(2026, 2, 30, 0, 0);
        assert_eq!(paris.mktime(&mut t), 1_772_406_000);
        assert_eq!((t.mon, t.mday, t.wday), (2, 2, 1));
        let mut t = tm(2026, 1, 0, 0, 0);
        assert_eq!(paris.mktime(&mut t), 1_767_135_600);
        assert_eq!((t.year, t.mon, t.mday), (125, 11, 31));
    }

    #[test]
    fn localtime_breaks_down() {
        let utc = Tz::utc();
        let t = utc.localtime(0);
        assert_eq!((t.year, t.mon, t.mday, t.wday, t.yday), (70, 0, 1, 4, 0));
        let t = utc.localtime(951_782_400); // 2000-02-29
        assert_eq!((t.mon, t.mday, t.yday, t.wday), (1, 29, 59, 2));
        let paris = Tz::posix(PARIS).unwrap();
        let t = paris.localtime(1_792_890_000); // 2026-10-25 01:00 UTC
        assert_eq!((t.hour, t.isdst), (2, false));
    }

    #[test]
    fn reads_tzif_files() {
        // Version 2 file: one transition, then the footer rule.
        let mut data = Vec::new();
        let header = |data: &mut Vec<u8>, version: u8, times: u32| {
            data.extend_from_slice(b"TZif");
            data.push(version);
            data.extend_from_slice(&[0; 15]);
            for count in [0u32, 0, 0, times, 2, 8] {
                data.extend_from_slice(&count.to_be_bytes());
            }
        };
        header(&mut data, b'2', 0);
        data.extend_from_slice(&[0; 12 + 8]);
        header(&mut data, b'2', 1);
        data.extend_from_slice(&0i64.to_be_bytes());
        data.push(1);
        data.extend_from_slice(&3600i32.to_be_bytes());
        data.extend_from_slice(&[0, 0]);
        data.extend_from_slice(&7200i32.to_be_bytes());
        data.extend_from_slice(&[1, 4]);
        data.extend_from_slice(b"CET\0CEST");
        data.extend_from_slice(format!("\n{PARIS}\n").as_bytes());
        let zone = Tz::tzif(&data).unwrap();
        // Before the transition: the standard type.
        assert_eq!(zone.localtime(-10).hour, 0);
        // From the last transition on, the footer rule: winter time.
        assert_eq!(zone.localtime(0).hour, 1);
        // After it, the footer rule.
        assert!(zone.localtime(1_782_000_000).isdst);
        assert!(!zone.localtime(1_800_000_000).isdst);
        assert!(Tz::tzif(b"nope").is_none());
    }

    #[test]
    fn zones_by_name() {
        let dir = tempfile::tempdir().unwrap();
        let mut zones = Zones::with(dir.path().to_path_buf(), Tz::utc());
        assert_eq!(*zones.get(""), Tz::utc());
        let paris = zones.get(&format!(":{PARIS}"));
        assert_eq!(*paris, Tz::posix(PARIS).unwrap());
        assert_eq!(*zones.get(":Nowhere/Land"), Tz::utc());
        assert_eq!(*zones.get("../etc/passwd"), Tz::utc());
        // Southern hemisphere: DST across new year.
        let sydney = Tz::posix("AEST-10AEDT,M10.1.0,M4.1.0/3").unwrap();
        assert!(sydney.localtime(1_767_225_600).isdst); // 2026-01-01
        assert!(!sydney.localtime(1_783_000_000).isdst); // July
    }
}
