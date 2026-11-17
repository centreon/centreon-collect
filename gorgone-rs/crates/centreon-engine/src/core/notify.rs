//! Notifications, as Engine's `notifier::notify` sends them
//! (`engine/src/notifier.cc:222-1041`, `engine/src/notification.cc:61-189`,
//! `contact::should_be_notified` `engine/src/contact.cc:815-1020`,
//! `host::notify_contact` `host.cc:2602-2732`, `service::notify_contact`
//! `service.cc:3207-3335`): viability per category, the contacts (those
//! of the escalations in force, else the object's and its contact
//! groups'), one process per contact and command (no shell, killed at
//! `notification_timeout`), the `HOST NOTIFICATION` / `SERVICE
//! NOTIFICATION` lines, then the numbers and times. No BBDO event: Engine
//! sends none, the central gets the log lines and the fields of the
//! statuses.

use std::collections::{BTreeSet, HashMap};
use std::time::Duration;

use crate::cfg::{DateFormat, LogLevel, NotifyConf};
use crate::macros::{Context, HostView, ServiceView};
use crate::objects::{ContactConf, notify_on};
use crate::state::{AckType, Reason, Sent, StateType, code};
use crate::{cmdline, runner};

use super::{Core, ObjConf};

/// What a viability check decides.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Viable {
    Yes,
    /// Not now; a recovery may still be sent later.
    Later,
    /// Not now, and a recovery forgets the problem notified.
    Drop,
}

const MONTHS: [&str; 12] = [
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sept", "Oct", "Nov", "Dec",
];
const WEEKDAYS: [&str; 7] = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];

/// The date macros at `now` in `zone` (`get_datetime_string`,
/// `engine/src/shared.cc:116-190`): `LONGDATETIME`, `SHORTDATETIME`,
/// `DATE`, `TIME`, `TIMET`.
pub(super) fn date_macros(
    tz: &crate::tz::Tz,
    abbreviation: &str,
    format: DateFormat,
    now: i64,
    out: &mut HashMap<String, String>,
) {
    let t = tz.localtime(now);
    let (year, mon, day) = (t.year + 1900, t.mon + 1, t.mday);
    let time = format!("{:02}:{:02}:{:02}", t.hour, t.min, t.sec);
    let date = match format {
        DateFormat::Us => format!("{mon:02}-{day:02}-{year:04}"),
        DateFormat::Euro => format!("{day:02}-{mon:02}-{year:04}"),
        DateFormat::Iso8601 | DateFormat::StrictIso8601 => {
            format!("{year:04}-{mon:02}-{day:02}")
        }
    };
    let separator = if format == DateFormat::StrictIso8601 {
        "T"
    } else {
        " "
    };
    let long = format!(
        "{} {} {} {time} {abbreviation} {year}",
        WEEKDAYS[t.wday.rem_euclid(7) as usize],
        MONTHS[t.mon.rem_euclid(12) as usize],
        t.mday
    );
    out.insert("LONGDATETIME".into(), long);
    out.insert("SHORTDATETIME".into(), format!("{date}{separator}{time}"));
    out.insert("DATE".into(), date);
    out.insert("TIME".into(), time);
    out.insert("TIMET".into(), now.to_string());
}

/// `HOST NOTIFICATION` / `SERVICE NOTIFICATION` state field: the state,
/// or `TYPE (STATE)` for another reason.
fn state_field(reason: Reason, state: &str) -> String {
    match reason {
        Reason::Normal => state.to_owned(),
        other => format!("{} ({state})", other.name()),
    }
}

impl Core {
    /// The notification settings of object `index`.
    pub(super) fn notify_conf(&self, index: usize) -> &NotifyConf {
        match &self.objs[index].conf {
            ObjConf::Host(host) => &host.notify,
            ObjConf::Service { conf, .. } => &conf.notify,
        }
    }

    /// `enable_notifications`, as `ENABLE_NOTIFICATIONS` left it.
    pub(super) fn notifications_on(&self) -> bool {
        self.program
            .notifications
            .unwrap_or(self.settings().enable_notifications)
    }

    /// `notifications_enabled` of object `index`, as the commands left it.
    pub(super) fn notifier_enabled(&self, index: usize) -> bool {
        self.objs[index]
            .state
            .modified
            .notifications_enabled
            .unwrap_or(self.notify_conf(index).notifications_enabled)
    }

