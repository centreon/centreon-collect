//! Event handlers (`engine/src/sehandlers.cc`, called from
//! `service::handle_async_check_result` and `host::handle_state`) and the
//! freshness of passive results (`service::check_result_freshness`,
//! `is_result_fresh`, `engine/src/service.cc:3416-3810`, the same for
//! hosts `host.cc:2870-3819`).

use std::collections::HashMap;
use std::time::Duration;

use crate::cfg::{self, LogLevel};
use crate::macros::{Context, HostView, ServiceView};
use crate::state::{StateType, code};
use crate::{cmdline, runner};

use super::{Core, ObjConf, Timer};

/// `get_time_breakdown`: days, hours, minutes, seconds.
fn breakdown(seconds: i64) -> String {
    let seconds = seconds.max(0);
    format!(
        "{}d {}h {}m {}s",
        seconds / 86_400,
        seconds % 86_400 / 3600,
        seconds % 3600 / 60,
        seconds % 60
    )
}

impl Core {
    /// `enable_event_handlers`, as `ENABLE_EVENT_HANDLERS` left it.
    pub(super) fn event_handlers_on(&self) -> bool {
        self.program
            .event_handlers
            .unwrap_or(self.settings().enable_event_handlers)
    }

    /// `event_handler_enabled` of object `index`, as the commands left it.
    pub(super) fn event_handler_enabled(&self, index: usize) -> bool {
        self.objs[index]
            .state
            .modified
            .event_handler_enabled
            .unwrap_or(self.notify_conf(index).event_handler_enabled)
    }

    /// `handle_service_event` / `handle_host_event`: the global handler,
    /// then the object's, each with the macros of the object, stripped
    /// as Engine does, no shell, killed at `event_handler_timeout`. The
    /// object's switch also stops the global handler.
    pub(super) fn handle_event(&mut self, index: usize) {
        if !self.event_handlers_on() || !self.event_handler_enabled(index) {
            return;
        }
        let settings = self.settings().clone();
        let host = self.objs[index].is_host();
        let global = if host {
            settings.global_host_event_handler.clone()
        } else {
            settings.global_service_event_handler.clone()
        };
        let own = self.notify_conf(index).event_handler.clone();
        if !global.is_empty() {
            self.run_handler(index, &global, true, &settings);
        }
        if !own.is_empty() {
            self.run_handler(index, &own, false, &settings);
        }
    }

