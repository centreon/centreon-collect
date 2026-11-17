//! What relates objects at run time: parents and reachability
//! (`host::determine_host_reachability`, the propagated checks of
//! `host::process_check_result_3x`), on-demand checks, execution
//! dependencies (`authorized_by_dependencies`), the events of groups,
//! severities, tags and parents (`engine/src/broker.cc`, the appliers of
//! `engine/src/configuration/applier/`) and the event log lines.

use std::collections::{BTreeMap, HashMap};

use crate::cfg::{HostConf, MonitoringConfig};
use crate::events::{self, Action};
use crate::logs;
use crate::objects::{DependencyConf, HostGroupConf, ServiceGroupConf};
use crate::state::{CheckState, StateType, code};

use super::{Changes, Core, Key, ObjConf};

/// Log lines kept until the first connection: Engine's `cbmod` keeps
/// what it is given before it reaches the central.
const MAX_EARLY_LOGS: usize = 10_000;

/// Parents, children and dependencies by object index.
#[derive(Debug, Default)]
pub(super) struct Links {
    pub parents: Vec<Vec<usize>>,
    pub children: Vec<Vec<usize>>,
    /// Dependencies of each dependent object, with the index of its master.
    pub masters: Vec<Vec<(usize, DependencyConf)>>,
}

/// The configuration events of a reload, in Engine's order.
#[derive(Debug, Default)]
pub(super) struct RelationFrames {
    /// Severities then tags: modified, removed, added
    /// (`applier::state::_apply`, `severity.cc`, `tag.cc`).
    pub before_objects: Vec<Vec<u8>>,
    /// Group memberships of the removed services.
    pub removed_services: Vec<Vec<u8>>,
    /// Group memberships and parents of the removed hosts
    /// (`applier::host::remove_object`, `host.cc:410-436`).
    pub removed_hosts: Vec<Vec<u8>>,
    /// Old parents of the hosts whose parents changed (`host.cc:359-368`).
    pub changed_parents: Vec<Vec<u8>>,
    /// Host groups: modified, removed, added (`applier/hostgroup.cc`).
    pub hostgroups: Vec<Vec<u8>>,
    /// Service groups (`applier/servicegroup.cc`).
    pub servicegroups: Vec<Vec<u8>>,
    /// Resolution: every parent, then the members of the new or changed
    /// groups (`host::resolve`, `hostgroup::resolve`,
    /// `servicegroup::resolve`).
    pub resolve: Vec<Vec<u8>>,
}

/// Group events of the snapshot: the group, then each member
/// (`send_host_group_list`, `send_service_group_list`).
fn host_group_frames(poller_id: u64, group: &HostGroupConf, frames: &mut Vec<Vec<u8>>) {
    if group.id == 0 {
        return;
    }
    frames.push(events::host_group(poller_id, group, true));
    host_group_members(poller_id, group, true, frames);
}

fn host_group_members(
    poller_id: u64,
    group: &HostGroupConf,
    enabled: bool,
    frames: &mut Vec<Vec<u8>>,
) {
    if group.id == 0 {
        return;
    }
    for host_id in &group.members {
        frames.push(events::host_group_member(
            poller_id, group, *host_id, enabled,
        ));
    }
}

fn service_group_frames(poller_id: u64, group: &ServiceGroupConf, frames: &mut Vec<Vec<u8>>) {
    if group.id == 0 {
        return;
    }
    frames.push(events::service_group(poller_id, group, true));
    service_group_members(poller_id, group, true, frames);
}

fn service_group_members(
    poller_id: u64,
    group: &ServiceGroupConf,
    enabled: bool,
    frames: &mut Vec<Vec<u8>>,
) {
    if group.id == 0 {
        return;
    }
    for key in &group.members {
        frames.push(events::service_group_member(
            poller_id, group, *key, enabled,
        ));
    }
}

/// Parent events of one host (`send_host_parents_list`).
fn parent_frames(
    host: &HostConf,
    ids: &HashMap<&str, u64>,
    enabled: bool,
    frames: &mut Vec<Vec<u8>>,
) {
    for parent in &host.parents {
        if let Some(parent_id) = ids.get(parent.as_str()) {
            frames.push(events::host_parent(host.host_id, *parent_id, enabled));
        }
    }
}

fn host_ids(config: &MonitoringConfig) -> HashMap<&str, u64> {
    config
        .hosts
        .iter()
        .map(|host| (host.name.as_str(), host.host_id))
        .collect()
}

/// Modified, removed, then added entries of two keyed maps.
fn keyed_diff<'a, K: Ord, V: PartialEq>(
    old: &'a BTreeMap<K, V>,
    new: &'a BTreeMap<K, V>,
) -> (Vec<&'a V>, Vec<&'a V>, Vec<&'a V>) {
    let modified = new
        .iter()
        .filter(|(key, value)| old.get(*key).is_some_and(|before| before != *value))
        .map(|(_, value)| value)
        .collect();
    let removed = old
        .iter()
        .filter(|(key, _)| !new.contains_key(*key))
        .map(|(_, value)| value)
        .collect();
    let added = new
        .iter()
        .filter(|(key, _)| !old.contains_key(*key))
        .map(|(_, value)| value)
        .collect();
    (modified, removed, added)
}