    /// `is_in_downtime`: a service is also in the downtime of its host.
    pub(super) fn in_downtime(&self, index: usize) -> bool {
        let obj = &self.objs[index];
        obj.state.downtime_depth > 0
            || matches!(obj.conf, ObjConf::Service { host, .. }
                if self.objs[host].state.downtime_depth > 0)
    }

    fn notify_on(&self, index: usize, flag: u32) -> bool {
        self.notify_conf(index).notification_options & flag != 0
    }

    /// `get_notify_on_current_state` (`host.cc:3941-3945`,
    /// `service.cc:3816-3824`).
    fn notify_on_current_state(&self, index: usize) -> bool {
        let obj = &self.objs[index];
        match &obj.conf {
            ObjConf::Host(_) => self.notify_on(index, notify_on::state(true, obj.state.state)),
            ObjConf::Service { host, .. } => {
                let host = &self.objs[*host].state;
                if host.state != code::OK
                    && (host.state_type == StateType::Hard
                        || self.settings().soft_state_dependencies)
                {
                    return false;
                }
                self.notify_on(index, notify_on::state(false, obj.state.state))
            }
        }
    }

    /// `check_time_against_period[_for_notif]` of `period` (always when
    /// empty or unknown) at `time`, in `zone` (`use_timezone` when empty).
    pub(super) fn period_contains(
        &mut self,
        zone: &str,
        period: &str,
        time: i64,
        for_notification: bool,
    ) -> bool {
        let Some(loaded) = self.loaded.as_ref() else {
            return true;
        };
        if period.is_empty() {
            return true;
        }
        let zone = if zone.is_empty() {
            loaded.config.settings.use_timezone.as_str()
        } else {
            zone
        };
        let tz = self.zones.get(zone);
        let periods = &loaded.config.timeperiods;
        let calc = crate::timeperiod::Calc { tz: &tz, periods };
        if for_notification {
            calc.contains_for_notification(time, periods.get(period))
        } else {
            calc.contains(time, periods.get(period))
        }
    }

    /// The notification period of object `index` holds now.
    fn in_notification_period(&mut self, index: usize) -> bool {
        let now = self.clock.now();
        let zone = self.objs[index].timezone().to_owned();
        let period = self.notify_conf(index).notification_period.clone();
        self.period_contains(&zone, &period, now, true)
    }

    /// `_is_notification_viable_*` (`notifier.cc:222-865`).
    fn notification_viable(&mut self, index: usize, reason: Reason, forced: bool) -> Viable {
        let category = reason.category();
        let now = self.clock.now();
        let il = self.interval_length() as i64;
        let st = self.objs[index].state.clone();
        let notify = self.notify_conf(index).clone();
        let on = |v: bool| if v { Viable::Yes } else { Viable::Drop };
        if reason == Reason::Recovery {
            if !self.notifications_on() || !self.notifier_enabled(index) {
                return Viable::Drop;
            }
            if !self.in_notification_period(index) {
                return Viable::Later;
            }
            if self.in_downtime(index)
                || st.flap.is_flapping
                || st.state_type != StateType::Hard
                || st.state != code::OK
            {
                return Viable::Later;
            }
            if !self.notify_on(index, notify_on::UP | notify_on::OK) {
                return Viable::Drop;
            }
            if st.last_hard_state_change + i64::from(notify.recovery_notification_delay) * il > now
            {
                return Viable::Later;
            }
            return on(st.notification.number != 0 && st.notification.sent.contains_key(&0));
        }
        if forced {
            return Viable::Yes;
        }
        if !self.notifications_on() || !self.notifier_enabled(index) {
            return Viable::Drop;
        }
        match reason {
            Reason::Normal => {
                if self.in_downtime(index)
                    || !self.in_notification_period(index)
                    || st.flap.is_flapping
                {
                    return Viable::Drop;
                }
                if notify.is_volatile {
                    return Viable::Yes;
                }
                if st.state_type != StateType::Hard
                    || st.acknowledgement != AckType::None
                    || st.state == code::OK
                    || !self.notify_on_current_state(index)
                {
                    return Viable::Drop;
                }
                let normal = st.notification.sent.get(&0);
                if notify.first_notification_delay > 0
                    && normal.is_none()
                    && st.last_hard_state_change + i64::from(notify.first_notification_delay) * il
                        > now
                {
                    return Viable::Drop;
                }
                if !self.authorized_by_dependencies_for(index, 0, true) {
                    return Viable::Drop;
                }
                if let Some(normal) = normal
                    && st.last_hard_state_change <= st.notification.last
                {
                    let interval = normal.interval;
                    if interval == 0.0
                        || st.notification.last as f64 + interval * il as f64 > now as f64
                    {
                        return Viable::Drop;
                    }
                }
                Viable::Yes
            }
            Reason::Acknowledgement => on(st.state != code::OK),
            Reason::FlappingStart | Reason::FlappingStop | Reason::FlappingDisabled => {
                let flag = match reason {
                    Reason::FlappingStart => notify_on::FLAPPING_START,
                    Reason::FlappingStop => notify_on::FLAPPING_STOP,
                    _ => notify_on::FLAPPING_DISABLED,
                };
                let previous = st.notification.sent.get(&category).map(|s| s.reason);
                let ok = self.notify_on(index, flag)
                    && match reason {
                        Reason::FlappingStart => previous.is_none(),
                        _ => previous == Some(Reason::FlappingStart),
                    }
                    && previous != Some(reason)
                    && !self.in_downtime(index);
                on(ok)
            }
            Reason::DowntimeStart | Reason::DowntimeEnd | Reason::DowntimeCancelled => {
                on(self.notify_on(index, notify_on::DOWNTIME) && st.downtime_depth == 0)
            }
            Reason::Custom => on(!self.in_downtime(index)),
            Reason::Recovery => Viable::Drop,
        }
    }

