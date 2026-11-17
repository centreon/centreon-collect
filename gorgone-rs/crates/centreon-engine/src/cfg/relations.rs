//! Severities, tags, groups, dependencies and parents of the configuration
//! (`state_helper::expand`, the helpers' `check_validity` and the appliers'
//! resolution), once hosts and services are known.

use std::collections::HashMap;

use super::{Kind, MonitoringConfig, RawObject, Reader, parse_bool};
use crate::objects::{
    self, DependencyConf, FailOn, HostGroupConf, Key, ServiceGroupConf, SeverityConf, SeverityType,
    TagConf, TagType,
};

/// `(host_id, group names, origin)` of the `hostgroups` of the hosts.
pub(super) type HostGroupsOf = Vec<(u64, Vec<String>, String)>;
/// `(key, group names, origin)` of the `servicegroups` of the services.
pub(super) type ServiceGroupsOf = Vec<(Key, Vec<String>, String)>;

impl Reader {
    pub(super) fn relations(
        &mut self,
        config: &mut MonitoringConfig,
        raws: Vec<(Kind, RawObject)>,
        host_groups_of: &HostGroupsOf,
        service_groups_of: &ServiceGroupsOf,
    ) {
        let mut hostgroups = Vec::new();
        let mut servicegroups = Vec::new();
        let mut dependencies = Vec::new();
        for (kind, object) in raws {
            match kind {
                Kind::Severity => self.severity(&object, config),
                Kind::Tag => self.tag(&object, config),
                Kind::HostGroup => hostgroups.push(object),
                Kind::ServiceGroup => servicegroups.push(object),
                _ => dependencies.push((kind, object)),
            }
        }
        let host_ids: HashMap<String, u64> = config
            .hosts
            .iter()
            .map(|host| (host.name.clone(), host.host_id))
            .collect();
        let service_keys: HashMap<(String, String), Key> = config
            .services
            .iter()
            .map(|s| {
                (
                    (s.host_name.clone(), s.description.clone()),
                    (s.host_id, s.service_id),
                )
            })
            .collect();
        self.host_groups(config, &hostgroups, host_groups_of, &host_ids);
        self.service_groups(config, &servicegroups, service_groups_of, &service_keys);
        for (kind, object) in &dependencies {
            self.dependency(*kind, object, config, &host_ids, &service_keys);
        }
        self.check_links(config, &host_ids);
    }

    /// Parents, tags and severities must exist (`host::resolve`,
    /// `applier::host::add_object`, `applier::service::add_object`).
    fn check_links(&mut self, config: &MonitoringConfig, host_ids: &HashMap<String, u64>) {
        let mut errors = Vec::new();
        for host in &config.hosts {
            for parent in &host.parents {
                if !host_ids.contains_key(parent) {
                    errors.push(format!(
                        "'{parent}' is not a valid parent for host '{}'!",
                        host.name
                    ));
                }
            }
            for (id, kind) in &host.tags {
                if !config.tags.contains_key(&(*id, *kind)) {
                    errors.push(format!(
                        "Could not find tag '{id}' on which to apply host ({})",
                        host.host_id
                    ));
                }
            }
            if host.severity_id != 0
                && !config
                    .severities
                    .contains_key(&(host.severity_id, SeverityType::Host))
            {
                errors.push(format!(
                    "Could not add the severity ({}, 1) to the host '{}'",
                    host.severity_id, host.name
                ));
            }
        }
        for service in &config.services {
            for (id, kind) in &service.tags {
                if !config.tags.contains_key(&(*id, *kind)) {
                    errors.push(format!(
                        "Could not find tag '{id}' on which to apply service ({}, {})",
                        service.host_id, service.service_id
                    ));
                }
            }
            if service.severity_id != 0
                && !config
                    .severities
                    .contains_key(&(service.severity_id, SeverityType::Service))
            {
                errors.push(format!(
                    "Could not add the severity ({}, 0) to the service '{}' of host '{}'",
                    service.severity_id, service.description, service.host_name
                ));
            }
        }
        for dependency in config
            .host_dependencies
            .iter()
            .chain(&config.service_dependencies)
        {
            if !dependency.period.is_empty() && !config.timeperiods.contains_key(&dependency.period)
            {
                errors.push(format!(
                    "Dependency period '{}' of the dependency of ({}, {}) on ({}, {}) is not \
                     defined anywhere",
                    dependency.period,
                    dependency.dependent.0,
                    dependency.dependent.1,
                    dependency.master.0,
                    dependency.master.1
                ));
            }
        }
        errors.sort();
        errors.dedup();
        self.report.errors.extend(errors);
    }

