//! Contacts, contact groups and escalations, and the notification, event
//! handler, flapping and freshness keys of hosts and services
//! (`contact_helper.cc`, `contactgroup_helper.cc`,
//! `hostescalation_helper.cc`, `serviceescalation_helper.cc`, the
//! appliers' resolution: `applier/contact.cc`, `contactgroup.cc`,
//! `hostescalation.cc`, `serviceescalation.cc`).

use std::collections::{BTreeMap, BTreeSet, HashMap};

use super::{Kind, MonitoringConfig, NotifyConf, RawObject, Reader, parse_bool, parse_u32};
use crate::objects::{self, ContactConf, ContactGroupConf, EscalationConf, Key, notify_on};

/// Names of a list field; a leading `+` (additive inheritance, resolved
/// with the templates) is dropped.
fn names(object: &RawObject, key: &str) -> Vec<String> {
    object
        .fields
        .get(key)
        .map(|value| objects::names(value.trim_start().trim_start_matches('+')))
        .unwrap_or_default()
}

impl Reader {
    fn flag_or(&mut self, object: &RawObject, key: &str, default: bool) -> bool {
        match object.fields.get(key) {
            None => default,
            Some(value) => parse_bool(value).unwrap_or_else(|| {
                self.error(format!("{}: invalid {key} {value:?}", object.origin));
                default
            }),
        }
    }

    fn u32_or(&mut self, object: &RawObject, key: &str, default: u32) -> u32 {
        match object.fields.get(key).map(|v| v.trim()) {
            None | Some("") => default,
            Some(value) => parse_u32(value).unwrap_or_else(|| {
                self.error(format!("{}: invalid {key} {value:?}", object.origin));
                default
            }),
        }
    }

    fn f64_or(&mut self, object: &RawObject, key: &str, default: f64) -> f64 {
        match object.fields.get(key).map(|v| v.trim()) {
            None | Some("") => default,
            Some(value) => value.parse().unwrap_or_else(|_| {
                self.error(format!("{}: invalid {key} {value:?}", object.origin));
                default
            }),
        }
    }

    fn options(
        &mut self,
        object: &RawObject,
        key: &str,
        default: u32,
        parse: impl Fn(&str) -> Option<u32>,
    ) -> u32 {
        match object.fields.get(key) {
            None => default,
            Some(value) => parse(value).unwrap_or_else(|| {
                self.error(format!("{}: invalid {key} {value:?}", object.origin));
                default
            }),
        }
    }

    /// The notification, event handler, flapping and freshness keys of a
    /// host or a service, Engine's defaults for those absent.
    pub(super) fn notify_conf(&mut self, object: &RawObject, host: bool) -> NotifyConf {
        let defaults = NotifyConf::defaults(host);
        let text = |key: &str| {
            object
                .fields
                .get(key)
                .map(|v| v.trim().to_owned())
                .unwrap_or_default()
        };
        NotifyConf {
            notifications_enabled: self.flag_or(object, "notifications_enabled", true),
            notification_interval: self.u32_or(object, "notification_interval", 0),
            first_notification_delay: self.u32_or(object, "first_notification_delay", 0),
            recovery_notification_delay: self.u32_or(object, "recovery_notification_delay", 0),
            notification_options: self.options(
                object,
                "notification_options",
                defaults.notification_options,
                |v| notify_on::parse(v, host),
            ),
            notification_period: text("notification_period"),
            contacts: names(object, "contacts"),
            contact_groups: names(object, "contact_groups"),
            event_handler: text("event_handler"),
            event_handler_enabled: self.flag_or(object, "event_handler_enabled", true),
            flap_detection_enabled: self.flag_or(object, "flap_detection_enabled", true),
            flap_detection_options: self.options(
                object,
                "flap_detection_options",
                defaults.flap_detection_options,
                |v| notify_on::parse_flap(v, host),
            ),
            low_flap_threshold: self.f64_or(object, "low_flap_threshold", 0.0),
            high_flap_threshold: self.f64_or(object, "high_flap_threshold", 0.0),
            check_freshness: self.flag_or(object, "check_freshness", false),
            freshness_threshold: self.u32_or(object, "freshness_threshold", 0),
            is_volatile: !host && self.flag_or(object, "is_volatile", false),
        }
    }