    /// `contact::should_be_notified` (`contact.cc:815-1020`).
    fn should_be_notified(&mut self, contact: &ContactConf, index: usize, reason: Reason) -> bool {
        let host = self.objs[index].is_host();
        let (enabled, period, options) = if host {
            (
                contact.host_notifications_enabled,
                &contact.host_notification_period,
                contact.host_notification_options,
            )
        } else {
            (
                contact.service_notifications_enabled,
                &contact.service_notification_period,
                contact.service_notification_options,
            )
        };
        let now = self.clock.now();
        if !enabled || !self.period_contains(&contact.timezone, period, now, true) {
            return false;
        }
        let state = self.objs[index].state.state;
        match reason {
            Reason::Normal => options & notify_on::state(host, state) != 0,
            Reason::Recovery => {
                options & (notify_on::OK | notify_on::UP) != 0
                    && self.objs[index]
                        .state
                        .notification
                        .sent
                        .get(&0)
                        .is_some_and(|sent| sent.contacts.contains(&contact.name))
            }
            Reason::Acknowledgement | Reason::Custom => true,
            Reason::FlappingStart => options & notify_on::FLAPPING_START != 0,
            Reason::FlappingStop => options & notify_on::FLAPPING_STOP != 0,
            Reason::FlappingDisabled => options & notify_on::FLAPPING_DISABLED != 0,
            Reason::DowntimeStart | Reason::DowntimeEnd | Reason::DowntimeCancelled => {
                options & notify_on::DOWNTIME != 0
            }
        }
    }

    /// `get_contacts_to_notify` (`notifier.cc:878-943`): the contacts of
    /// the escalations in force (with the smallest interval), else the
    /// object's and its contact groups'; sorted (Engine's set has no
    /// order).
    fn contacts_to_notify(&mut self, index: usize, reason: Reason) -> (Vec<String>, f64, bool) {
        let Some(loaded) = self.loaded.as_ref() else {
            return (Vec::new(), 0.0, false);
        };
        let notify = self.notify_conf(index).clone();
        let st = self.objs[index].state.clone();
        let host = self.objs[index].is_host();
        let escalations: Vec<_> = self.links.escalations[index]
            .iter()
            .filter_map(|e| loaded.config.escalations.get(*e).cloned())
            .collect();
        let use_timezone = loaded.config.settings.use_timezone.clone();
        let now = self.clock.now();
        let mut interval = f64::from(notify.notification_interval);
        let mut escalated = false;
        let mut names: BTreeSet<String> = BTreeSet::new();
        let group_members = |core: &Self, groups: &[String], names: &mut BTreeSet<String>| {
            if let Some(loaded) = core.loaded.as_ref() {
                for group in groups {
                    if let Some(group) = loaded.config.contactgroups.get(group) {
                        names.extend(group.members.iter().cloned());
                    }
                }
            }
        };
        for escalation in &escalations {
            let number = st.notification.number;
            let in_range = escalation.first_notification <= number
                && (escalation.last_notification == 0 || number <= escalation.last_notification);
            if !in_range
                || escalation.escalate_on & notify_on::state(host, st.state) == 0
                || !self.period_contains(&use_timezone, &escalation.escalation_period, now, false)
            {
                continue;
            }
            interval = if escalated {
                interval.min(escalation.notification_interval)
            } else {
                escalation.notification_interval
            };
            escalated = true;
            group_members(self, &escalation.contact_groups, &mut names);
        }
        if !escalated {
            names.extend(notify.contacts.iter().cloned());
            group_members(self, &notify.contact_groups, &mut names);
        }
        let mut contacts = Vec::new();
        for name in names {
            let Some(contact) = self
                .loaded
                .as_ref()
                .and_then(|l| l.config.contacts.get(&name))
                .cloned()
            else {
                continue;
            };
            if self.should_be_notified(&contact, index, reason) {
                contacts.push(name);
            }
        }
        (contacts, interval, escalated)
    }