    fn run_handler(&mut self, index: usize, handler: &str, global: bool, settings: &cfg::Settings) {
        let mut extra = HashMap::new();
        self.object_macros(index, &mut extra);
        let Some(loaded) = self.loaded.as_ref() else {
            return;
        };
        let name = cfg::command_name(handler);
        let Some(line) = loaded.config.commands.get(name) else {
            return;
        };
        let obj = &self.objs[index];
        let (host_obj, service) = match &obj.conf {
            ObjConf::Host(_) => (obj, None),
            ObjConf::Service { conf, host } => (&self.objs[*host], Some((conf, obj))),
        };
        let ObjConf::Host(host) = &host_obj.conf else {
            return;
        };
        let secrets = if loaded.config.secret_prefixes {
            self.credentials.as_ref()
        } else {
            None
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
        .with_extra(&extra)
        .stripping(&settings.illegal_macro_output_chars);
        let processed = ctx.check_command_line(handler, line);
        let st = &obj.state;
        let view = Self::state_view(st, service.is_none());
        // `sehandlers.cc:221-227`, `:330-336`, `:483-488`, `:586-591`; the
        // global service line is a debug one, and the global host line has
        // no `;` after the host name, as in Engine.
        let line = match (service, global) {
            (Some((conf, _)), false) => Some(format!(
                "SERVICE EVENT HANDLER: {};{};{};{};{};{handler}",
                host.name, conf.description, view.state, view.state_type, view.attempt
            )),
            (Some(_), true) => None,
            (None, false) => Some(format!(
                "HOST EVENT HANDLER: {};{};{};{};{handler}",
                host.name, view.state, view.state_type, view.attempt
            )),
            (None, true) => Some(format!(
                "GLOBAL HOST EVENT HANDLER: {}{};{};{};{handler}",
                host.name, view.state, view.state_type, view.attempt
            )),
        };
        let what = match (service.is_some(), global) {
            (true, false) => "Service event handler",
            (true, true) => "Global service event handler",
            (false, false) => "Host event handler",
            (false, true) => "Global host event handler",
        };
        let key = obj.key;
        let argv = cmdline::split(&processed);
        let events_info = settings.log_level_events.logs_info();
        if let Some(line) = line.filter(|_| settings.log_event_handlers && events_info) {
            self.log(&line);
        }
        let argv = match argv {
            Ok(argv) => argv,
            Err(error) => {
                tracing::warn!(host_id = key.0, service_id = key.1, command = name, %error, "event handler not run");
                return;
            }
        };
        let timeout = Duration::from_secs(u64::from(settings.event_handler_timeout.max(1)));
        let side = self.side_tx.clone();
        let max_output = self.options.max_output_bytes;
        let command = name.to_owned();
        tracing::debug!(
            host_id = key.0,
            service_id = key.1,
            command,
            "event handler"
        );
        tokio::spawn(async move {
            let run = runner::run(&argv, timeout, max_output).await;
            // The command name: the processed line may hold secrets.
            if run.timed_out && events_info {
                let _ = side.send(format!(
                    "Warning: {what} command '{command}' timed out after {} seconds",
                    timeout.as_secs()
                ));
            }
            if let Some(error) = run.spawn_error {
                tracing::warn!(%command, %error, "event handler not started");
            }
        });
    }

    /// `is_result_fresh`: the threshold (given, or one interval plus the
    /// latency and `additional_freshness_latency`), the expiration (from
    /// the start of the engine while never checked, with the check spread
    /// after a start for an active object), and the stale warning.
    pub(super) fn is_result_fresh(&mut self, index: usize, now: i64, log: bool) -> bool {
        let settings = self.settings().clone();
        let il = i64::from(settings.interval_length.max(1));
        let obj = &self.objs[index];
        let st = &obj.state;
        let check = obj.check();
        let notify = self.notify_conf(index);
        let threshold = if notify.freshness_threshold == 0 {
            let units = if st.state_type == StateType::Hard || st.state == code::OK {
                check.check_interval
            } else {
                check.retry_interval
            };
            (i64::from(units) * il) as f64
                + st.latency
                + f64::from(settings.additional_freshness_latency)
        } else {
            f64::from(notify.freshness_threshold)
        } as i64;
        let spread = if obj.is_host() {
            settings.max_host_check_spread
        } else {
            settings.max_service_check_spread
        };
        let expiration = if !st.checked {
            self.start_time + threshold
        } else if check.active_checks
            && self.start_time > st.last_check
            && notify.freshness_threshold == 0
        {
            self.start_time + threshold + i64::from(spread) * il
        } else {
            st.last_check + st.execution_time as i64 + threshold
        };
        if expiration >= now {
            return true;
        }
        if log && settings.log_level_runtime <= LogLevel::Warning {
            let stale = breakdown(now - expiration);
            let limit = breakdown(threshold);
            let line = match &obj.conf {
                ObjConf::Service { conf, .. } => format!(
                    "Warning: The results of service '{}' on host '{}' are stale by {stale} \
                     (threshold={limit}).  I'm forcing an immediate check of the service.",
                    conf.description, conf.host_name
                ),
                ObjConf::Host(host) => format!(
                    "Warning: The results of host '{}' are stale by {stale} (threshold={limit}).  \
                     I'm forcing an immediate check of the host.",
                    host.name
                ),
            };
            self.log(&line);
        }
        tracing::info!(
            host_id = self.objs[index].key.0,
            service_id = self.objs[index].key.1,
            "results are stale: forcing a check"
        );
        false
    }

    /// `check_result_freshness`: each object with `check_freshness` whose
    /// result is stale gets a forced active check now
    /// (`CHECK_OPTION_FORCE_EXECUTION | CHECK_OPTION_FRESHNESS_CHECK`).
    pub(super) fn check_freshness(&mut self, hosts: bool) {
        let now = self.clock.now();
        for index in 0..self.objs.len() {
            let obj = &self.objs[index];
            if obj.is_host() != hosts {
                continue;
            }
            let check = obj.check();
            let notify = self.notify_conf(index);
            if !notify.check_freshness
                || obj.running
                || (!check.active_checks && !check.passive_checks)
                || obj.state.being_freshened
                || (!hosts && check.check_interval == 0 && notify.freshness_threshold == 0)
                || check.check_command.is_empty()
            {
                continue;
            }
            if !self.in_check_period(index, now) || self.is_result_fresh(index, now, true) {
                continue;
            }
            self.objs[index].state.being_freshened = true;
            self.objs[index].forced = true;
            self.schedule(index, tokio::time::Instant::now());
        }
        self.launch();
    }

    /// `EVENT_SFRESHNESS_CHECK` / `EVENT_HFRESHNESS_CHECK`, created when a
    /// configuration with the check enabled is put in force
    /// (`applier/scheduler.cc:342-404`).
    pub(super) fn plan_freshness(&mut self, now: i64) {
        let settings = self.settings().clone();
        for (hosts, enabled, interval) in [
            (
                false,
                settings.check_service_freshness,
                settings.service_freshness_check_interval,
            ),
            (
                true,
                settings.check_host_freshness,
                settings.host_freshness_check_interval,
            ),
        ] {
            let slot = usize::from(hosts);
            if enabled && !self.freshness_planned[slot] {
                self.freshness_planned[slot] = true;
                self.add_timer(now + i64::from(interval.max(1)), Timer::Freshness { hosts });
            }
        }
    }

    /// The freshness timer ran: check, then plan the next one while the
    /// check is enabled.
    pub(super) fn run_freshness(&mut self, hosts: bool, now: i64) {
        let settings = self.settings().clone();
        let (enabled, interval) = if hosts {
            (
                settings.check_host_freshness,
                settings.host_freshness_check_interval,
            )
        } else {
            (
                settings.check_service_freshness,
                settings.service_freshness_check_interval,
            )
        };
        if !enabled {
            self.freshness_planned[usize::from(hosts)] = false;
            return;
        }
        self.check_freshness(hosts);
        self.add_timer(now + i64::from(interval.max(1)), Timer::Freshness { hosts });
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn breakdown_as_engine() {
        assert_eq!(super::breakdown(90_061), "1d 1h 1m 1s");
        assert_eq!(super::breakdown(-5), "0d 0h 0m 0s");
    }
}