    /// Contacts, contact groups and escalations, then the references of
    /// the hosts and services to them.
    pub(super) fn notifications(
        &mut self,
        config: &mut MonitoringConfig,
        raws: Vec<(Kind, RawObject)>,
    ) {
        let mut groups_of_contacts: Vec<(String, Vec<String>)> = Vec::new();
        let mut groups = Vec::new();
        let mut escalations = Vec::new();
        for (kind, object) in raws {
            match kind {
                Kind::Contact => {
                    if let Some(contact) = self.contact(&object) {
                        groups_of_contacts
                            .push((contact.name.clone(), names(&object, "contactgroups")));
                        let name = contact.name.clone();
                        if config.contacts.insert(name.clone(), contact).is_some() {
                            self.error(format!(
                                "{}: contact '{name}' is defined twice",
                                object.origin
                            ));
                        }
                    }
                }
                Kind::ContactGroup => groups.push(object),
                _ => escalations.push((kind, object)),
            }
        }
        self.contact_groups(config, &groups, &groups_of_contacts);
        for (kind, object) in &escalations {
            self.escalation(*kind, object, config);
        }
        // The same order whatever the input lists them in.
        config.escalations.sort_by(|a, b| {
            a.key
                .cmp(&b.key)
                .then(a.first_notification.cmp(&b.first_notification))
        });
        self.check_notification_links(config);
    }

    /// `define contact` (`contact_helper::check_validity`, defaults of
    /// `set_default_values`).
    fn contact(&mut self, object: &RawObject) -> Option<ContactConf> {
        let Some(name) = object
            .fields
            .get("contact_name")
            .map(|n| n.trim().to_owned())
            .filter(|n| !n.is_empty())
        else {
            self.error(format!(
                "{}: Contact has no name (property 'contact_name')",
                object.origin
            ));
            return None;
        };
        let text = |key: &str| {
            object
                .fields
                .get(key)
                .map(|v| v.trim().to_owned())
                .unwrap_or_default()
        };
        let addresses = std::array::from_fn(|i| text(&format!("address{}", i + 1)));
        Some(ContactConf {
            alias: Some(text("alias"))
                .filter(|a| !a.is_empty())
                .unwrap_or_else(|| name.clone()),
            email: text("email"),
            pager: text("pager"),
            addresses,
            host_notifications_enabled: self.flag_or(object, "host_notifications_enabled", true),
            service_notifications_enabled: self.flag_or(
                object,
                "service_notifications_enabled",
                true,
            ),
            host_notification_options: self.options(object, "host_notification_options", 0, |v| {
                notify_on::parse(v, true)
            }),
            service_notification_options: self.options(
                object,
                "service_notification_options",
                0,
                |v| notify_on::parse(v, false),
            ),
            host_notification_period: text("host_notification_period"),
            service_notification_period: text("service_notification_period"),
            host_notification_commands: names(object, "host_notification_commands"),
            service_notification_commands: names(object, "service_notification_commands"),
            timezone: text("timezone"),
            groups: Vec::new(),
            name,
        })
    }

