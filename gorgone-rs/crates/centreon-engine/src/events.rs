//! BBDO events the scheduler sends, filled as `engine/src/broker.cc` fills
//! them for the fields the scheduler knows.

use gorgone_proto::bbdo::{
    AckType, Acknowledgement, AdaptiveHost, AdaptiveService, Comment, Downtime, Host, HostGroup,
    HostGroupMember, HostParent, HostStatus, Instance, InstanceConfiguration, InstanceStatus,
    Service, ServiceGroup, ServiceGroupMember, ServiceStatus, ServiceType, Severity, Tag, TagInfo,
    acknowledgement, comment, downtime, host, host_status, service, service_status, severity, tag,
};

use crate::bbdo::{event, frame};
use crate::book;
use crate::cfg::{HostConf, NotifyConf, ServiceConf};
use crate::objects::{self, HostGroupConf, ServiceGroupConf, SeverityConf, TagConf, notify_on};
use crate::state::{self, CheckState, CheckType, StateType};

/// Name the instance reports in `instances.engine`.
pub const ENGINE_NAME: &str = "Centreon Engine (Rust)";

/// Version the instance reports in `instances.version`: the Centreon
/// Engine release whose configuration and BBDO contract this engine
/// follows (`COLLECT_MAJOR.COLLECT_MINOR.COLLECT_PATCH` of the root
/// `CMakeLists.txt`). centreon-web compares it with Engine versions
/// (`www/class/config-generate/engine.class.php`, `broker.class.php`): the
/// crate version (0.x) would read as a very old Engine.
pub const INSTANCE_VERSION: &str = "27.01.0";

/// `pb_instance`: start (`running`) or end of the poller; `encryption`
/// when it has the keys to decrypt `encrypt::` values
/// (`broker_program_state`, `engine/src/broker.cc:4176`).
#[must_use]
pub fn instance(
    poller_id: u64,
    name: &str,
    running: bool,
    (start, end): (i64, i64),
    encryption: bool,
) -> Vec<u8> {
    frame(
        event::INSTANCE,
        &Instance {
            engine: ENGINE_NAME.to_owned(),
            running,
            name: name.to_owned(),
            pid: i64::from(std::process::id()),
            instance_id: poller_id,
            end_time: end,
            start_time: start,
            version: INSTANCE_VERSION.to_owned(),
            is_encryption_ready: encryption,
            ..Instance::default()
        },
    )
}

fn ack_type(st: &CheckState) -> AckType {
    match st.acknowledgement {
        state::AckType::None => AckType::None,
        state::AckType::Normal => AckType::Normal,
        state::AckType::Sticky => AckType::Sticky,
    }
}

/// `-1` of `cbmod` for a time not set yet, as the unsigned field takes it.
fn time_or_unset(time: Option<i64>) -> u64 {
    time.map_or(u64::MAX, |t| u64::try_from(t).unwrap_or(0))
}

fn epoch(time: i64) -> u64 {
    u64::try_from(time).unwrap_or(0)
}

/// `pb_downtime` as `cbmod` fills it (`broker/neb/src/cbmod.cc:251-372`):
/// `deletion` is the time of a `remove_downtime`.
#[must_use]
pub fn downtime(poller_id: u64, dt: &book::Downtime, deletion: Option<i64>) -> Vec<u8> {
    frame(
        event::DOWNTIME,
        &Downtime {
            id: dt.id,
            instance_id: poller_id,
            host_id: dt.host_id,
            service_id: dt.service_id,
            author: dt.author.clone(),
            comment_data: dt.comment.clone(),
            r#type: if dt.service_id == 0 {
                downtime::DowntimeType::Host
            } else {
                downtime::DowntimeType::Service
            }
            .into(),
            duration: dt.duration,
            triggered_by: dt.triggered_by,
            entry_time: dt.entry_time,
            actual_start_time: time_or_unset(dt.actual_start),
            actual_end_time: time_or_unset(dt.actual_end),
            start_time: epoch(dt.start_time),
            deletion_time: time_or_unset(deletion),
            end_time: epoch(dt.end_time),
            started: dt.started,
            cancelled: dt.cancelled,
            fixed: dt.fixed,
        },
    )
}