    /// `notifier::notify`: sends the notification `reason` of object
    /// `index` when it is viable.
    pub(super) fn notify(
        &mut self,
        index: usize,
        reason: Reason,
        author: &str,
        comment: &str,
        forced: bool,
    ) {
        if self.loaded.is_none() {
            return;
        }
        let category = reason.category();
        match self.notification_viable(index, reason, forced) {
            Viable::Yes => {}
            Viable::Later => return,
            Viable::Drop => {
                if reason == Reason::Recovery {
                    let notification = &mut self.objs[index].state.notification;
                    notification.sent.remove(&0);
                    notification.number = 0;
                }
                return;
            }
        }
        if reason != Reason::Recovery {
            self.objs[index].state.notification.number += 1;
        }
        let (contacts, interval, escalated) = self.contacts_to_notify(index, reason);
        self.next_notification_id += 1;
        let id = self.next_notification_id;
        let number = self.objs[index].state.notification.number;
        self.objs[index].state.notification.id = id;
        tracing::debug!(
            host_id = self.objs[index].key.0,
            service_id = self.objs[index].key.1,
            reason = reason.name(),
            number,
            contacts = contacts.len(),
            "notification"
        );
        let notified = self.send_notification(
            index,
            reason,
            (author, comment),
            (id, number, escalated),
            &contacts,
        );
        let now = self.clock.now();
        let notification = &mut self.objs[index].state.notification;
        if !notified.is_empty() {
            notification.last = now;
        }
        let mut sent = Sent {
            reason,
            number,
            interval,
            escalated,
            contacts: notified,
        };
        if category == 0 {
            if let Some(previous) = notification.sent.get(&0) {
                sent.contacts.extend(previous.contacts.iter().cloned());
            }
            notification.sent.insert(0, sent);
            return;
        }
        notification.sent.insert(category, sent);
        match reason {
            Reason::Recovery => {
                notification.sent.remove(&0);
                notification.sent.remove(&1);
            }
            Reason::FlappingStop | Reason::FlappingDisabled => {
                notification.sent.remove(&3);
            }
            Reason::DowntimeEnd | Reason::DowntimeCancelled => {
                notification.sent.remove(&4);
            }
            Reason::FlappingStart | Reason::DowntimeStart => {}
            _ => {
                notification.sent.remove(&category);
            }
        }
        if category != 2 && category != 4 {
            notification.number = 0;
        }
    }

