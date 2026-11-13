# Gorgone in Rust (proof of concept)

A replacement for the Perl `gorgoned`: a stateless central control plane
(`gorgone-server`) and a small poller executor (`gorgone-agent`).
The architecture, the protocol and the migration path are in
[docs/DESIGN.md](docs/DESIGN.md).

## Layout

| Path                    | Content                                          |
| ----------------------- | ------------------------------------------------ |
| `proto/`                | gRPC contracts                                   |
| `crates/gorgone-proto`  | generated gRPC code                              |
| `crates/gorgone-core`   | config bundles, command allowlist, legacy codes  |
| `crates/gorgone-server` | REST API, agent gateway, job store, dispatcher   |
| `crates/gorgone-agent`  | job executors and session with the server        |
| `crates/gorgone-e2e`    | tests running the real server and agent together |
| `deploy/`               | container images and a two-replica demo          |

## Build and test

```bash
cargo build --release
cargo test --workspace
cargo clippy --workspace --all-targets -- -D warnings
```

The gRPC code is checked in: building needs neither `protoc` nor a code
generator. After changing a file under `proto/`, regenerate it with
`cargo run --manifest-path tools/protogen/Cargo.toml`.

Dependencies are kept to what the stack needs (async runtime, HTTP/2, TLS,
SQL client); think twice before adding one.

The server tests run on SQLite; set
`GORGONE_TEST_DATABASE_URL=mysql://…` to run them on MariaDB.

## Run

```bash
gorgone-server --config crates/gorgone-server/config.example.toml
gorgone-agent  --config crates/gorgone-agent/config.example.toml
```

Every key can also come from the environment (`GORGONE_SERVER__…`,
`GORGONE_AGENT__…`), which is how the containers are configured: see
[deploy/compose.yaml](deploy/compose.yaml).

The server refuses to start without REST credentials and binds loopback by
default; `config.example.toml` lists what to set for a real deployment.

A configuration bundle is a deterministic `tar.zst`:

```bash
cargo run -q -p gorgone-core --example pack -- /var/cache/centreon/config/engine/1 > bundle.tar.zst
curl -H "Authorization: Bearer $TOKEN" -X PUT --data-binary @bundle.tar.zst \
     https://central:8085/v1/nodes/1/config/engine
```

## Test with a real Centreon

`deploy/centreon/` runs real Centreon components, each in its own container,
with no Perl Gorgone and no command file:

| Container        | Content                                                                     |
| ---------------- | --------------------------------------------------------------------------- |
| `gorgone-server` | the control plane alone, store in MariaDB                                   |
| `web`            | centreon-web calling the Gorgone REST API instead of writing `centcore.cmd` |
| `poller`         | the Centreon poller test image, driven by `gorgone-agent`                   |
| `engine`         | the engine image of this branch, driven over its gRPC API by a sidecar      |

It needs a checkout of the centreon-web branch that sends its commands to the
Gorgone API, and the engine image built from this branch
([deploy/centreon/engine/README.md](deploy/centreon/engine/README.md)):

```bash
export CENTREON_SRC=/path/to/centreon
docker compose -f deploy/centreon/compose.yaml up --build -d
deploy/centreon/verify.sh
docker compose -f deploy/centreon/compose.yaml down -v
```

Both pollers register themselves, then ask centreon-web to generate, push and
restart; centreon-web reports the real outcome of each job. `verify.sh` then
checks, among other things, that a forced check sent through the centreon-web
API reaches the engine, that the engine container is driven over a Unix socket
with nothing listening on the network, that `gorgone-server` knows what each
node runs, and that a reload asked on top of a failed configuration push is
not sent. The web UI is on <http://localhost:4000>.

On a poller the broker module is reloaded with the engine, so the bench maps
"broker reload" to an engine reload there.

## Benchmark

```bash
cargo test --release -p gorgone-e2e --test bench -- --ignored --nocapture
```

Each job is one command going through REST submit, the store, the agent and
back to a waiting REST client. One replica, SQLite with full synchronisation,
on a developer laptop (8 cores, WSL2). Throughput follows the cost of a disk
sync, which varied between runs on that machine:

| Agents | Clients | Throughput         | Median latency |
| ------ | ------- | ------------------ | -------------- |
| 1      | 1       | 140 to 260 jobs/s  | 3 to 7 ms      |
| 1      | 64      | 650 to 1100 jobs/s | 55 to 95 ms    |
| 32     | 256     | 600 to 1000 jobs/s | 250 to 400 ms  |

Set `GORGONE_TEST_DATABASE_URL` to measure on MariaDB.

## Containers

`deploy/Dockerfile` builds static binaries into two distroless images.
The agent image carries no plugin and no shell: it fits configuration push and
engine commands as a sidecar sharing the engine volumes. To run plugins
(`RunCommands`), copy the binary into the poller image instead:

```dockerfile
COPY --from=gorgone-agent /usr/bin/gorgone-agent /usr/bin/gorgone-agent
```