/// `pb_comment` (`forward_pb_comment`, `engine/src/broker.cc:1286-1360`):
/// `deletion` on `NEBTYPE_COMMENT_DELETE`.
#[must_use]
pub fn comment(poller_id: u64, c: &book::Comment, deletion: Option<i64>) -> Vec<u8> {
    frame(
        event::COMMENT,
        &Comment {
            author: c.author.clone(),
            r#type: if c.service_id == 0 {
                comment::Type::Host
            } else {
                comment::Type::Service
            }
            .into(),
            data: c.data.clone(),
            deletion_time: deletion.map_or(0, epoch),
            entry_time: epoch(c.entry_time),
            entry_type: match c.entry_type {
                book::EntryType::User => comment::EntryType::User,
                book::EntryType::Downtime => comment::EntryType::Downtime,
                book::EntryType::Acknowledgement => comment::EntryType::Acknowledgment,
                book::EntryType::Flapping => comment::EntryType::Flapping,
            }
            .into(),
            expire_time: 0,
            expires: false,
            host_id: c.host_id,
            internal_id: c.id,
            persistent: c.persistent,
            instance_id: poller_id,
            service_id: c.service_id,
            source: match c.source {
                book::Source::Internal => comment::Src::Internal,
                book::Source::External => comment::Src::External,
            }
            .into(),
            ..Comment::default()
        },
    )
}

/// `pb_acknowledgement` (`forward_pb_acknowledgement`,
/// `engine/src/broker.cc:140-190`); `deletion` when it ends.
#[must_use]
pub fn acknowledgement(
    poller_id: u64,
    ack: &book::Acknowledgement,
    deletion: Option<i64>,
) -> Vec<u8> {
    frame(
        event::ACKNOWLEDGEMENT,
        &Acknowledgement {
            host_id: ack.host_id,
            service_id: ack.service_id,
            instance_id: poller_id,
            r#type: if ack.service_id == 0 {
                acknowledgement::ResourceType::Host
            } else {
                acknowledgement::ResourceType::Service
            }
            .into(),
            author: ack.author.clone(),
            comment_data: ack.comment.clone(),
            sticky: ack.sticky,
            notify_contacts: ack.notify,
            entry_time: epoch(ack.entry_time),
            deletion_time: deletion.map_or(0, epoch),
            persistent_comment: ack.persistent,
            state: ack.state,
        },
    )
}

/// The global switches `pb_instance_status` reports
/// (`broker_program_status`, `engine/src/broker.cc:4333-4347`).
#[derive(Debug, Clone, Default)]
pub struct Program {
    pub notifications: bool,
    pub event_handlers: bool,
    pub flap_detection: bool,
    pub check_hosts_freshness: bool,
    pub check_services_freshness: bool,
    pub global_host_event_handler: String,
    pub global_service_event_handler: String,
}

/// `pb_instance_status`: the poller is alive.
#[must_use]
pub fn instance_status(poller_id: u64, now: i64, program: &Program) -> Vec<u8> {
    frame(
        event::INSTANCE_STATUS,
        &InstanceStatus {
            notifications: program.notifications,
            event_handlers: program.event_handlers,
            flap_detection: program.flap_detection,
            check_hosts_freshness: program.check_hosts_freshness,
            check_services_freshness: program.check_services_freshness,
            global_host_event_handler: program.global_host_event_handler.clone(),
            global_service_event_handler: program.global_service_event_handler.clone(),
            active_host_checks: true,
            active_service_checks: true,
            passive_host_checks: true,
            passive_service_checks: true,
            last_alive: u64::try_from(now).unwrap_or(0),
            last_command_check: now,
            instance_id: poller_id,
            ..InstanceStatus::default()
        },
    )
}