    /// Contact groups: `members`, the `contactgroups` of the contacts and,
    /// flattened, the members of `contactgroup_members`.
    fn contact_groups(
        &mut self,
        config: &mut MonitoringConfig,
        raws: &[RawObject],
        groups_of_contacts: &[(String, Vec<String>)],
    ) {
        let mut nested: HashMap<String, Vec<String>> = HashMap::new();
        let mut members: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
        for object in raws {
            let name = object
                .fields
                .get("contactgroup_name")
                .map(|n| n.trim().to_owned())
                .unwrap_or_default();
            if name.is_empty() {
                self.error(format!(
                    "{}: Contact group has no name (property 'contactgroup_name')",
                    object.origin
                ));
                continue;
            }
            let mut set = BTreeSet::new();
            for member in names(object, "members") {
                if config.contacts.contains_key(&member) {
                    set.insert(member);
                } else {
                    self.error(format!(
                        "Contact '{member}' specified in contact group '{name}' is not defined \
                         anywhere!"
                    ));
                }
            }
            nested.insert(name.clone(), names(object, "contactgroup_members"));
            let group = ContactGroupConf {
                alias: object
                    .fields
                    .get("alias")
                    .map(|a| a.trim().to_owned())
                    .filter(|a| !a.is_empty())
                    .unwrap_or_else(|| name.clone()),
                name: name.clone(),
                members: Vec::new(),
            };
            members.insert(name.clone(), set);
            if config.contactgroups.insert(name.clone(), group).is_some() {
                self.error(format!(
                    "{}: contact group '{name}' is defined twice",
                    object.origin
                ));
            }
        }
        for (contact, groups) in groups_of_contacts {
            for group in groups {
                match members.get_mut(group) {
                    Some(set) => {
                        set.insert(contact.clone());
                    }
                    None => self.error(format!(
                        "Could not add contact '{contact}' to non-existing contact group \
                         '{group}'"
                    )),
                }
            }
        }
        // `contactgroup_members`, recursively (loops end).
        let direct = members.clone();
        for (name, set) in &mut members {
            let mut stack = nested.get(name).cloned().unwrap_or_default();
            let mut seen = BTreeSet::from([name.clone()]);
            while let Some(child) = stack.pop() {
                if !seen.insert(child.clone()) {
                    continue;
                }
                match direct.get(&child) {
                    Some(more) => {
                        set.extend(more.iter().cloned());
                        stack.extend(nested.get(&child).cloned().unwrap_or_default());
                    }
                    None => self.error(format!(
                        "Contact group '{child}' specified in contact group '{name}' is not \
                         defined anywhere!"
                    )),
                }
            }
        }
        for (name, set) in members {
            for contact in &set {
                if let Some(contact) = config.contacts.get_mut(contact) {
                    contact.groups.push(name.clone());
                }
            }
            if let Some(group) = config.contactgroups.get_mut(&name) {
                group.members = set.into_iter().collect();
            }
        }
    }

    /// One `hostescalation` or `serviceescalation`, one per object
    /// (`applier/hostescalation.cc:79-128`, the service escalation
    /// expansion: hosts by descriptions, then service group members).
    fn escalation(&mut self, kind: Kind, object: &RawObject, config: &mut MonitoringConfig) {
        let host = kind == Kind::HostEscalation;
        let origin = object.origin.clone();
        // The helpers' default is -2: an escalation without
        // first_notification never applies.
        let first_notification = self.u32_or(object, "first_notification", u32::MAX - 1);
        let last_notification = self.u32_or(object, "last_notification", 0);
        let notification_interval = self.f64_or(object, "notification_interval", 0.0).max(0.0);
        let escalate_on = self.options(object, "escalation_options", 0, |v| {
            notify_on::parse_escalation(v, host)
        });
        let escalation_period = object
            .fields
            .get("escalation_period")
            .map(|p| p.trim().to_owned())
            .unwrap_or_default();
        let contact_groups = names(object, "contact_groups");
        let mut errors = Vec::new();
        let hosts =
            Self::dependency_hosts(config, object, "host_name", "hostgroup_name", &mut errors);
        let mut keys: Vec<Key> = Vec::new();
        if host {
            if hosts.is_empty() {
                errors.push(format!(
                    "{origin}: Host escalation is not attached to any host or host group \
                     (properties 'host_name' or 'hostgroup_name', respectively)"
                ));
            }
            for name in &hosts {
                match config.hosts.iter().find(|h| &h.name == name) {
                    Some(h) => keys.push((h.host_id, 0)),
                    None => errors.push(format!(
                        "{origin}: host '{name}' of a host escalation is not defined anywhere"
                    )),
                }
            }
        } else {
            let descriptions = names(object, "service_description");
            let groups = names(object, "servicegroup_name");
            if descriptions.is_empty() && groups.is_empty() {
                errors.push(format!(
                    "{origin}: Service escalation is not attached to any service or service \
                     group (properties 'service_description' or 'servicegroup_name', \
                     respectively)"
                ));
            }
            for name in &hosts {
                for description in &descriptions {
                    match config
                        .services
                        .iter()
                        .find(|s| &s.host_name == name && &s.description == description)
                    {
                        Some(s) => keys.push((s.host_id, s.service_id)),
                        None => errors.push(format!(
                            "{origin}: service '{description}' of host '{name}' of a service \
                             escalation is not defined anywhere"
                        )),
                    }
                }
            }
            for group in groups {
                match config.servicegroups.get(&group) {
                    Some(group) => keys.extend(group.members.iter().copied()),
                    None => errors.push(format!(
                        "{origin}: Could not resolve service group '{group}'"
                    )),
                }
            }
        }
        self.report.errors.extend(errors);
        keys.sort_unstable();
        keys.dedup();
        for key in keys {
            config.escalations.push(EscalationConf {
                key,
                first_notification,
                last_notification,
                notification_interval,
                escalation_period: escalation_period.clone(),
                escalate_on,
                contact_groups: contact_groups.clone(),
            });
        }
    }