    /// `define severity` (`severity_helper::check_validity`).
    fn severity(&mut self, object: &RawObject, config: &mut MonitoringConfig) {
        let origin = &object.origin;
        let name = object
            .fields
            .get("severity_name")
            .cloned()
            .unwrap_or_default();
        if name.is_empty() {
            return self.error(format!(
                "{origin}: Severity has no name (property 'severity_name')"
            ));
        }
        let id = self.number(object, "id");
        if id == 0 {
            return self.error(format!(
                "{origin}: Severity id must not be less than 1 (property 'severity_id')"
            ));
        }
        let level = u32::try_from(self.number(object, "level")).unwrap_or(0);
        if level == 0 {
            return self.error(format!(
                "{origin}: Severity level must not be less than 1 (property 'level')"
            ));
        }
        let Some(kind) = object
            .fields
            .get("type")
            .and_then(|kind| SeverityType::parse(kind))
        else {
            return self.error(format!(
                "{origin}: Severity type must be one of 'service' or 'host'"
            ));
        };
        let icon_id = self.number(object, "icon_id");
        let severity = SeverityConf {
            id,
            kind,
            name,
            level,
            icon_id,
        };
        if config.severities.insert((id, kind), severity).is_some() {
            self.error(format!(
                "{origin}: severity ({id}, {}) is defined twice",
                kind as u8
            ));
        }
    }

    /// `define tag` (`tag_helper::check_validity`).
    fn tag(&mut self, object: &RawObject, config: &mut MonitoringConfig) {
        let origin = &object.origin;
        let name = object.fields.get("tag_name").cloned().unwrap_or_default();
        if name.is_empty() {
            return self.error(format!("{origin}: Tag has no name (property 'tag_name')"));
        }
        let id = self.number(object, "id");
        if id == 0 {
            return self.error(format!("{origin}: Tag '{name}' has a null id"));
        }
        let Some(kind) = object
            .fields
            .get("type")
            .and_then(|kind| TagType::parse(kind))
        else {
            return self.error(format!("{origin}: Tag type must be specified"));
        };
        if config
            .tags
            .insert((id, kind), TagConf { id, kind, name })
            .is_some()
        {
            self.error(format!(
                "{origin}: tag ({id}, {}) is defined twice",
                kind as u8
            ));
        }
    }

