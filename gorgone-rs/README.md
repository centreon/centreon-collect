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
with no volume shared between containers and no RRD file. centreon-web runs
alone in a slim image: no centreon-collect, Gorgone, plugin, Perl, Python or
`rrdtool` next to it. Configuration, keys and commands go over the network
(rules: [docs/containers-contract.md](docs/containers-contract.md)).

| Container          | Content                                                                           |
| ------------------ | --------------------------------------------------------------------------------- |
| `gorgone-server`   | the control plane alone, store in MariaDB                                         |
| `web`              | the slim centreon-web image (PHP and Apache), calling the Gorgone REST API        |
| `victoria-metrics` | the time series store; centreon-web reads its graphs from it                      |
| `broker`           | the central broker; receives its configuration and key through its control API    |
| `central-engine`   | the engine of the "Central" poller and its `gorgone-agent`, in the same container |
| `engine`           | a second container-only poller, same image; registers and deploys itself          |
| `poller`           | the Centreon poller test image, driven by `gorgone-agent`                         |

Inside an engine container the agent drives the engine over its gRPC API on a
local Unix socket. The Central agent pushes the broker configuration and the
encryption key to the broker container and reloads it through its control API.
centreon-web pushes the key (`engine_context`) before every configuration.
Only VictoriaMetrics keeps a volume, its own; `central-setup` reads one SQL
file that points the broker flows at the broker container and at
VictoriaMetrics.

It needs a checkout of the centreon-web branch that sends its commands to the
Gorgone API (its slim image is built from `CENTREON_SRC`), and the engine and
broker images built from this branch
([deploy/centreon/engine/README.md](deploy/centreon/engine/README.md),
[deploy/centreon/broker/README.md](deploy/centreon/broker/README.md)):

```bash
export CENTREON_SRC=/path/to/centreon
docker compose -f deploy/centreon/compose.yaml up --build -d
deploy/centreon/verify.sh
docker compose -f deploy/centreon/compose.yaml down -v
```

`verify.sh` runs from the host and checks, among other things, that the web
image carries no collect, Gorgone, plugin package nor `rrdtool`, that no volume
is shared between containers, that the central broker sees every engine
running, that a forced check sent through the centreon-web API reaches the
engine, that the engine API is not reachable over the network, that a reload
asked on top of a failed configuration push is not sent, that a configuration
is verified by the poller's own engine and refused when invalid, and that
performance data goes engine, broker, VictoriaMetrics and back to an API v2
graph, with no RRD file anywhere. The data model is in
[docs/victoria-metrics-contract.md](docs/victoria-metrics-contract.md). The web
UI is on <http://localhost:4000>.

On the `poller` test image the broker module is reloaded with the engine, so
the bench maps "broker reload" to an engine reload there. An `engine` target
must keep `prune = false`: pruning would delete the key file the
`engine_context` target writes in the same directory.

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
