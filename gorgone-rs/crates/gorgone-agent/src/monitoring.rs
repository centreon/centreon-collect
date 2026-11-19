//! The built-in engine (crate `centreon-engine`, cargo feature `scheduler`)
//! as the executors see it. Without the feature the
//! handle type has no value, so every branch that uses it is dead code the
//! compiler removes, and the configuration check refuses `scheduler.enabled`.

use gorgone_proto::agent::EngineState;

use crate::config::AgentConfig;

#[cfg(feature = "scheduler")]
mod imp {
    use std::path::PathBuf;

    use super::{AgentConfig, EngineState};

    /// Handle on the running engine.
    #[derive(Clone)]
    pub(crate) struct Monitoring {
        engine: centreon_engine::Scheduler,
        /// Target the structured configuration is reported under, when the
        /// input is structured.
        structured: Option<(String, PathBuf)>,
    }

    impl Monitoring {
        /// Starts the engine when `scheduler.enabled`.
        pub(crate) fn start(config: &AgentConfig) -> Option<Self> {
            if !config.scheduler.enabled {
                return None;
            }
            let section = &config.scheduler;
            // Both checked by the configuration validation.
            let (mut options, structured) = match config.scheduler_snapshot_dir() {
                Some(dir) => (
                    centreon_engine::Options::structured(&dir),
                    Some((section.target.clone(), dir)),
                ),
                None => {
                    let mut options = centreon_engine::Options::new(config.scheduler_main_file()?);
                    if let centreon_engine::Input::Files { broker_config, .. } = &mut options.input
                    {
                        broker_config.clone_from(&section.broker_config);
                    }
                    (options, None)
                }
            };
            options.broker_address = section.broker_address.clone();
            options.max_concurrent_checks = section.max_concurrent_checks;
            options.max_pending_events = section.max_pending_events;
            options.max_output_bytes = config.max_output_bytes.min(64 * 1024);
            options.retention_file = config.scheduler_retention_file();
            if let Some(file) = &section.credentials_file {
                options.credentials_file = Some(file.clone());
            }
            Some(Self {
                engine: centreon_engine::Scheduler::start(options),
                structured,
            })
        }

        pub(crate) async fn reload(&self) -> Result<String, String> {
            self.engine.reload().await
        }

        /// Target and digest of the structured configuration: the one in
        /// force, else the one kept from a former run.
        pub(crate) fn applied(&self) -> Option<(String, Option<String>)> {
            let (target, dir) = self.structured.as_ref()?;
            let digest = self
                .engine
                .status()
                .digest
                .or_else(|| centreon_engine::kept_digest(dir));
            Some((target.clone(), digest))
        }

        /// Checks and puts a structured document in force (see
        /// `centreon_engine::Scheduler::apply`).
        pub(crate) async fn apply(
            &self,
            document: Vec<u8>,
            digest: String,
        ) -> Result<String, String> {
            self.engine.apply(document, digest).await
        }

        pub(crate) async fn external_command(&self, line: &str) -> Result<String, String> {
            self.engine.external_command(line).await
        }

        /// Checks a staged configuration: valid or not, and the report,
        /// which ends with `Total Errors:` like `centengine -v`.
        pub(crate) async fn validate(
            &self,
            main_file: &str,
            broker_config: Option<PathBuf>,
        ) -> Result<(bool, String), String> {
            let main_file = PathBuf::from(main_file);
            tokio::task::spawn_blocking(move || {
                centreon_engine::validate(&main_file, broker_config.as_deref())
            })
            .await
            .map_err(|error| format!("check task failed: {error}"))
        }

        /// Engine state of the heartbeat: reachable once a configuration
        /// is in force.
        pub(crate) fn engine_state(&self) -> EngineState {
            EngineState {
                reachable: self.engine.status().loaded,
                version: centreon_engine::VERSION.to_owned(),
            }
        }

        /// Waits for a change of what `engine_state` reports.
        pub(crate) async fn changed(&mut self) -> bool {
            self.engine.status_changed().await.is_some()
        }

        pub(crate) async fn shutdown(&self) {
            self.engine.shutdown().await;
        }
    }
}

#[cfg(not(feature = "scheduler"))]
mod imp {
    use std::path::PathBuf;

    use super::{AgentConfig, EngineState};

    /// No value: the agent is built without the scheduler.
    #[derive(Clone)]
    pub(crate) enum Monitoring {}

    impl Monitoring {
        pub(crate) fn start(_config: &AgentConfig) -> Option<Self> {
            None
        }

        pub(crate) async fn reload(&self) -> Result<String, String> {
            match *self {}
        }

        pub(crate) fn applied(&self) -> Option<(String, Option<String>)> {
            match *self {}
        }

        pub(crate) async fn apply(
            &self,
            _document: Vec<u8>,
            _digest: String,
        ) -> Result<String, String> {
            match *self {}
        }

        pub(crate) async fn external_command(&self, _line: &str) -> Result<String, String> {
            match *self {}
        }

        pub(crate) async fn validate(
            &self,
            _main_file: &str,
            _broker_config: Option<PathBuf>,
        ) -> Result<(bool, String), String> {
            match *self {}
        }

        pub(crate) fn engine_state(&self) -> EngineState {
            match *self {}
        }

        pub(crate) async fn changed(&mut self) -> bool {
            match *self {}
        }

        pub(crate) async fn shutdown(&self) {
            match *self {}
        }
    }
}

pub(crate) use imp::Monitoring;

#[cfg(all(test, feature = "scheduler"))]
mod tests {
    use std::collections::BTreeMap;
    use std::time::Duration;

