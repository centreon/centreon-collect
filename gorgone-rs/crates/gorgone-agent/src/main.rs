//! `gorgone-agent` binary: runs on a node the jobs gorgone-server sends.

fn main() -> anyhow::Result<()> {
    gorgone_agent::cli::main(&gorgone_agent::cli::GORGONE_AGENT)
}