    /// Host groups: their `members` and the `hostgroups` of the hosts
    /// (`host_helper::expand`, `hostgroup::resolve`).
    fn host_groups(
        &mut self,
        config: &mut MonitoringConfig,
        raws: &[RawObject],
        host_groups_of: &HostGroupsOf,
        host_ids: &HashMap<String, u64>,
    ) {
        for object in raws {
            let name = object
                .fields
                .get("hostgroup_name")
                .cloned()
                .unwrap_or_default();
            if name.is_empty() {
                self.error(format!(
                    "{}: Host group has no name (property 'hostgroup_name')",
                    object.origin
                ));
                continue;
            }
            let mut members = Vec::new();
            for member in objects::names(object.fields.get("members").map_or("", String::as_str)) {
                match host_ids.get(&member) {
                    Some(id) => members.push(*id),
                    None => self.error(format!(
                        "Host '{member}' specified in host group '{name}' is not defined \
                         anywhere!"
                    )),
                }
            }
            let group = HostGroupConf {
                id: self.number(object, "hostgroup_id"),
                alias: object.fields.get("alias").cloned().unwrap_or_default(),
                name: name.clone(),
                members,
            };
            if config.hostgroups.insert(name.clone(), group).is_some() {
                self.error(format!(
                    "{}: host group '{name}' is defined twice",
                    object.origin
                ));
            }
        }
        for (host_id, groups, origin) in host_groups_of {
            for group in groups {
                if let Some(group) = config.hostgroups.get_mut(group) {
                    group.members.push(*host_id);
                    continue;
                }
                let host = config
                    .hosts
                    .iter()
                    .find(|h| h.host_id == *host_id)
                    .map(|h| h.name.clone())
                    .unwrap_or_default();
                self.error(format!(
                    "{origin}: Could not add host '{host}' to non-existing host group '{group}'"
                ));
            }
        }
        for group in config.hostgroups.values_mut() {
            group.members.sort_unstable();
            group.members.dedup();
        }
    }

    /// Service groups: `members` (host, service pairs), the
    /// `servicegroups` of the services and `servicegroup_members`
    /// (`service_helper::expand`, `servicegroup_helper::expand`).
    fn service_groups(
        &mut self,
        config: &mut MonitoringConfig,
        raws: &[RawObject],
        service_groups_of: &ServiceGroupsOf,
        services: &HashMap<(String, String), Key>,
    ) {
        let mut nested: HashMap<String, Vec<String>> = HashMap::new();
        for object in raws {
            let name = object
                .fields
                .get("servicegroup_name")
                .cloned()
                .unwrap_or_default();
            if name.is_empty() {
                self.error(format!(
                    "{}: Service group has no name (property 'servicegroup_name')",
                    object.origin
                ));
                continue;
            }
            let mut members = Vec::new();
            let pairs = objects::pairs(object.fields.get("members").map_or("", String::as_str));
            for (host, service) in pairs {
                match services.get(&(host.clone(), service.clone())) {
                    Some(key) => members.push(*key),
                    None => self.error(format!(
                        "Service '{service}' on host '{host}' specified in service group \
                         '{name}' is not defined anywhere!"
                    )),
                }
            }
            if let Some(sub) = object.fields.get("servicegroup_members") {
                nested.insert(name.clone(), objects::names(sub));
            }
            let group = ServiceGroupConf {
                id: self.number(object, "servicegroup_id"),
                alias: object.fields.get("alias").cloned().unwrap_or_default(),
                name: name.clone(),
                members,
            };
            if config.servicegroups.insert(name.clone(), group).is_some() {
                self.error(format!(
                    "{}: service group '{name}' is defined twice",
                    object.origin
                ));
            }
        }
        for (key, groups, origin) in service_groups_of {
            for group in groups {
                match config.servicegroups.get_mut(group) {
                    Some(group) => group.members.push(*key),
                    None => self.error(format!(
                        "{origin}: Could not add service ({}, {}) to non-existing service group \
                         '{group}'",
                        key.0, key.1
                    )),
                }
            }
        }
        // The members of the sub-groups, transitively.
        let mut added: Vec<(String, Vec<Key>)> = Vec::new();
        for (name, subs) in &nested {
            let mut seen = vec![name.clone()];
            let mut todo = subs.clone();
            let mut members = Vec::new();
            while let Some(sub) = todo.pop() {
                if seen.contains(&sub) {
                    continue;
                }
                seen.push(sub.clone());
                let Some(group) = config.servicegroups.get(&sub) else {
                    self.error(format!(
                        "Service group '{sub}' specified in service group '{name}' is not \
                         defined anywhere!"
                    ));
                    continue;
                };
                members.extend(group.members.iter().copied());
                if let Some(more) = nested.get(&sub) {
                    todo.extend(more.iter().cloned());
                }
            }
            added.push((name.clone(), members));
        }
        for (name, members) in added {
            if let Some(group) = config.servicegroups.get_mut(&name) {
                group.members.extend(members);
            }
        }
        for group in config.servicegroups.values_mut() {
            group.members.sort_unstable();
            group.members.dedup();
        }
    }