    /// The macros every contact of a notification shares
    /// (`notification::execute`, `notification.cc:61-158`, and the host,
    /// service, date and summary macros).
    fn notification_macros(
        &mut self,
        index: usize,
        reason: Reason,
        (author, comment): (&str, &str),
        (id, number, escalated): (u64, u32, bool),
    ) -> HashMap<String, String> {
        let mut m = HashMap::new();
        let mut set = |k: &str, v: String| {
            m.insert(k.to_owned(), v);
        };
        set("NOTIFICATIONTYPE", reason.macro_name().to_owned());
        set("NOTIFICATIONNUMBER", number.to_string());
        set("NOTIFICATIONISESCALATED", u8::from(escalated).to_string());
        set("NOTIFICATIONAUTHOR", author.to_owned());
        set("NOTIFICATIONCOMMENT", comment.to_owned());
        let (author_name, author_alias) = self
            .loaded
            .as_ref()
            .and_then(|l| {
                l.config.contacts.get(author).or_else(|| {
                    l.config
                        .contacts
                        .values()
                        .find(|c| !author.is_empty() && c.alias == author)
                })
            })
            .map(|c| (c.name.clone(), c.alias.clone()))
            .unwrap_or_default();
        set("NOTIFICATIONAUTHORNAME", author_name);
        set("NOTIFICATIONAUTHORALIAS", author_alias);
        let prefix = if self.objs[index].is_host() {
            "HOST"
        } else {
            "SERVICE"
        };
        set(&format!("{prefix}NOTIFICATIONNUMBER"), number.to_string());
        set(&format!("{prefix}NOTIFICATIONID"), id.to_string());
        self.object_macros(index, &mut m);
        m
    }

    /// Host and service macros a check does not need: outputs, last
    /// change, notification numbers, the services of the host, dates and
    /// the administrator (`grab_host.cc`, `grab_service.cc`, `macros.cc`).
    pub(super) fn object_macros(&mut self, index: usize, m: &mut HashMap<String, String>) {
        let host_index = match &self.objs[index].conf {
            ObjConf::Host(_) => index,
            ObjConf::Service { host, .. } => *host,
        };
        let now = self.clock.now();
        let host = &self.objs[host_index].state;
        m.insert("HOSTOUTPUT".into(), host.output.clone());
        m.insert("LONGHOSTOUTPUT".into(), host.long_output.clone());
        m.insert("HOSTPERFDATA".into(), host.perfdata.clone());
        m.insert(
            "LASTHOSTSTATECHANGE".into(),
            host.last_state_change.to_string(),
        );
        m.insert("LASTHOSTCHECK".into(), host.last_check.to_string());
        m.entry("HOSTNOTIFICATIONNUMBER".into())
            .or_insert_with(|| host.notification.number.to_string());
        m.entry("HOSTNOTIFICATIONID".into())
            .or_insert_with(|| host.notification.id.to_string());
        let host_id = self.objs[host_index].key.0;
        let mut totals = [0usize; 4];
        let mut all = 0;
        for obj in &self.objs {
            if obj.key.0 == host_id && obj.key.1 != 0 {
                all += 1;
                if let Some(total) = totals.get_mut(obj.state.state as usize) {
                    *total += 1;
                }
            }
        }
        m.insert("TOTALHOSTSERVICES".into(), all.to_string());
        for (name, total) in ["OK", "WARNING", "CRITICAL", "UNKNOWN"].iter().zip(totals) {
            m.insert(format!("TOTALHOSTSERVICES{name}"), total.to_string());
        }
        if !self.objs[index].is_host() {
            let st = &self.objs[index].state;
            m.insert("SERVICEOUTPUT".into(), st.output.clone());
            m.insert("LONGSERVICEOUTPUT".into(), st.long_output.clone());
            m.insert("SERVICEPERFDATA".into(), st.perfdata.clone());
            m.insert(
                "LASTSERVICESTATECHANGE".into(),
                st.last_state_change.to_string(),
            );
            m.insert("LASTSERVICECHECK".into(), st.last_check.to_string());
            m.entry("SERVICENOTIFICATIONNUMBER".into())
                .or_insert_with(|| st.notification.number.to_string());
            m.entry("SERVICENOTIFICATIONID".into())
                .or_insert_with(|| st.notification.id.to_string());
        }
        let settings = self.settings().clone();
        m.insert("ADMINEMAIL".into(), settings.admin_email.clone());
        m.insert("ADMINPAGER".into(), settings.admin_pager.clone());
        let tz = self.zones.get(&settings.use_timezone);
        let abbreviation = tz.abbreviation(now);
        date_macros(&tz, &abbreviation, settings.date_format, now, m);
    }