/// `pb_instance_configuration`: the configuration has been sent.
#[must_use]
pub fn instance_configuration(poller_id: u64) -> Vec<u8> {
    frame(
        event::INSTANCE_CONFIGURATION,
        &InstanceConfiguration {
            loaded: true,
            poller_id,
            ..InstanceConfiguration::default()
        },
    )
}

fn hard(state_type: StateType) -> bool {
    state_type == StateType::Hard
}

fn active(check_type: CheckType) -> bool {
    check_type == CheckType::Active
}

fn should_be_scheduled(st: &CheckState) -> bool {
    st.next_check != 0
}

fn on(notify: &NotifyConf, flag: u32) -> bool {
    notify.notification_options & flag != 0
}

fn notifications_enabled(notify: &NotifyConf, st: &CheckState) -> bool {
    st.modified
        .notifications_enabled
        .unwrap_or(notify.notifications_enabled)
}

fn event_handler_enabled(notify: &NotifyConf, st: &CheckState) -> bool {
    st.modified
        .event_handler_enabled
        .unwrap_or(notify.event_handler_enabled)
}

fn flap_detection_enabled(notify: &NotifyConf, st: &CheckState) -> bool {
    st.modified
        .flap_detection_enabled
        .unwrap_or(notify.flap_detection_enabled)
}

fn number(value: u32) -> i32 {
    i32::try_from(value).unwrap_or(i32::MAX)
}

/// What an external command changed on an object
/// (`broker_adaptive_*_data`, `engine/src/broker.cc:489-520`, `:944-1000`).
#[derive(Debug, Clone, Copy, Default)]
pub struct Adaptive {
    pub notify: Option<bool>,
    pub event_handler_enabled: Option<bool>,
    pub flap_detection: Option<bool>,
}

/// `pb_adaptive_host`.
#[must_use]
pub fn adaptive_host(host_id: u64, change: Adaptive) -> Vec<u8> {
    frame(
        event::ADAPTIVE_HOST,
        &AdaptiveHost {
            host_id,
            notify: change.notify,
            event_handler_enabled: change.event_handler_enabled,
            flap_detection: change.flap_detection,
            ..AdaptiveHost::default()
        },
    )
}

/// `pb_adaptive_service`.
#[must_use]
pub fn adaptive_service((host_id, service_id): (u64, u64), change: Adaptive) -> Vec<u8> {
    frame(
        event::ADAPTIVE_SERVICE,
        &AdaptiveService {
            host_id,
            service_id,
            notify: change.notify,
            event_handler_enabled: change.event_handler_enabled,
            flap_detection_enabled: change.flap_detection,
            ..AdaptiveService::default()
        },
    )
}

fn tag_type(kind: objects::TagType) -> gorgone_proto::bbdo::TagType {
    match kind {
        objects::TagType::ServiceGroup => gorgone_proto::bbdo::TagType::Servicegroup,
        objects::TagType::HostGroup => gorgone_proto::bbdo::TagType::Hostgroup,
        objects::TagType::ServiceCategory => gorgone_proto::bbdo::TagType::Servicecategory,
        objects::TagType::HostCategory => gorgone_proto::bbdo::TagType::Hostcategory,
    }
}

/// `tags` of `pb_host`/`pb_service` (`forward_pb_host`, `broker.cc:660-664`).
fn tag_infos(tags: &[(u64, objects::TagType)]) -> Vec<TagInfo> {
    tags.iter()
        .map(|(id, kind)| TagInfo {
            id: *id,
            r#type: tag_type(*kind).into(),
        })
        .collect()
}

/// What a severity or tag event tells (`NEBTYPE_*_ADD`, `_DELETE`,
/// `_UPDATE`).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Action {
    Add,
    Delete,
    Modify,
}