/// The configuration events Engine sends at start
/// (`send_initial_configuration`, `engine/src/broker.cc:4135-4153`), but
/// for hosts, services, downtimes and comments: severities and tags first,
/// then parents and groups.
pub(super) fn snapshot_frames(
    poller_id: u64,
    config: &MonitoringConfig,
) -> (Vec<Vec<u8>>, Vec<Vec<u8>>) {
    let mut before = Vec::new();
    for severity in config.severities.values() {
        before.push(events::severity(poller_id, severity, Action::Add));
    }
    for tag in config.tags.values() {
        before.push(events::tag(poller_id, tag, Action::Add));
    }
    let mut after = Vec::new();
    let ids = host_ids(config);
    for host in &config.hosts {
        parent_frames(host, &ids, true, &mut after);
    }
    for group in config.hostgroups.values() {
        host_group_frames(poller_id, group, &mut after);
    }
    for group in config.servicegroups.values() {
        service_group_frames(poller_id, group, &mut after);
    }
    (before, after)
}

/// The events of a reload, from the configuration in force to the new one.
pub(super) fn change_frames(
    poller_id: u64,
    old: &MonitoringConfig,
    new: &MonitoringConfig,
    changes: &Changes,
) -> RelationFrames {
    let mut frames = RelationFrames::default();
    let (modified, removed, added) = keyed_diff(&old.severities, &new.severities);
    for (list, action) in [
        (modified, Action::Modify),
        (removed, Action::Delete),
        (added, Action::Add),
    ] {
        for severity in list {
            frames
                .before_objects
                .push(events::severity(poller_id, severity, action));
        }
    }
    let (modified, removed, added) = keyed_diff(&old.tags, &new.tags);
    for (list, action) in [
        (modified, Action::Modify),
        (removed, Action::Delete),
        (added, Action::Add),
    ] {
        for tag in list {
            frames
                .before_objects
                .push(events::tag(poller_id, tag, action));
        }
    }
    let old_ids = host_ids(old);
    for conf in &changes.removed {
        match conf {
            ObjConf::Service { conf, .. } => {
                let key = (conf.host_id, conf.service_id);
                for group in old.servicegroups.values() {
                    if group.id != 0 && group.members.contains(&key) {
                        frames
                            .removed_services
                            .push(events::service_group_member(poller_id, group, key, false));
                    }
                }
            }
            ObjConf::Host(host) => {
                for group in old.hostgroups.values() {
                    if group.id != 0 && group.members.contains(&host.host_id) {
                        frames.removed_hosts.push(events::host_group_member(
                            poller_id,
                            group,
                            host.host_id,
                            false,
                        ));
                    }
                }
                parent_frames(host, &old_ids, false, &mut frames.removed_hosts);
            }
        }
    }
    let old_hosts: HashMap<u64, &HostConf> =
        old.hosts.iter().map(|host| (host.host_id, host)).collect();
    for key in &changes.changed {
        let Some(before) = old_hosts.get(&key.0).filter(|_| key.1 == 0) else {
            continue;
        };
        let after = new.hosts.iter().find(|host| host.host_id == key.0);
        if after.is_some_and(|after| after.parents != before.parents) {
            parent_frames(before, &old_ids, false, &mut frames.changed_parents);
        }
    }
    // Groups: a changed member list sends the old members as removed, the
    // group, then every member again at resolution.
    let mut resolve_members = Vec::new();
    for (name, group) in &new.hostgroups {
        match old.hostgroups.get(name) {
            Some(before) if before == group => {}
            Some(before) => {
                if before.members != group.members {
                    let renamed = HostGroupConf {
                        members: before.members.clone(),
                        ..group.clone()
                    };
                    host_group_members(poller_id, &renamed, false, &mut frames.hostgroups);
                    host_group_members(poller_id, group, true, &mut resolve_members);
                }
                if group.id != 0 {
                    frames.hostgroups.push(events::host_group(
                        poller_id,
                        group,
                        !group.members.is_empty(),
                    ));
                }
            }
            None => {}
        }
    }
    for (name, group) in &old.hostgroups {
        if !new.hostgroups.contains_key(name) && group.id != 0 {
            frames
                .hostgroups
                .push(events::host_group(poller_id, group, false));
        }
    }
    for (name, group) in &new.hostgroups {
        if !old.hostgroups.contains_key(name) && group.id != 0 {
            frames
                .hostgroups
                .push(events::host_group(poller_id, group, true));
            host_group_members(poller_id, group, true, &mut resolve_members);
        }
    }
    let mut resolve_service_members = Vec::new();
    for (name, group) in &new.servicegroups {
        match old.servicegroups.get(name) {
            Some(before) if before == group => {}
            Some(before) => {
                if before.members != group.members {
                    let renamed = ServiceGroupConf {
                        members: before.members.clone(),
                        ..group.clone()
                    };
                    service_group_members(poller_id, &renamed, false, &mut frames.servicegroups);
                    service_group_members(poller_id, group, true, &mut resolve_service_members);
                }
                if group.id != 0 {
                    frames.servicegroups.push(events::service_group(
                        poller_id,
                        group,
                        !group.members.is_empty(),
                    ));
                }
            }
            None => {}
        }
    }
    for (name, group) in &old.servicegroups {
        if !new.servicegroups.contains_key(name) && group.id != 0 {
            frames
                .servicegroups
                .push(events::service_group(poller_id, group, false));
        }
    }
    for (name, group) in &new.servicegroups {
        if !old.servicegroups.contains_key(name) && group.id != 0 {
            frames
                .servicegroups
                .push(events::service_group(poller_id, group, true));
            service_group_members(poller_id, group, true, &mut resolve_service_members);
        }
    }
    // `host::resolve` runs for every host: every parent is sent again.
    let new_ids = host_ids(new);
    for host in &new.hosts {
        parent_frames(host, &new_ids, true, &mut frames.resolve);
    }
    frames.resolve.extend(resolve_members);
    frames.resolve.extend(resolve_service_members);
    frames
}