    /// Contacts, contact groups, periods and commands that must exist
    /// (`applier::contact::resolve_object`, `host::resolve`,
    /// `service::resolve`, `escalation::resolve`).
    fn check_notification_links(&mut self, config: &MonitoringConfig) {
        let mut errors = Vec::new();
        let period = |name: &str, what: &str, errors: &mut Vec<String>| {
            if !name.is_empty() && !config.timeperiods.contains_key(name) {
                errors.push(format!("{what} '{name}' is not defined anywhere"));
            }
        };
        let command = |line: &str, what: &str, errors: &mut Vec<String>| {
            let name = super::command_name(line);
            if !name.is_empty() && !config.commands.contains_key(name) {
                errors.push(format!("{what} command '{name}' is not defined anywhere"));
            }
        };
        for contact in config.contacts.values() {
            let who = format!("Contact '{}'", contact.name);
            period(
                &contact.host_notification_period,
                &format!("{who}: host notification period"),
                &mut errors,
            );
            period(
                &contact.service_notification_period,
                &format!("{who}: service notification period"),
                &mut errors,
            );
            for name in &contact.host_notification_commands {
                if !config.commands.contains_key(name) {
                    errors.push(format!(
                        "{who}: host notification command '{name}' is not defined anywhere"
                    ));
                }
            }
            for name in &contact.service_notification_commands {
                if !config.commands.contains_key(name) {
                    errors.push(format!(
                        "{who}: service notification command '{name}' is not defined anywhere"
                    ));
                }
            }
        }
        let objects = config
            .hosts
            .iter()
            .map(|h| (&h.notify, format!("host '{}'", h.name)))
            .chain(config.services.iter().map(|s| {
                (
                    &s.notify,
                    format!("service '{}' on host '{}'", s.description, s.host_name),
                )
            }));
        for (notify, what) in objects {
            for contact in &notify.contacts {
                if !config.contacts.contains_key(contact) {
                    errors.push(format!(
                        "Contact '{contact}' specified in {what} is not defined anywhere!"
                    ));
                }
            }
            for group in &notify.contact_groups {
                if !config.contactgroups.contains_key(group) {
                    errors.push(format!(
                        "Contact group '{group}' specified in {what} is not defined anywhere!"
                    ));
                }
            }
            period(
                &notify.notification_period,
                &format!("Notification period of {what}:"),
                &mut errors,
            );
            command(
                &notify.event_handler,
                &format!("Event handler of {what}:"),
                &mut errors,
            );
        }
        for escalation in &config.escalations {
            let what = format!("Escalation of ({}, {})", escalation.key.0, escalation.key.1);
            for group in &escalation.contact_groups {
                if !config.contactgroups.contains_key(group) {
                    errors.push(format!(
                        "Contact group '{group}' specified in {what} is not defined anywhere!"
                    ));
                }
            }
            period(
                &escalation.escalation_period,
                &format!("{what}: escalation period"),
                &mut errors,
            );
        }
        let settings = &config.settings;
        command(
            &settings.global_host_event_handler,
            "Global host event handler",
            &mut errors,
        );
        command(
            &settings.global_service_event_handler,
            "Global service event handler",
            &mut errors,
        );
        errors.sort();
        errors.dedup();
        self.report.errors.extend(errors);
    }
}