/// `pb_severity` (`broker_adaptive_severity_data`, `broker.cc:233-276`).
#[must_use]
pub fn severity(poller_id: u64, conf: &SeverityConf, action: Action) -> Vec<u8> {
    frame(
        event::SEVERITY,
        &Severity {
            id: conf.id,
            action: match action {
                Action::Add => severity::Action::Add,
                Action::Delete => severity::Action::Delete,
                Action::Modify => severity::Action::Modify,
            }
            .into(),
            level: conf.level,
            icon_id: conf.icon_id,
            name: conf.name.clone(),
            r#type: match conf.kind {
                objects::SeverityType::Service => severity::Type::Service,
                objects::SeverityType::Host => severity::Type::Host,
            }
            .into(),
            poller_id,
        },
    )
}

/// `pb_tag` (`broker_adaptive_tag_data`, `broker.cc:280-336`).
#[must_use]
pub fn tag(poller_id: u64, conf: &TagConf, action: Action) -> Vec<u8> {
    frame(
        event::TAG,
        &Tag {
            id: conf.id,
            action: match action {
                Action::Add => tag::Action::Add,
                Action::Delete => tag::Action::Delete,
                Action::Modify => tag::Action::Modify,
            }
            .into(),
            r#type: tag_type(conf.kind).into(),
            name: conf.name.clone(),
            poller_id,
        },
    )
}

/// `pb_host_group` (`forward_pb_group`, `broker.cc:2131-2180`): enabled
/// when added, or updated with members.
#[must_use]
pub fn host_group(poller_id: u64, conf: &HostGroupConf, enabled: bool) -> Vec<u8> {
    frame(
        event::HOST_GROUP,
        &HostGroup {
            enabled,
            hostgroup_id: conf.id,
            name: conf.name.clone(),
            poller_id,
            alias: conf.alias.clone(),
            ..HostGroup::default()
        },
    )
}

/// `pb_host_group_member` (`forward_pb_group_member`, `broker.cc:2341-2385`).
#[must_use]
pub fn host_group_member(
    poller_id: u64,
    conf: &HostGroupConf,
    host_id: u64,
    enabled: bool,
) -> Vec<u8> {
    frame(
        event::HOST_GROUP_MEMBER,
        &HostGroupMember {
            enabled,
            hostgroup_id: conf.id,
            name: conf.name.clone(),
            host_id,
            poller_id,
            ..HostGroupMember::default()
        },
    )
}

/// `pb_service_group` (`forward_pb_group`, `broker.cc:2182-2215`).
#[must_use]
pub fn service_group(poller_id: u64, conf: &ServiceGroupConf, enabled: bool) -> Vec<u8> {
    frame(
        event::SERVICE_GROUP,
        &ServiceGroup {
            enabled,
            servicegroup_id: conf.id,
            name: conf.name.clone(),
            poller_id,
            alias: conf.alias.clone(),
            ..ServiceGroup::default()
        },
    )
}

/// `pb_service_group_member` (`forward_pb_group_member`, `broker.cc:2386-2425`).
#[must_use]
pub fn service_group_member(
    poller_id: u64,
    conf: &ServiceGroupConf,
    (host_id, service_id): (u64, u64),
    enabled: bool,
) -> Vec<u8> {
    frame(
        event::SERVICE_GROUP_MEMBER,
        &ServiceGroupMember {
            enabled,
            servicegroup_id: conf.id,
            name: conf.name.clone(),
            host_id,
            poller_id,
            service_id,
            ..ServiceGroupMember::default()
        },
    )
}

/// `pb_host_parent` (`forward_pb_relation`, `broker.cc:3973-4005`).
#[must_use]
pub fn host_parent(child_id: u64, parent_id: u64, enabled: bool) -> Vec<u8> {
    frame(
        event::HOST_PARENT,
        &HostParent {
            enabled,
            child_id,
            parent_id,
            ..HostParent::default()
        },
    )
}

