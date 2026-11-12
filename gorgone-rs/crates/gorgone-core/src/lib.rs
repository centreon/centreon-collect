//! Domain pieces shared by `gorgone-server` and `gorgone-agent`.

pub mod allowlist;
pub mod bundle;
pub mod legacy;

/// Action kind names, used as agent capabilities and as the `kind` job column.
pub mod kind {
    /// Run allowlisted commands.
    pub const RUN_COMMANDS: &str = "run_commands";
    /// Write external commands to the engine command pipe.
    pub const ENGINE_COMMANDS: &str = "engine_commands";
    /// Apply a configuration bundle.
    pub const APPLY_CONFIG: &str = "apply_config";
    /// Start, stop, reload or restart a local service.
    pub const SERVICE_CONTROL: &str = "service_control";
}