impl Core {
    /// Parents, children and dependencies of the objects now in force.
    pub(super) fn index_links(&mut self, config: &MonitoringConfig) {
        let count = self.objs.len();
        let mut links = Links {
            parents: vec![Vec::new(); count],
            children: vec![Vec::new(); count],
            masters: vec![Vec::new(); count],
        };
        for (index, obj) in self.objs.iter().enumerate() {
            let ObjConf::Host(host) = &obj.conf else {
                continue;
            };
            for parent in &host.parents {
                if let Some(&parent_index) = self.hosts_by_name.get(parent) {
                    links.parents[index].push(parent_index);
                    links.children[parent_index].push(index);
                }
            }
        }
        for dependency in config
            .host_dependencies
            .iter()
            .chain(&config.service_dependencies)
        {
            let (Some(&dependent), Some(&master)) = (
                self.by_key.get(&dependency.dependent),
                self.by_key.get(&dependency.master),
            ) else {
                continue;
            };
            links.masters[dependent].push((master, dependency.clone()));
        }
        self.links = links;
    }

    /// `determine_host_reachability`: a host not UP is DOWN when it has no
    /// parent or one parent UP, else UNREACHABLE.
    pub(super) fn reachability(&self, index: usize) -> u32 {
        let parents = &self.links.parents[index];
        if parents.is_empty()
            || parents
                .iter()
                .any(|parent| self.objs[*parent].state.state == code::OK)
        {
            code::DOWN
        } else {
            code::UNREACHABLE
        }
    }

    /// `authorized_by_dependencies(execution)` (`service.cc:3613-3662`,
    /// `host.cc:3706-3755`), quirks included: a dependency outside its
    /// period authorizes everything.
    pub(super) fn authorized_by_dependencies(&mut self, index: usize, depth: usize) -> bool {
        if depth > self.objs.len() {
            return true;
        }
        let soft_state_dependencies = self
            .loaded
            .as_ref()
            .is_some_and(|l| l.config.settings.soft_state_dependencies);
        let masters = self.links.masters[index].clone();
        for (master, dependency) in masters {
            if !dependency.period.is_empty() && !self.in_dependency_period(&dependency.period) {
                return true;
            }
            let st = &self.objs[master].state;
            let state = if st.state_type == StateType::Soft && !soft_state_dependencies {
                st.last_hard_state
            } else {
                st.state
            };
            if dependency.execution.on(state) {
                return false;
            }
            if state == code::OK && !st.checked && dependency.execution.pending {
                return false;
            }
            if dependency.inherits_parent && !self.authorized_by_dependencies(master, depth + 1) {
                return false;
            }
        }
        true
    }

    /// `check_time_against_period` of a dependency period, in the zone of
    /// the process (`use_timezone`): Engine takes no object's zone there.
    fn in_dependency_period(&mut self, period: &str) -> bool {
        let now = self.clock.now();
        let Some(loaded) = self.loaded.as_ref() else {
            return true;
        };
        let zone = loaded.config.settings.use_timezone.clone();
        let tz = self.zones.get(&zone);
        let periods = &loaded.config.timeperiods;
        crate::timeperiod::Calc { tz: &tz, periods }.contains(now, periods.get(period))
    }

    /// An on-demand check (`run_async_check(CHECK_OPTION_NONE)` of
    /// `process_check_result_3x` and `handle_async_check_result`): run now
    /// unless running, without a check command, or `cached` and checked
    /// within `cached_host_check_horizon`.
    pub(super) fn check_on_demand(&mut self, index: usize, cached: bool) {
        let horizon = self.loaded.as_ref().map_or(15, |l| {
            i64::from(l.config.settings.cached_host_check_horizon)
        });
        let now = self.clock.now();
        let obj = &self.objs[index];
        if obj.running || obj.check().check_command.is_empty() || !obj.check().active_checks {
            return;
        }
        if cached && obj.state.checked && now - obj.state.last_check <= horizon {
            return;
        }
        let at = tokio::time::Instant::now();
        if obj.due.is_some_and(|due| due <= at) {
            return;
        }
        tracing::debug!(
            host_id = obj.key.0,
            service_id = obj.key.1,
            "on-demand check"
        );
        self.schedule(index, at);
    }