/// `pb_host`: configuration and current state of a host; `enabled` false
/// when it is removed.
#[must_use]
pub fn host(poller_id: u64, conf: &HostConf, st: &CheckState, now: i64, enabled: bool) -> Vec<u8> {
    let check = &conf.check;
    let message = Host {
        host_id: conf.host_id,
        acknowledged: st.acknowledgement != state::AckType::None,
        acknowledgement_type: ack_type(st).into(),
        scheduled_downtime_depth: st.downtime_depth,
        check_period: check.check_period.clone(),
        timezone: conf.timezone.clone(),
        active_checks: check.active_checks,
        enabled,
        check_command: check.check_command.clone(),
        check_interval: i32::try_from(check.check_interval).unwrap_or(i32::MAX),
        check_type: if active(st.check_type) {
            host::CheckType::Active
        } else {
            host::CheckType::Passive
        }
        .into(),
        check_attempt: i32::try_from(st.attempt).unwrap_or(i32::MAX),
        state: i32::try_from(st.state).unwrap_or(0),
        execution_time: st.execution_time,
        checked: st.checked,
        last_check: st.last_check,
        last_hard_state: i32::try_from(st.last_hard_state).unwrap_or(0),
        last_hard_state_change: st.last_hard_state_change,
        last_state_change: st.last_state_change,
        last_time_up: st.last_time[0],
        last_time_down: st.last_time[1],
        last_time_unreachable: st.last_time[2],
        last_update: now,
        latency: st.latency,
        max_check_attempts: i32::try_from(check.max_check_attempts).unwrap_or(i32::MAX),
        next_check: st.next_check,
        output: st.output.clone(),
        passive_checks: check.passive_checks,
        perfdata: st.perfdata.clone(),
        retry_interval: f64::from(check.retry_interval),
        should_be_scheduled: should_be_scheduled(st),
        state_type: if hard(st.state_type) {
            host::StateType::Hard
        } else {
            host::StateType::Soft
        }
        .into(),
        address: conf.address.clone(),
        alias: conf.alias.clone(),
        default_active_checks: check.active_checks,
        default_passive_checks: check.passive_checks,
        display_name: conf.name.clone(),
        name: conf.name.clone(),
        instance_id: poller_id,
        severity_id: conf.severity_id,
        tags: tag_infos(&conf.tags),
        icon_id: conf.icon_id,
        // `forward_pb_host`, `broker.cc:569-640`.
        last_notification: st.notification.last,
        notification_number: number(st.notification.number),
        next_host_notification: st.notification.next,
        no_more_notifications: st.notification.no_more,
        notify: notifications_enabled(&conf.notify, st),
        default_notify: conf.notify.notifications_enabled,
        first_notification_delay: f64::from(conf.notify.first_notification_delay),
        notification_interval: f64::from(conf.notify.notification_interval),
        notification_period: conf.notify.notification_period.clone(),
        notify_on_down: on(&conf.notify, notify_on::DOWN),
        notify_on_downtime: on(&conf.notify, notify_on::DOWNTIME),
        notify_on_flapping: on(&conf.notify, notify_on::FLAPPING_START),
        notify_on_recovery: on(&conf.notify, notify_on::UP),
        notify_on_unreachable: on(&conf.notify, notify_on::UNREACHABLE),
        event_handler: conf.notify.event_handler.clone(),
        event_handler_enabled: event_handler_enabled(&conf.notify, st),
        default_event_handler_enabled: conf.notify.event_handler_enabled,
        flap_detection: flap_detection_enabled(&conf.notify, st),
        default_flap_detection: conf.notify.flap_detection_enabled,
        flapping: st.flap.is_flapping,
        percent_state_change: st.flap.percent_state_change,
        check_freshness: conf.notify.check_freshness,
        freshness_threshold: f64::from(conf.notify.freshness_threshold),
        low_flap_threshold: conf.notify.low_flap_threshold,
        high_flap_threshold: conf.notify.high_flap_threshold,
        ..Host::default()
    };
    frame(event::HOST, &message)
}