#[cfg(test)]
mod tests {
    use crate::cfg;
    use crate::objects::notify_on;

    #[test]
    fn reads_contacts_groups_and_escalations() {
        let dir = tempfile::tempdir().unwrap();
        let main = cfg::tests::web_like(dir.path());
        let (config, report) = cfg::read(&main);
        assert!(report.errors.is_empty(), "{report:?}");
        let admin = &config.contacts["admin"];
        assert_eq!(admin.email, "admin@localhost");
        assert_eq!(admin.alias, "Administrator");
        assert_eq!(
            admin.service_notification_options,
            notify_on::WARNING | notify_on::CRITICAL | notify_on::OK | notify_on::FLAPPING
        );
        assert_eq!(admin.service_notification_commands, ["notify"]);
        assert_eq!(admin.groups, ["ops", "team"]);
        // Nested group members are flattened.
        assert_eq!(config.contactgroups["ops"].members, ["admin", "guest"]);
        assert_eq!(config.contactgroups["team"].members, ["admin"]);
        // A template sets the commands of `guest`.
        assert_eq!(
            config.contacts["guest"].host_notification_commands,
            ["notify"]
        );
        let h1 = &config.hosts[0];
        assert_eq!(h1.notify.contacts, ["admin"]);
        assert_eq!(h1.notify.notification_interval, 10);
        assert_eq!(
            h1.notify.notification_options,
            notify_on::DOWN | notify_on::UP
        );
        // A service without contacts takes those of its host, and its interval.
        let ping = &config.services[0];
        assert_eq!(ping.notify.contacts, ["admin"]);
        assert_eq!(ping.notify.notification_interval, 10);
        assert_eq!(ping.notify.event_handler, "check_dummy!0!handled");
        assert!(ping.notify.check_freshness);
        assert_eq!(ping.notify.high_flap_threshold, 50.0);
        let disk = &config.services[1];
        assert_eq!(disk.notify.contact_groups, ["ops"]);
        assert!(disk.notify.contacts.is_empty());
        assert_eq!(config.escalations.len(), 2);
        let escalation = &config.escalations[1];
        assert_eq!(escalation.key, (13, 102));
        assert_eq!(
            (escalation.first_notification, escalation.last_notification),
            (2, 0)
        );
        assert_eq!(escalation.escalate_on, notify_on::CRITICAL | notify_on::OK);
        assert_eq!(config.escalations[0].key, (12, 0));
        assert!(config.settings.enable_flap_detection);
        assert_eq!(config.settings.notification_timeout, 12);
    }

    #[test]
    fn refuses_unknown_contacts() {
        let dir = tempfile::tempdir().unwrap();
        let main = cfg::tests::web_like(dir.path());
        let path = dir.path().join("contacts.cfg");
        let text = std::fs::read_to_string(&path).unwrap();
        std::fs::write(
            &path,
            text.replace("members guest", "members nobody").replace(
                "service_notification_commands notify",
                "service_notification_commands nope",
            ),
        )
        .unwrap();
        let hosts = dir.path().join("hosts.cfg");
        let text = std::fs::read_to_string(&hosts).unwrap();
        std::fs::write(&hosts, text.replace("contacts admin", "contacts ghost")).unwrap();
        let (_, report) = cfg::read(&main);
        let all = report.errors.join("\n");
        for expected in [
            "Contact 'nobody' specified in contact group 'ops' is not defined anywhere!",
            "Contact 'admin': service notification command 'nope' is not defined anywhere",
            "Contact 'ghost' specified in host 'h1' is not defined anywhere!",
        ] {
            assert!(all.contains(expected), "{expected}: {all}");
        }
    }
}