    /// The checks Engine propagates after a host result
    /// (`process_check_result_3x`, `host.cc:3154-3380`).
    pub(super) fn propagate_host_checks(&mut self, index: usize, was_up: bool) {
        let obj = &self.objs[index];
        let up = obj.state.state == code::OK;
        let max = obj.check().max_check_attempts;
        let attempt = obj.state.attempt;
        let parents = self.links.parents[index].clone();
        let children = self.links.children[index].clone();
        let state_of = |core: &Self, i: usize| core.objs[i].state.state;
        let mut list = Vec::new();
        if !was_up && up {
            list.extend(parents.iter().filter(|p| state_of(self, **p) != code::OK));
            list.extend(children.iter().filter(|c| state_of(self, **c) != code::OK));
        } else if was_up && !up {
            if max > 1 {
                list.extend(parents.iter().filter(|p| state_of(self, **p) == code::OK));
            }
            list.extend(
                children
                    .iter()
                    .filter(|c| state_of(self, **c) != code::UNREACHABLE),
            );
            let predictive = self
                .loaded
                .as_ref()
                .is_some_and(|l| l.config.settings.enable_predictive_host_dependency_checks);
            if max > 1 && predictive && attempt == max - 1 {
                list.extend(self.links.masters[index].iter().map(|(m, _)| *m));
            }
        }
        for target in list {
            self.check_on_demand(target, true);
        }
    }

    /// The host checks and predictive dependency checks Engine starts
    /// after a service result (`service.cc:1408-1422`, `:1580-1620`,
    /// `:1700-1760`, `:1840-1870`).
    pub(super) fn propagate_service_checks(&mut self, index: usize, state_change: bool) {
        let ObjConf::Service { host, .. } = self.objs[index].conf else {
            return;
        };
        let now = self.clock.now();
        let service = &self.objs[index];
        let host_obj = &self.objs[host];
        let host_up = host_obj.state.state == code::OK;
        if service.is_ok() {
            // A host never checked and not planned within 5 minutes.
            let never = !host_obj.state.checked
                && (host_obj.state.next_check == 0 || host_obj.state.next_check - now > 300);
            if never {
                self.check_on_demand(host, false);
            } else if !host_up {
                self.check_on_demand(host, true);
            }
            return;
        }
        // A problem: the route to the host, when it is up and the service
        // changed state (`state_changes_use_cached_state`).
        if host_up && state_change {
            self.check_on_demand(host, true);
        }
        let service = &self.objs[index];
        let (attempt, max) = (service.state.attempt, service.check().max_check_attempts);
        let predictive = self.loaded.as_ref().is_some_and(|l| {
            l.config
                .settings
                .enable_predictive_service_dependency_checks
        });
        if predictive && attempt < max && attempt == max - 1 {
            let masters: Vec<usize> = self.links.masters[index].iter().map(|(m, _)| *m).collect();
            for master in masters {
                self.check_on_demand(master, false);
            }
        }
    }

    /// Sends one line of the event log, or keeps it until the first
    /// connection.
    pub(super) fn log(&mut self, line: &str) {
        let Some((_, name)) = self.poller() else {
            return;
        };
        let ids = |host: &str, service: Option<&str>| -> u64 {
            match service {
                None => self
                    .hosts_by_name
                    .get(host)
                    .map_or(0, |index| self.objs[*index].key.0),
                Some(service) => self
                    .services_by_name
                    .get(&(host.to_owned(), service.to_owned()))
                    .map_or(0, |index| self.objs[*index].key.1),
            }
        };
        let frame = logs::entry(&name, self.clock.now(), line, &ids);
        if self.ever_connected {
            self.emit(frame);
        } else if self.early_logs.len() < MAX_EARLY_LOGS {
            self.early_logs.push(frame);
        }
    }

    /// Settings of the event log, defaults when nothing is loaded.
    fn log_settings(&self) -> crate::cfg::Settings {
        self.loaded
            .as_ref()
            .map(|l| l.config.settings.clone())
            .unwrap_or_default()
    }

    /// `INITIAL HOST STATE` / `INITIAL SERVICE STATE` of the objects a
    /// configuration added (`applier::state::_processing`,
    /// `state.cc:1621-1634`).
    pub(super) fn log_initial_states(&mut self, added: &[Key]) {
        if !self.log_settings().log_level_events.logs_info() {
            return;
        }
        let mut indexes: Vec<usize> = added
            .iter()
            .filter_map(|key| self.by_key.get(key).copied())
            .collect();
        indexes.sort_by_key(|index| (!self.objs[*index].is_host(), *index));
        for index in indexes {
            let line = self.state_line(index, true);
            self.log(&line);
        }
    }

    /// The alert or initial state line of object `index`.
    fn state_line(&self, index: usize, initial: bool) -> String {
        let obj = &self.objs[index];
        let st: &CheckState = &obj.state;
        let hard = st.state_type == StateType::Hard;
        match &obj.conf {
            ObjConf::Host(host) if initial => {
                logs::host_initial_state(&host.name, st.state, hard, st.attempt, &st.output)
            }
            ObjConf::Host(host) => {
                logs::host_alert(&host.name, st.state, hard, st.attempt, &st.output)
            }
            ObjConf::Service { conf, .. } if initial => logs::service_initial_state(
                &conf.host_name,
                &conf.description,
                st.state,
                hard,
                st.attempt,
                &st.output,
            ),
            ObjConf::Service { conf, .. } => logs::service_alert(
                &conf.host_name,
                &conf.description,
                st.state,
                hard,
                st.attempt,
                &st.output,
            ),
        }
    }