/// `pb_host_status`: current state of a host.
#[must_use]
pub fn host_status(conf: &HostConf, st: &CheckState) -> Vec<u8> {
    frame(
        event::HOST_STATUS,
        &HostStatus {
            host_id: conf.host_id,
            checked: st.checked,
            check_type: if active(st.check_type) {
                host_status::CheckType::Active
            } else {
                host_status::CheckType::Passive
            }
            .into(),
            state: i32::try_from(st.state).unwrap_or(0),
            state_type: if hard(st.state_type) {
                host_status::StateType::Hard
            } else {
                host_status::StateType::Soft
            }
            .into(),
            last_state_change: st.last_state_change,
            last_hard_state: i32::try_from(st.last_hard_state).unwrap_or(0),
            last_hard_state_change: st.last_hard_state_change,
            last_time_up: st.last_time[0],
            last_time_down: st.last_time[1],
            last_time_unreachable: st.last_time[2],
            output: st.output.clone(),
            long_output: st.long_output.clone(),
            perfdata: st.perfdata.clone(),
            latency: st.latency,
            execution_time: st.execution_time,
            last_check: st.last_check,
            next_check: st.next_check,
            should_be_scheduled: should_be_scheduled(st),
            check_attempt: i32::try_from(st.attempt).unwrap_or(i32::MAX),
            acknowledgement_type: ack_type(st).into(),
            scheduled_downtime_depth: st.downtime_depth,
            flapping: st.flap.is_flapping,
            percent_state_change: st.flap.percent_state_change,
            notification_number: number(st.notification.number),
            no_more_notifications: st.notification.no_more,
            last_notification: st.notification.last,
            next_host_notification: st.notification.next,
        },
    )
}

/// `pb_service`: configuration and current state of a service; `enabled`
/// false when it is removed.
#[must_use]
pub fn service(
    poller_id: u64,
    conf: &ServiceConf,
    st: &CheckState,
    now: i64,
    enabled: bool,
) -> Vec<u8> {
    let check = &conf.check;
    let message = Service {
        host_id: conf.host_id,
        service_id: conf.service_id,
        acknowledged: st.acknowledgement != state::AckType::None,
        acknowledgement_type: ack_type(st).into(),
        scheduled_downtime_depth: st.downtime_depth,
        check_period: check.check_period.clone(),
        active_checks: check.active_checks,
        enabled,
        check_command: check.check_command.clone(),
        check_interval: check.check_interval,
        check_type: if active(st.check_type) {
            service::CheckType::Active
        } else {
            service::CheckType::Passive
        }
        .into(),
        check_attempt: i32::try_from(st.attempt).unwrap_or(i32::MAX),
        state: i32::try_from(st.state).unwrap_or(0),
        execution_time: st.execution_time,
        checked: st.checked,
        last_check: st.last_check,
        last_hard_state: i32::try_from(st.last_hard_state).unwrap_or(0),
        last_hard_state_change: st.last_hard_state_change,
        last_state_change: st.last_state_change,
        last_time_ok: st.last_time[0],
        last_time_warning: st.last_time[1],
        last_time_critical: st.last_time[2],
        last_time_unknown: st.last_time[3],
        last_update: now,
        latency: st.latency,
        max_check_attempts: check.max_check_attempts,
        next_check: st.next_check,
        output: st.output.clone(),
        long_output: st.long_output.clone(),
        passive_checks: check.passive_checks,
        perfdata: st.perfdata.clone(),
        retry_interval: f64::from(check.retry_interval),
        host_name: conf.host_name.clone(),
        description: conf.description.clone(),
        should_be_scheduled: should_be_scheduled(st),
        state_type: if hard(st.state_type) {
            service::StateType::Hard
        } else {
            service::StateType::Soft
        }
        .into(),
        default_active_checks: check.active_checks,
        default_passive_checks: check.passive_checks,
        display_name: conf.description.clone(),
        r#type: ServiceType::Service.into(),
        instance_id: poller_id,
        severity_id: conf.severity_id,
        tags: tag_infos(&conf.tags),
        icon_id: conf.icon_id,
        // `forward_pb_service`, `broker.cc:1054-1125`.
        last_notification: st.notification.last,
        notification_number: number(st.notification.number),
        next_notification: st.notification.next,
        no_more_notifications: st.notification.no_more,
        notify: notifications_enabled(&conf.notify, st),
        default_notify: conf.notify.notifications_enabled,
        first_notification_delay: f64::from(conf.notify.first_notification_delay),
        notification_interval: f64::from(conf.notify.notification_interval),
        notification_period: conf.notify.notification_period.clone(),
        notify_on_critical: on(&conf.notify, notify_on::CRITICAL),
        notify_on_downtime: on(&conf.notify, notify_on::DOWNTIME),
        notify_on_flapping: on(&conf.notify, notify_on::FLAPPING_START),
        notify_on_recovery: on(&conf.notify, notify_on::OK),
        notify_on_unknown: on(&conf.notify, notify_on::UNKNOWN),
        notify_on_warning: on(&conf.notify, notify_on::WARNING),
        event_handler: conf.notify.event_handler.clone(),
        event_handler_enabled: event_handler_enabled(&conf.notify, st),
        default_event_handler_enabled: conf.notify.event_handler_enabled,
        flap_detection: flap_detection_enabled(&conf.notify, st),
        default_flap_detection: conf.notify.flap_detection_enabled,
        flapping: st.flap.is_flapping,
        percent_state_change: st.flap.percent_state_change,
        check_freshness: conf.notify.check_freshness,
        freshness_threshold: f64::from(conf.notify.freshness_threshold),
        low_flap_threshold: conf.notify.low_flap_threshold,
        high_flap_threshold: conf.notify.high_flap_threshold,
        is_volatile: conf.notify.is_volatile,
        ..Service::default()
    };
    frame(event::SERVICE, &message)
}