    /// Hosts of a `hosts` list and of the members of a `hostgroups` list.
    pub(super) fn dependency_hosts(
        config: &MonitoringConfig,
        object: &RawObject,
        names_key: &str,
        groups_key: &str,
        errors: &mut Vec<String>,
    ) -> Vec<String> {
        let mut hosts = list(object, names_key);
        for group in list(object, groups_key) {
            match config.hostgroups.get(&group) {
                Some(group) => hosts.extend(
                    config
                        .hosts
                        .iter()
                        .filter(|h| group.members.contains(&h.host_id))
                        .map(|h| h.name.clone()),
                ),
                None => errors.push(format!(
                    "{}: Could not resolve host group '{group}'",
                    object.origin
                )),
            }
        }
        hosts.sort();
        hosts.dedup();
        hosts
    }

    /// One `hostdependency` or `servicedependency`, expanded to one
    /// dependency per (dependent, master) as `hostdependency_helper::expand`
    /// and `servicedependency_helper::expand` do.
    fn dependency(
        &mut self,
        kind: Kind,
        object: &RawObject,
        config: &mut MonitoringConfig,
        host_ids: &HashMap<String, u64>,
        services: &HashMap<(String, String), Key>,
    ) {
        let origin = object.origin.clone();
        let host = kind == Kind::HostDependency;
        let mut errors = Vec::new();
        let options = |key: &str, errors: &mut Vec<String>| match object.fields.get(key) {
            None => FailOn::default(),
            Some(value) => FailOn::parse(value, host).unwrap_or_else(|| {
                errors.push(format!("{origin}: invalid {key} {value:?}"));
                FailOn::default()
            }),
        };
        let execution = options("execution_failure_options", &mut errors);
        let notification = options("notification_failure_options", &mut errors);
        let inherits_parent = match object.fields.get("inherits_parent") {
            None => false,
            Some(value) => parse_bool(value).unwrap_or_else(|| {
                errors.push(format!("{origin}: invalid inherits_parent {value:?}"));
                false
            }),
        };
        let period = object
            .fields
            .get("dependency_period")
            .map(|p| p.trim().to_owned())
            .unwrap_or_default();
        let (masters, dependents) = if host {
            self.host_dependency_ends(object, config, host_ids, &mut errors)
        } else {
            self.service_dependency_ends(object, config, services, &mut errors)
        };
        self.report.errors.extend(errors);
        let target = if host {
            &mut config.host_dependencies
        } else {
            &mut config.service_dependencies
        };
        for master in &masters {
            for dependent in &dependents {
                target.push(DependencyConf {
                    dependent: *dependent,
                    master: *master,
                    execution,
                    notification,
                    inherits_parent,
                    period: period.clone(),
                });
            }
        }
    }

    /// Masters and dependents of a host dependency
    /// (`hostdependency_helper::check_validity`).
    fn host_dependency_ends(
        &self,
        object: &RawObject,
        config: &MonitoringConfig,
        host_ids: &HashMap<String, u64>,
        errors: &mut Vec<String>,
    ) -> (Vec<Key>, Vec<Key>) {
        let origin = &object.origin;
        if list(object, "hosts").is_empty() && list(object, "hostgroups").is_empty() {
            errors.push(format!(
                "{origin}: Host dependency is not attached to any host or host group \
                 (properties 'host_name' or 'hostgroup_name', respectively)"
            ));
        }
        if list(object, "dependent_hosts").is_empty()
            && list(object, "dependent_hostgroups").is_empty()
        {
            errors.push(format!(
                "{origin}: Host dependency is not attached to any dependent host or dependent \
                 host group (properties 'dependent_host_name' or 'dependent_hostgroup_name', \
                 respectively)"
            ));
        }
        let keys = |names_key: &str, groups_key: &str, errors: &mut Vec<String>| {
            Self::dependency_hosts(config, object, names_key, groups_key, errors)
                .into_iter()
                .filter_map(|name| match host_ids.get(&name) {
                    Some(id) => Some((*id, 0)),
                    None => {
                        errors.push(format!(
                            "{origin}: host '{name}' of a host dependency is not defined anywhere"
                        ));
                        None
                    }
                })
                .collect::<Vec<Key>>()
        };
        let masters = keys("hosts", "hostgroups", errors);
        let dependents = keys("dependent_hosts", "dependent_hostgroups", errors);
        (masters, dependents)
    }

