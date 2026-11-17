//! `centreon-engine` binary: the whole poller, in place of centengine and
//! cbd. The agent with its built-in engine (crate `centreon-engine`) always
//! on, taking its configuration from gorgone-server.

fn main() -> anyhow::Result<()> {
    gorgone_agent::cli::main(&gorgone_agent::cli::CENTREON_ENGINE)
}