    use gorgone_core::allowlist::Allowlist;
    use gorgone_proto::agent::job_finished::Outcome;
    use gorgone_proto::agent::service_control::Operation;
    use gorgone_proto::agent::{EngineCommands, ServiceControl};
    use tokio_util::sync::CancellationToken;

    use super::*;
    use crate::config::{SCHEDULER_RELOAD, ServiceConfig};
    use crate::executor::testing::run;
    use crate::executor::{
        ConfigChecker, EngineCommandsExecutor, EngineTransport, ServiceControlExecutor,
    };

    fn write(dir: &std::path::Path, services: &str) {
        std::fs::write(
            dir.join("centengine.cfg"),
            "cfg_file=objects.cfg\nbroker_module_cfg_file=module.json\n",
        )
        .unwrap();
        std::fs::write(
            dir.join("objects.cfg"),
            format!(
                "define command {{\n command_name ok\n command_line /bin/true\n}}\n\
                 define host {{\n host_name h\n _HOST_ID 1\n}}\n{services}"
            ),
        )
        .unwrap();
        std::fs::write(
            dir.join("module.json"),
            r#"{"centreonBroker":{"poller_id":3,"poller_name":"p"}}"#,
        )
        .unwrap();
    }

    const SERVICE: &str = "define service {\n host_name h\n service_description s\n \
                           check_command ok\n _SERVICE_ID 1\n}\n";

    #[tokio::test]
    async fn executors_drive_the_scheduler() {
        let dir = tempfile::tempdir().unwrap();
        write(dir.path(), SERVICE);
        let toml = format!(
            "node_id = \"3\"\nserver_url = \"http://central:5556\"\ntoken = \"t\"\n\
             [scheduler]\nenabled = true\nbroker_address = \"127.0.0.1:1\"\n\
             [targets.engine]\npath = \"{}\"\nengine_main_file = \"centengine.cfg\"\n",
            dir.path().display()
        );
        let config = AgentConfig::from_toml(&toml).unwrap();
        let monitoring = Monitoring::start(&config).unwrap();
        let mut watch = monitoring.clone();
        while !watch.engine_state().reachable {
            tokio::time::timeout(Duration::from_secs(5), watch.changed())
                .await
                .unwrap();
        }
        assert!(
            monitoring
                .engine_state()
                .version
                .starts_with("centreon-engine ")
        );

        // Configuration check: the staged file is read by the scheduler.
        let checker = ConfigChecker::Scheduler {
            monitoring: monitoring.clone(),
            broker_config: None,
        };
        let main = dir.path().join("centengine.cfg");
        let cancel = CancellationToken::new();
        let valid = checker
            .check(main.to_str().unwrap(), 4096, &cancel)
            .await
            .unwrap();
        assert_eq!(valid.exit_code, 0, "{}", valid.stdout);
        assert!(
            valid.stdout.contains("Total Errors:   0"),
            "{}",
            valid.stdout
        );
        write(dir.path(), "define service {\n host_name nope\n}\n");
        let invalid = checker
            .check(main.to_str().unwrap(), 4096, &cancel)
            .await
            .unwrap();
        assert_eq!(invalid.exit_code, 1);
        assert!(
            invalid.stdout.contains("Total Errors:   1"),
            "{}",
            invalid.stdout
        );

        // Reload refused: the running configuration stays.
        let control = ServiceControlExecutor {
            services: BTreeMap::from([(
                "engine".to_owned(),
                ServiceConfig {
                    reload: Some(SCHEDULER_RELOAD.to_owned()),
                    ..ServiceConfig::default()
                },
            )]),
            engine: None,
            monitoring: Some(monitoring.clone()),
            http_timeout: Duration::from_secs(1),
            bearer: None,
            timeout: Duration::from_secs(1),
            kill_grace: Duration::from_secs(1),
            max_output: 1024,
        };
        let reload = ServiceControl {
            service: "engine".to_owned(),
            operation: Operation::Reload.into(),
        };
        let (done, _) = run(&control, reload.clone()).await;
        assert_eq!(done.outcome(), Outcome::Failed, "{}", done.message);
        write(dir.path(), SERVICE);
        let (done, _) = run(&control, reload).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{}", done.message);
        assert!(done.message.contains("1 services"), "{}", done.message);

        // External commands: supported ones run, others fail the job.
        let commands = EngineCommandsExecutor {
            transport: EngineTransport::Scheduler(monitoring.clone()),
            allowed: Allowlist::new(["SCHEDULE_FORCED_SVC_CHECK", "DEL_SVC_DOWNTIME_FULL"])
                .unwrap(),
        };
        let action = |lines: &[&str]| EngineCommands {
            commands: lines.iter().map(|line| (*line).to_owned()).collect(),
        };
        let (done, _) = run(&commands, action(&["[1] SCHEDULE_FORCED_SVC_CHECK;h;s;1"])).await;
        assert_eq!(done.outcome(), Outcome::Ok, "{}", done.message);
        let (done, _) = run(
            &commands,
            action(&[
                "[1] SCHEDULE_FORCED_SVC_CHECK;h;s;1",
                "[1] DEL_SVC_DOWNTIME_FULL;h;s;;;;;;",
            ]),
        )
        .await;
        assert_eq!(done.outcome(), Outcome::Failed);
        assert!(done.message.contains("not supported"), "{}", done.message);
        assert!(done.message.contains("1 of 2"), "{}", done.message);
        monitoring.shutdown().await;
    }
}