/// `pb_service_status`: current state of a service.
#[must_use]
pub fn service_status(conf: &ServiceConf, st: &CheckState) -> Vec<u8> {
    frame(
        event::SERVICE_STATUS,
        &ServiceStatus {
            host_id: conf.host_id,
            service_id: conf.service_id,
            checked: st.checked,
            check_type: if active(st.check_type) {
                service_status::CheckType::Active
            } else {
                service_status::CheckType::Passive
            }
            .into(),
            state: i32::try_from(st.state).unwrap_or(0),
            state_type: if hard(st.state_type) {
                service_status::StateType::Hard
            } else {
                service_status::StateType::Soft
            }
            .into(),
            last_state_change: st.last_state_change,
            last_hard_state: i32::try_from(st.last_hard_state).unwrap_or(0),
            last_hard_state_change: st.last_hard_state_change,
            last_time_ok: st.last_time[0],
            last_time_warning: st.last_time[1],
            last_time_critical: st.last_time[2],
            last_time_unknown: st.last_time[3],
            output: st.output.clone(),
            long_output: st.long_output.clone(),
            perfdata: st.perfdata.clone(),
            latency: st.latency,
            execution_time: st.execution_time,
            last_check: st.last_check,
            next_check: st.next_check,
            should_be_scheduled: should_be_scheduled(st),
            check_attempt: i32::try_from(st.attempt).unwrap_or(i32::MAX),
            r#type: ServiceType::Service.into(),
            acknowledgement_type: ack_type(st).into(),
            scheduled_downtime_depth: st.downtime_depth,
            flapping: st.flap.is_flapping,
            percent_state_change: st.flap.percent_state_change,
            notification_number: number(st.notification.number),
            no_more_notifications: st.notification.no_more,
            last_notification: st.notification.last,
            next_notification: st.notification.next,
            ..ServiceStatus::default()
        },
    )
}
