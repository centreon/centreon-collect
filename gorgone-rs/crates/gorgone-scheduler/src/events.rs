//! BBDO events the scheduler sends, filled as `engine/src/broker.cc` fills
//! them for the fields the scheduler knows.

use gorgone_proto::bbdo::{
    Host, HostStatus, Instance, InstanceConfiguration, InstanceStatus, Service, ServiceStatus,
    ServiceType, host, host_status, service, service_status,
};

use crate::bbdo::{event, frame};
use crate::cfg::{HostConf, ServiceConf};
use crate::state::{CheckState, CheckType, StateType};

/// Name the instance reports in `instances.engine`.
pub const ENGINE_NAME: &str = "Centreon Engine (gorgone-scheduler)";

/// `pb_instance`: start (`running`) or end of the poller.
#[must_use]
pub fn instance(poller_id: u64, name: &str, running: bool, start: i64, end: i64) -> Vec<u8> {
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
            version: env!("CARGO_PKG_VERSION").to_owned(),
            ..Instance::default()
        },
    )
}

/// `pb_instance_status`: the poller is alive.
#[must_use]
pub fn instance_status(poller_id: u64, now: i64) -> Vec<u8> {
    frame(
        event::INSTANCE_STATUS,
        &InstanceStatus {
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

/// `pb_host`: configuration and current state of a host.
#[must_use]
pub fn host(poller_id: u64, conf: &HostConf, st: &CheckState, now: i64) -> Vec<u8> {
    let check = &conf.check;
    let message = Host {
        host_id: conf.host_id,
        active_checks: check.active_checks,
        enabled: true,
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
            output: st.output.clone(),
            long_output: st.long_output.clone(),
            perfdata: st.perfdata.clone(),
            latency: st.latency,
            execution_time: st.execution_time,
            last_check: st.last_check,
            next_check: st.next_check,
            should_be_scheduled: should_be_scheduled(st),
            check_attempt: i32::try_from(st.attempt).unwrap_or(i32::MAX),
            ..HostStatus::default()
        },
    )
}

/// `pb_service`: configuration and current state of a service.
#[must_use]
pub fn service(poller_id: u64, conf: &ServiceConf, st: &CheckState, now: i64) -> Vec<u8> {
    let check = &conf.check;
    let message = Service {
        host_id: conf.host_id,
        service_id: conf.service_id,
        active_checks: check.active_checks,
        enabled: true,
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
            ..ServiceStatus::default()
        },
    )
}