    /// `log_event` of a service (`service.cc:2129-2151`): SOFT lines only
    /// with `log_service_retries`.
    pub(super) fn log_service_alert(&mut self, index: usize) {
        let settings = self.log_settings();
        let soft = self.objs[index].state.state_type == StateType::Soft;
        if (soft && !settings.log_service_retries) || !settings.log_level_events.logs_info() {
            return;
        }
        let line = self.state_line(index, false);
        self.log(&line);
    }

    /// `handle_state` of a host (`host.cc:2358-2500`): a change of state
    /// logged when HARD, or SOFT with `log_host_retries`; a SOFT result
    /// without change with `log_host_retries`.
    pub(super) fn log_host_alert(&mut self, index: usize, last_state: u32, last_hard_state: u32) {
        let settings = self.log_settings();
        if !settings.log_level_events.logs_info() {
            return;
        }
        let st = &self.objs[index].state;
        let soft = st.state_type == StateType::Soft;
        let state_change =
            last_state != st.state || last_hard_state != st.state || (st.state == code::OK && soft);
        let logged = if state_change {
            !soft || settings.log_host_retries
        } else {
            soft && settings.log_host_retries
        };
        if logged {
            let line = self.state_line(index, false);
            self.log(&line);
        }
    }

    /// `PASSIVE SERVICE CHECK` (`service.cc:1396-1404`): at INFO whatever
    /// `log_passive_checks` says, which only drives the legacy log.
    pub(super) fn log_passive_service_check(&mut self, index: usize, state: u32, output: &str) {
        if !self.log_settings().log_level_checks.logs_info() {
            return;
        }
        if let ObjConf::Service { conf, .. } = &self.objs[index].conf {
            let line =
                logs::passive_service_check(&conf.host_name, &conf.description, state, output);
            self.log(&line);
        }
    }

    /// `EXTERNAL COMMAND: name;args` (`processing.cc:878-891`): check
    /// results with `log_passive_checks` on the checks logger, the others
    /// with `log_external_commands` on the external command logger.
    pub(super) fn log_external_command(&mut self, name: &str, args: &str) {
        let settings = self.log_settings();
        let logged = if matches!(
            name,
            "PROCESS_SERVICE_CHECK_RESULT" | "PROCESS_HOST_CHECK_RESULT"
        ) {
            settings.log_passive_checks && settings.log_level_checks.logs_info()
        } else {
            settings.log_external_commands && settings.log_level_external_command.logs_info()
        };
        if logged {
            self.log(&logs::external_command(name, args));
        }
    }

    /// `HOST DOWNTIME ALERT` / `SERVICE DOWNTIME ALERT` of object `index`.
    pub(super) fn log_downtime_alert(&mut self, index: usize, alert: logs::DowntimeAlert) {
        if !self.log_settings().log_level_events.logs_info() {
            return;
        }
        let line = match &self.objs[index].conf {
            ObjConf::Host(host) => logs::host_downtime_alert(&host.name, alert),
            ObjConf::Service { conf, .. } => {
                logs::service_downtime_alert(&conf.host_name, &conf.description, alert)
            }
        };
        self.log(&line);
    }

    /// The acknowledgement of object `index` expired.
    pub(super) fn log_acknowledgement_expired(&mut self, index: usize) {
        if !self.log_settings().log_level_events.logs_info() {
            return;
        }
        let line = match &self.objs[index].conf {
            ObjConf::Host(host) => logs::host_acknowledgement_expired(&host.name),
            ObjConf::Service { conf, .. } => {
                logs::service_acknowledgement_expired(&conf.host_name, &conf.description)
            }
        };
        self.log(&line);
    }
}

#[cfg(test)]
mod tests {
    use std::path::Path;
    use std::time::Duration;

    use gorgone_proto::bbdo::{
        HostGroup, HostGroupMember, HostParent, HostStatus, LogEntry, Service, ServiceGroupMember,
        Severity, Tag,
    };
    use prost::Message;
    use tokio::sync::mpsc;

    use super::super::tests::core;
    use super::*;
    use crate::bbdo::{Decoder, event};
    use crate::client::Outgoing;