    /// Masters and dependents of a service dependency
    /// (`servicedependency_helper::check_validity`, `_expand_services`).
    fn service_dependency_ends(
        &self,
        object: &RawObject,
        config: &MonitoringConfig,
        services: &HashMap<(String, String), Key>,
        errors: &mut Vec<String>,
    ) -> (Vec<Key>, Vec<Key>) {
        let origin = &object.origin;
        let is_empty = |key: &str| list(object, key).is_empty();
        if is_empty("servicegroups") {
            if is_empty("service_description") {
                errors.push(format!(
                    "{origin}: Service dependency is not attached to any service or service \
                     group (properties 'service_description' or 'servicegroup_name', \
                     respectively)"
                ));
            } else if is_empty("hosts") && is_empty("hostgroups") {
                errors.push(format!(
                    "{origin}: Service dependency is not attached to any host or host group \
                     (properties 'host_name' or 'hostgroup_name', respectively)"
                ));
            }
        }
        if is_empty("dependent_servicegroups") {
            if is_empty("dependent_service_description") {
                errors.push(format!(
                    "{origin}: Service dependency is not attached to any dependent service or \
                     dependent service group (properties 'dependent_service_description' or \
                     'dependent_servicegroup_name', respectively)"
                ));
            } else if is_empty("dependent_hosts") && is_empty("dependent_hostgroups") {
                errors.push(format!(
                    "{origin}: Service dependency is not attached to any dependent host or \
                     dependent host group (properties 'dependent_host_name' or \
                     'dependent_hostgroup_name', respectively)"
                ));
            }
        }
        let expand = |[hosts_key, groups_key, descriptions_key, servicegroups_key]: [&str; 4],
                      errors: &mut Vec<String>| {
            let hosts = Self::dependency_hosts(config, object, hosts_key, groups_key, errors);
            let mut keys = Vec::new();
            for host in &hosts {
                for description in list(object, descriptions_key) {
                    match services.get(&(host.clone(), description.clone())) {
                        Some(key) => keys.push(*key),
                        None => errors.push(format!(
                            "{origin}: service '{description}' of host '{host}' of a service \
                             dependency is not defined anywhere"
                        )),
                    }
                }
            }
            for group in list(object, servicegroups_key) {
                match config.servicegroups.get(&group) {
                    Some(group) => keys.extend(group.members.iter().copied()),
                    None => errors.push(format!(
                        "{origin}: Could not resolve service group '{group}'"
                    )),
                }
            }
            keys.sort_unstable();
            keys.dedup();
            keys
        };
        let masters = expand(
            [
                "hosts",
                "hostgroups",
                "service_description",
                "servicegroups",
            ],
            errors,
        );
        let dependents = expand(
            [
                "dependent_hosts",
                "dependent_hostgroups",
                "dependent_service_description",
                "dependent_servicegroups",
            ],
            errors,
        );
        (masters, dependents)
    }
}

/// A comma-separated field as a list of names.
fn list(object: &RawObject, key: &str) -> Vec<String> {
    object
        .fields
        .get(key)
        .map(|value| objects::names(value))
        .unwrap_or_default()
}
