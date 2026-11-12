# Gorgone in Rust (proof of concept)

A replacement for the Perl `gorgoned`: a stateless central control plane
(`gorgone-server`) and a small poller executor (`gorgone-agent`).
The architecture, the protocol and the migration path are in
[docs/DESIGN.md](docs/DESIGN.md).

## Layout

| Path | Content |
| --- | --- |
| `proto/` | gRPC contracts |
| `crates/gorgone-proto` | generated gRPC code |
| `crates/gorgone-core` | config bundles, command allowlist, legacy codes |
| `crates/gorgone-server` | REST API, agent gateway, job store, dispatcher |
| `crates/gorgone-agent` | job executors and session with the server |
| `crates/gorgone-e2e` | tests running the real server and agent together |
| `deploy/` | container images and a two-replica demo |

## Build and test

```bash
cargo build --release
cargo test --workspace
cargo clippy --workspace --all-targets -- -D warnings
```

No system `protoc` is needed. The server tests run on SQLite; set
`GORGONE_TEST_DATABASE_URL=mysql://…` to run them on MariaDB.

## Run

```bash
gorgone-server --config crates/gorgone-server/config.example.toml
gorgone-agent  --config crates/gorgone-agent/config.example.toml
```

Every key can also come from the environment (`GORGONE_SERVER__…`,
`GORGONE_AGENT__…`), which is how the containers are configured: see
[deploy/compose.yaml](deploy/compose.yaml).

## Containers

`deploy/Dockerfile` builds static binaries into two distroless images.
The agent image carries no plugin and no shell: it fits configuration push and
engine commands as a sidecar sharing the engine volumes. To run plugins
(`RunCommands`), copy the binary into the poller image instead:

```dockerfile
COPY --from=gorgone-agent /usr/bin/gorgone-agent /usr/bin/gorgone-agent
```