    /// `notification::execute`: runs the commands of each contact, writes
    /// the log lines; the contacts notified.
    fn send_notification(
        &mut self,
        index: usize,
        reason: Reason,
        texts: (&str, &str),
        numbers: (u64, u32, bool),
        contacts: &[String],
    ) -> BTreeSet<String> {
        let mut notified = BTreeSet::new();
        if contacts.is_empty() {
            return notified;
        }
        let mut shared = self.notification_macros(index, reason, texts, numbers);
        let Some(loaded) = self.loaded.as_ref() else {
            return notified;
        };
        let settings = loaded.config.settings.clone();
        let log = settings.log_notifications && settings.log_level_notifications.logs_info();
        let timeout = Duration::from_secs(u64::from(settings.notification_timeout.max(1)));
        let obj = &self.objs[index];
        let is_host = obj.is_host();
        let (host_obj, service) = match &obj.conf {
            ObjConf::Host(_) => (obj, None),
            ObjConf::Service { conf, host } => (&self.objs[*host], Some((conf, obj))),
        };
        let ObjConf::Host(host) = &host_obj.conf else {
            return notified;
        };
        let state_name = if is_host {
            crate::logs::host_state(obj.state.state)
        } else {
            crate::logs::service_state(obj.state.state)
        };
        let field = state_field(reason, state_name);
        let info = if reason == Reason::Acknowledgement {
            format!(";{};{}", texts.0, texts.1)
        } else {
            String::new()
        };
        let secrets = if loaded.config.secret_prefixes {
            self.credentials.as_ref()
        } else {
            None
        };
        let mut runs = Vec::new();
        let mut lines = Vec::new();
        let mut recipients: Vec<String> = Vec::new();
        for name in contacts {
            let Some(contact) = loaded.config.contacts.get(name) else {
                continue;
            };
            let mut m = shared.clone();
            m.insert("CONTACTNAME".into(), contact.name.clone());
            m.insert("CONTACTALIAS".into(), contact.alias.clone());
            m.insert("CONTACTEMAIL".into(), contact.email.clone());
            m.insert("CONTACTPAGER".into(), contact.pager.clone());
            m.insert("CONTACTTIMEZONE".into(), contact.timezone.clone());
            for (i, address) in contact.addresses.iter().enumerate() {
                m.insert(format!("CONTACTADDRESS{}", i + 1), address.clone());
            }
            m.insert(
                "CONTACTGROUPNAME".into(),
                contact.groups.first().cloned().unwrap_or_default(),
            );
            m.insert("CONTACTGROUPNAMES".into(), contact.groups.join(","));
            m.insert("NOTIFICATIONRECIPIENTS".into(), recipients.join(","));
            let commands = if is_host {
                &contact.host_notification_commands
            } else {
                &contact.service_notification_commands
            };
            for command in commands {
                let Some(line) = loaded.config.commands.get(command) else {
                    continue;
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
                .with_secrets(secrets)
                .with_extra(&m)
                .stripping(&settings.illegal_macro_output_chars);
                let processed = ctx.check_command_line(command, line);
                if processed.trim().is_empty() {
                    continue;
                }
                if log {
                    lines.push(match service {
                        None => format!(
                            "HOST NOTIFICATION: {};{};{field};{command};{}{info}",
                            contact.name, host.name, obj.state.output
                        ),
                        Some((conf, _)) => format!(
                            "SERVICE NOTIFICATION: {};{};{};{field};{command};{};{info}",
                            contact.name, host.name, conf.description, obj.state.output
                        ),
                    });
                }
                match cmdline::split(&processed) {
                    Ok(argv) => runs.push((argv, contact.name.clone(), command.clone())),
                    Err(error) => {
                        tracing::warn!(contact = %contact.name, %command, %error, "notification command not run");
                    }
                }
            }
            recipients.push(contact.name.clone());
            notified.insert(contact.name.clone());
        }
        shared.clear();
        for line in lines {
            self.log(&line);
        }
        let kind = if is_host { "host" } else { "service" };
        let warn = settings.log_level_notifications <= LogLevel::Warning;
        for (argv, contact, command) in runs {
            let side = self.side_tx.clone();
            let max_output = self.options.max_output_bytes;
            tokio::spawn(async move {
                let run = runner::run(&argv, timeout, max_output).await;
                if run.timed_out && warn {
                    let _ = side.send(format!(
                        "Warning: Contact '{contact}' {kind} notification command '{command}' \
                         timed out after {} seconds",
                        timeout.as_secs()
                    ));
                }
                if let Some(error) = run.spawn_error {
                    tracing::warn!(%contact, %command, %error, "notification command not started");
                }
            });
        }
        notified
    }
}

#[cfg(test)]
mod tests;
