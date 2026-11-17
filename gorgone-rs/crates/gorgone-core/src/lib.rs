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
    /// Check a configuration bundle without applying it.
    pub const VALIDATE_CONFIG: &str = "validate_config";
    /// Start, stop, reload or restart a local service.
    pub const SERVICE_CONTROL: &str = "service_control";
}

/// Component of an agent that does not name one: the agent next to the engine.
pub const DEFAULT_COMPONENT: &str = "engine";

/// True for a component name: `[a-z0-9_-]{1,32}`.
#[must_use]
pub fn valid_component(name: &str) -> bool {
    (1..=32).contains(&name.len())
        && name
            .bytes()
            .all(|b| b.is_ascii_lowercase() || b.is_ascii_digit() || b == b'_' || b == b'-')
}