    /// A parent `p`, its child `c` and `c2` (all with a check command, so
    /// that on-demand checks can run), services `s1` (master) and `s2`
    /// (dependent) on `c`, groups, tags and severities.
    fn write(dir: &Path, members: &str, extra: &str) -> std::path::PathBuf {
        let w = |name: &str, text: String| std::fs::write(dir.join(name), text).unwrap();
        w(
            "centengine.cfg",
            "cfg_file=objects.cfg\ninterval_length=60\nlog_service_retries=1\n\
             log_level_external_command=info\n"
                .to_owned(),
        );
        w(
            "objects.cfg",
            format!(
                "define command {{\n command_name ok\n command_line /bin/true\n}}\n\
                 define host {{\n host_name p\n _HOST_ID 1\n check_command ok\n \
                 max_check_attempts 1\n}}\n\
                 define host {{\n host_name c\n _HOST_ID 2\n check_command ok\n parents p\n \
                 max_check_attempts 1\n severity 5\n category_tags 8\n}}\n\
                 define host {{\n host_name c2\n _HOST_ID 3\n check_command ok\n parents p\n}}\n\
                 define service {{\n host_name c\n service_description s1\n check_command ok\n \
                 max_check_attempts 1\n _SERVICE_ID 1\n}}\n\
                 define service {{\n host_name c\n service_description s2\n check_command ok\n \
                 max_check_attempts 2\n _SERVICE_ID 2\n acknowledgement_timeout 2\n}}\n\
                 define servicedependency {{\n host_name c\n service_description s1\n \
                 dependent_host_name c\n dependent_service_description s2\n \
                 execution_failure_criteria c\n}}\n\
                 define hostgroup {{\n hostgroup_id 7\n hostgroup_name hg\n alias HG\n \
                 members {members}\n}}\n\
                 define servicegroup {{\n servicegroup_id 9\n servicegroup_name sg\n \
                 members c,s1,c,s2\n}}\n\
                 define severity {{\n id 5\n severity_name sev\n level 2\n type host\n}}\n\
                 define tag {{\n id 8\n tag_name cat\n type hostcategory\n}}\n{extra}"
            ),
        );
        w(
            "module.json",
            r#"{"centreonBroker":{"poller_id":7,"poller_name":"p","broker_name":"p-module",
               "output":[{"type":"ipv4","host":"127.0.0.1","port":"1"}]}}"#
                .to_owned(),
        );
        dir.join("centengine.cfg")
    }

    /// Every event sent so far.
    fn sent(out: &mut mpsc::Receiver<Outgoing>) -> Vec<(u32, Vec<u8>)> {
        let mut decoder = Decoder::default();
        while let Ok(outgoing) = out.try_recv() {
            if let Outgoing::Event(frame) = outgoing {
                decoder.push(&frame);
            }
        }
        std::iter::from_fn(|| decoder.next_event()).collect()
    }

    fn logs(events: &[(u32, Vec<u8>)]) -> Vec<LogEntry> {
        events
            .iter()
            .filter(|(kind, _)| *kind == event::LOG_ENTRY)
            .map(|(_, payload)| LogEntry::decode(payload.as_slice()).unwrap())
            .collect()
    }

    fn index(core: &Core, key: Key) -> usize {
        core.by_key[&key]
    }

    /// The check of `key` is due now (planned now, or waiting for room).
    fn due_now(core: &Core, key: Key) -> bool {
        let i = index(core, key);
        let now = tokio::time::Instant::now();
        let seq = core.objs[i].seq;
        core.objs[i].due == Some(now)
            || core
                .ready
                .iter()
                .any(|(index, s, _)| *index == i && *s == seq)
    }

    #[tokio::test(start_paused = true)]
    async fn a_child_of_a_down_parent_is_unreachable() {
        let dir = tempfile::tempdir().unwrap();
        let main = write(dir.path(), "c", "");
        let (mut core, mut out, _target) = core(dir.path(), main);
        core.load().unwrap();
        core.ever_connected = true;
        sent(&mut out);
        // Nothing planned soon, so that the on-demand checks show.
        let later = tokio::time::Instant::now() + Duration::from_secs(3600);
        for key in [(1, 0), (2, 0), (3, 0)] {
            let i = index(&core, key);
            core.schedule(i, later);
        }

        core.command("PROCESS_HOST_CHECK_RESULT;p;1;down").unwrap();
        let parent = &core.objs[index(&core, (1, 0))].state;
        assert_eq!((parent.state, parent.state_type), (1, StateType::Hard));
        // The children of a host going down are checked at once.
        assert!(due_now(&core, (2, 0)));
        assert!(due_now(&core, (3, 0)));

        core.command("PROCESS_HOST_CHECK_RESULT;c;1;no route")
            .unwrap();
        let child = &core.objs[index(&core, (2, 0))].state;
        assert_eq!(
            (child.state, child.state_type),
            (code::UNREACHABLE, StateType::Hard)
        );
        let events = sent(&mut out);
        let status = events
            .iter()
            .filter(|(kind, _)| *kind == event::HOST_STATUS)
            .map(|(_, p)| HostStatus::decode(p.as_slice()).unwrap())
            .find(|s| s.host_id == 2)
            .unwrap();
        assert_eq!(status.state, 2);
        let lines: Vec<String> = logs(&events).into_iter().map(|l| l.output).collect();
        assert!(
            lines.iter().any(|l| l == "no route"),
            "the HOST ALERT carries the output: {lines:?}"
        );
        // A passive UNREACHABLE without parent is DOWN.
        core.command("PROCESS_HOST_CHECK_RESULT;p;2;down").unwrap();
        assert_eq!(core.objs[index(&core, (1, 0))].state.state, code::DOWN);
        // A service of an unreachable host is HARD at once.
        core.command("PROCESS_SERVICE_CHECK_RESULT;c;s2;2;bad")
            .unwrap();
        let s2 = &core.objs[index(&core, (2, 2))].state;
        assert_eq!((s2.state_type, s2.attempt), (StateType::Hard, 1));

        // The parent recovers: its children not UP are checked again, once
        // their last result is older than cached_host_check_horizon.
        tokio::time::advance(Duration::from_secs(20)).await;
        core.schedule(index(&core, (2, 0)), later);
        core.command("PROCESS_HOST_CHECK_RESULT;p;0;up").unwrap();
        assert!(due_now(&core, (2, 0)));
    }

    #[tokio::test(start_paused = true)]
    async fn a_failing_master_suppresses_the_checks_of_its_dependent() {
        let dir = tempfile::tempdir().unwrap();
        let main = write(dir.path(), "c", "");
        let (mut core, _out, _target) = core(dir.path(), main);
        core.load().unwrap();
        let s2 = index(&core, (2, 2));
        assert!(core.viable(s2));
        core.command("PROCESS_SERVICE_CHECK_RESULT;c;s1;2;master down")
            .unwrap();
        let before = core.objs[s2].state.last_check;
        assert!(!core.viable(s2));
        assert_eq!(core.objs[s2].state.last_check, before);
        // Planned one check interval later (5 minutes).
        let due = core.objs[s2].due.unwrap();
        let delay = due.saturating_duration_since(tokio::time::Instant::now());
        assert_eq!(delay.as_secs(), 300);
        // A forced check ignores it, as CHECK_OPTION_FORCE_EXECUTION.
        core.command("PROCESS_SERVICE_CHECK_RESULT;c;s1;0;master ok")
            .unwrap();
        assert!(core.viable(s2));
    }

    #[tokio::test(start_paused = true)]
    async fn the_event_log_follows_engine() {
        let dir = tempfile::tempdir().unwrap();
        let main = write(dir.path(), "c", "");
        let (mut core, mut out, _target) = core(dir.path(), main);
        core.load().unwrap();
        // Before any connection the initial states wait.
        assert_eq!(core.early_logs.len(), 5);
        core.ever_connected = true;
        core.emit_snapshot();
        let events = sent(&mut out);
        let initial: Vec<String> = logs(&events)
            .into_iter()
            .map(|l| {
                assert_eq!(l.instance_name, "p");
                l.output
            })
            .collect();
        assert_eq!(initial.len(), 5, "{initial:?}");
        let le = logs(&events).remove(0);
        assert_eq!(le.msg_type, 9, "HOST_INITIAL_STATE");
        assert!(core.early_logs.is_empty());

        core.command("[1] PROCESS_SERVICE_CHECK_RESULT;c;s2;2;CRITICAL; bad")
            .unwrap();
        let entries = logs(&sent(&mut out));
        let lines: Vec<(i32, String)> = entries
            .iter()
            .map(|l| (l.msg_type, l.output.clone()))
            .collect();
        assert_eq!(
            lines,
            [
                (
                    5,
                    "EXTERNAL COMMAND: PROCESS_SERVICE_CHECK_RESULT;c;s2;2;CRITICAL; bad".into()
                ),
                (5, "PASSIVE SERVICE CHECK: c;s2;2;CRITICAL: bad".into()),
                (0, "CRITICAL: bad".into()),
            ]
        );
        let alert = &entries[2];
        assert_eq!((alert.host_id, alert.service_id), (2, 2));
        assert_eq!((alert.status, alert.retry, alert.r#type), (2, 1, 0));

        // Downtime alerts.
        core.command("SCHEDULE_SVC_DOWNTIME;c;s2;0;4000000000;1;0;0;admin;dt")
            .unwrap();
        core.run_timers();
        let lines: Vec<String> = logs(&sent(&mut out))
            .into_iter()
            .map(|l| l.output)
            .collect();
        assert!(
            lines.contains(
                &"SERVICE DOWNTIME ALERT: c;s2;STARTED; Service has entered a period of \
                  scheduled downtime"
                    .to_owned()
            ),
            "{lines:?}"
        );
    }

    #[tokio::test(start_paused = true)]
    async fn groups_tags_severities_and_parents_at_dump_and_on_reload() {
        let dir = tempfile::tempdir().unwrap();
        let main = write(dir.path(), "c", "");
        let (mut core, mut out, _target) = core(dir.path(), main.clone());
        core.load().unwrap();
        core.ever_connected = true;
        core.connected = true;
        core.emit_snapshot();
        let events = sent(&mut out);
        let kinds: Vec<u32> = events.iter().map(|(kind, _)| *kind).collect();
        let at = |kind| kinds.iter().position(|k| *k == kind).unwrap();
        assert!(at(event::SEVERITY) < at(event::HOST) && at(event::TAG) < at(event::HOST));
        assert!(at(event::HOST_PARENT) > at(event::SERVICE));
        assert!(at(event::HOST_GROUP) < at(event::HOST_GROUP_MEMBER));
        assert!(at(event::SERVICE_GROUP_MEMBER) < at(event::INSTANCE_CONFIGURATION));
        let parents: Vec<(u64, u64)> = events
            .iter()
            .filter(|(kind, _)| *kind == event::HOST_PARENT)
            .map(|(_, p)| HostParent::decode(p.as_slice()).unwrap())
            .map(|p| (p.child_id, p.parent_id))
            .collect();
        assert_eq!(parents, [(2, 1), (3, 1)]);
        let child = events
            .iter()
            .filter(|(kind, _)| *kind == event::HOST)
            .map(|(_, p)| gorgone_proto::bbdo::Host::decode(p.as_slice()).unwrap())
            .find(|h| h.host_id == 2)
            .unwrap();
        assert_eq!(child.severity_id, 5);
        assert_eq!((child.tags[0].id, child.tags[0].r#type), (8, 3));
        let members: Vec<(u64, u64)> = events
            .iter()
            .filter(|(kind, _)| *kind == event::SERVICE_GROUP_MEMBER)
            .map(|(_, p)| ServiceGroupMember::decode(p.as_slice()).unwrap())
            .map(|m| (m.host_id, m.service_id))
            .collect();
        assert_eq!(members, [(2, 1), (2, 2)]);

        // Reload: c2 joins the group, the tag is renamed, a severity goes.
        write(dir.path(), "c,c2", "")
            .exists()
            .then_some(())
            .unwrap();
        let text = std::fs::read_to_string(dir.path().join("objects.cfg"))
            .unwrap()
            .replace("tag_name cat", "tag_name category")
            .replace(" severity 5\n", "\n")
            .replace(
                "define severity {\n id 5\n severity_name sev\n level 2\n type host\n}\n",
                "",
            );
        std::fs::write(dir.path().join("objects.cfg"), text).unwrap();
        core.load().unwrap();
        let events = sent(&mut out);
        let kinds: Vec<u32> = events.iter().map(|(kind, _)| *kind).collect();
        let severity =
            Severity::decode(events[at_kind(&kinds, event::SEVERITY)].1.as_slice()).unwrap();
        assert_eq!(severity.action, 1, "DELETE");
        let tag = Tag::decode(events[at_kind(&kinds, event::TAG)].1.as_slice()).unwrap();
        assert_eq!((tag.action, tag.name.as_str()), (2, "category"));
        let group =
            HostGroup::decode(events[at_kind(&kinds, event::HOST_GROUP)].1.as_slice()).unwrap();
        assert!(group.enabled);
        let members: Vec<(u64, bool)> = events
            .iter()
            .filter(|(kind, _)| *kind == event::HOST_GROUP_MEMBER)
            .map(|(_, p)| HostGroupMember::decode(p.as_slice()).unwrap())
            .map(|m| (m.host_id, m.enabled))
            .collect();
        // The old members removed, then every member added again.
        assert_eq!(members, [(2, false), (2, true), (3, true)]);
        assert!(at_kind(&kinds, event::HOST_GROUP) > at_kind(&kinds, event::HOST));
        assert_eq!(*kinds.last().unwrap(), event::INSTANCE_CONFIGURATION);
        assert!(!kinds.contains(&event::SERVICE_GROUP));
        // No service changed: none is sent.
        assert!(
            events
                .iter()
                .filter(|(kind, _)| *kind == event::SERVICE)
                .all(|(_, p)| Service::decode(p.as_slice()).unwrap().enabled)
        );
    }

    fn at_kind(kinds: &[u32], kind: u32) -> usize {
        kinds.iter().position(|k| *k == kind).unwrap()
    }

    #[tokio::test(start_paused = true)]
    async fn group_downtimes_and_acknowledgement_expiry() {
        let dir = tempfile::tempdir().unwrap();
        let main = write(dir.path(), "c,c2", "");
        let (mut core, _out, _target) = core(dir.path(), main);
        core.load().unwrap();
        let answer = core
            .command("SCHEDULE_HOSTGROUP_SVC_DOWNTIME;hg;0;4000000000;1;0;0;admin;dt")
            .unwrap();
        assert!(answer.contains("[1, 2]"), "{answer}");
        core.command("SCHEDULE_HOSTGROUP_HOST_DOWNTIME;hg;0;4000000000;1;0;0;admin;dt")
            .unwrap();
        core.command("SCHEDULE_SERVICEGROUP_HOST_DOWNTIME;sg;0;4000000000;1;0;0;admin;dt")
            .unwrap();
        let answer = core
            .command("SCHEDULE_SERVICEGROUP_SVC_DOWNTIME;sg;0;4000000000;1;0;0;admin;dt")
            .unwrap();
        assert!(answer.contains("[6, 7]"), "{answer}");
        let unknown = core
            .command("SCHEDULE_HOSTGROUP_HOST_DOWNTIME;nope;0;4000000000;1;0;0;a;b")
            .unwrap();
        assert!(unknown.contains("unknown host group"), "{unknown}");

        // acknowledgement_timeout 2 (minutes): the acknowledgement ends.
        core.command("PROCESS_SERVICE_CHECK_RESULT;c;s2;2;bad")
            .unwrap();
        core.command("ACKNOWLEDGE_SVC_PROBLEM;c;s2;2;0;0;admin;seen")
            .unwrap();
        let s2 = index(&core, (2, 2));
        assert_ne!(
            core.objs[s2].state.acknowledgement,
            crate::state::AckType::None
        );
        tokio::time::advance(Duration::from_secs(119)).await;
        core.run_timers();
        assert_ne!(
            core.objs[s2].state.acknowledgement,
            crate::state::AckType::None
        );
        tokio::time::advance(Duration::from_secs(1)).await;
        core.run_timers();
        assert_eq!(
            core.objs[s2].state.acknowledgement,
            crate::state::AckType::None
        );
    }
}
