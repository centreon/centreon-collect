# Gorgone in Rust — design

Status: proof of concept. The decision itself still has to go through an ADR
(`docs/adr/README.md`).

## Goals

1. Remove Perl from the Gorgone code path as fast as possible.
2. A central side that scales horizontally: any number of identical replicas,
   no local state, safe to run in containers.
3. A poller side that is one small static binary, usable as a systemd service
   or as a sidecar of `centengine`.
4. Low latency: results are pushed, never polled between nodes.

Non-goals: being wire compatible with the Perl daemon between central and
pollers (ZMQ, SSH, pull, pullwss are dropped), and being a broker module
(the `cbd` module ABI is C++, and pollers usually run no `cbd`).

## Components

```
              REST (legacy /api + /v1)                gRPC (TLS)
 centreon-web ───────────────┐          ┌──────────────── gorgone-agent (poller 1)
 CLAPI, BAM, trapd           ▼          ▼                 gorgone-agent (poller 2)
 centcore.cmd ──────►  gorgone-server × N  ◄───────────── gorgone-agent (remote…)
 (legacy intake)             │   ▲
                             ▼   │ peer wake-up hints (gRPC)
                       shared SQL store (MariaDB; SQLite for one replica)
```

| Crate            | Role                                                                  |
| ---------------- | --------------------------------------------------------------------- |
| `gorgone-proto`  | gRPC contracts (`proto/`): agent gateway and peer service             |
| `gorgone-core`   | Shared domain: config bundles, command allowlist, legacy codes        |
| `gorgone-server` | Central control plane: REST API, agent gateway, job store, dispatcher |
| `gorgone-agent`  | Poller executor: runs jobs received on its session                    |

## Principles

- **The store is the source of truth.** A job exists once it is committed.
  Everything else (in-memory session map, peer hints) is an optimisation that
  can be lost without losing a job.
- **Agents dial in.** One outbound HTTP/2 connection per agent, to any replica
  (load balancer friendly, no inbound port on pollers).
- **Typed actions, enforced on the agent.** The central never sends a path, a
  user or a restart command line. The agent maps an action to its own local
  configuration and applies its own allowlist. A compromised central can only
  ask for what the poller administrator allowed.
- **At-least-once, idempotent.** Dispatch and event delivery may be repeated;
  job ids and event sequence numbers make repetition harmless.
- **Control stream and bulk data are separate.** Configuration bundles are
  content-addressed blobs fetched with a dedicated streaming RPC.

## Domain model

- **Node**: a poller or remote server, identified by `node_id` (the
  `nagios_server.id` of Centreon, as a string). Has an enrolment token
  (stored hashed).
- **Job**: `id` (the legacy "token"), `node_id`, one typed `Action`, a
  deadline, a state.
- **Job event**: append-only, `(job_id, seq)`; `seq` starts at 1 and is
  assigned by whoever produces the event (server uses seq 0 for "accepted").
- **Blob**: content-addressed (`sha256` hex) byte string, stored in chunks.

### Actions

| Action           | Payload                                                        | Agent behaviour                                                                                                                                                                                                 |
| ---------------- | -------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `RunCommands`    | list of `{command, timeout_s, continue_on_error}`              | each command must match the agent allowlist entirely, hold no control character, and name its program with an absolute path free of `.`/`..`; executed without a shell unless the agent is configured otherwise |
| `EngineCommands` | list of external command lines                                 | each command name must match the agent's list of allowed external commands; sent to the engine gRPC API when the agent has its address, else written to the engine command pipe                                 |
| `ApplyConfig`    | `target` (e.g. `engine`, `broker`, `vmware`), `digest`, `size` | fetch blob, verify digest, sync into the directory the agent maps to `target`; no-op when the digest is already applied                                                                                         |
| `ServiceControl` | `service`, `operation` (start, stop, reload, restart)          | runs what the agent maps to `(service, operation)`: a local command, or an engine gRPC call (reload, shutdown) when the engine is another container                                                             |

### Job state machine

```
queued ──► dispatched ──► running ──► succeeded | failed
   │            │
   │            └─(session lost before any event)──► queued
   └─(deadline passed)──► expired
```

`cancelled` is a fourth terminal state, reached from `queued` directly or
from `dispatched`/`running` through a `Cancel` frame.

A job is claimed with a compare-and-set (`queued → dispatched`) that also
checks the claiming session is still the current one of the node, so that two
replicas never dispatch it twice and a replaced session claims nothing. A
terminal event from the agent sets the final state. Jobs past their deadline
become `expired` (sweeper). A job that is `dispatched` to a session that no
longer exists goes back to `queued`; this happens before anything else is
dispatched to the node.

**Order.** The store gives every job a per-node sequence number inside the
submit transaction. Jobs of a node are claimed and sent in that order, one
drain at a time per node.

**Time.** Stored timestamps, liveness cut-offs, leases and deadlines use the
database clock (each replica tracks its offset to it), never a comparison of
two hosts' clocks. A job carries the time left (`ttl_ms`) when dispatched and
the agent counts from reception.

### Legacy log codes

The REST compatibility layer exposes events with the Perl codes:
`0` accepted (begin), `1` finished KO, `2` finished OK, `4` started or
progress, `100` command result. An expiry or a cancellation is a code `1`
event written by the server.

## Agent protocol (`gorgone.agent.v1`)

`Session` is a bidirectional stream:

1. Agent sends `Hello {node_id, agent_version, capabilities}`. The enrolment
   token travels in the `authorization: Bearer` metadata and always identifies
   the node. With mTLS, the client certificate additionally proves the agent
   is trusted by the CA; it is not read for identity.
2. Server answers `Welcome {session_id, heartbeat_interval_s}` and registers
   the session in the store (`node_id → replica`). A newer session for the same
   node replaces the older one.
3. Server sends `Dispatch {job}`. Agent emits `JobEvent`s; the last one has a
   `finished` body. Server answers `EventAck {job_id, seq}` once the event is
   committed. The agent keeps unacknowledged events and resends them after a
   reconnection.
4. Agent sends `Heartbeat` every interval; a session without heartbeat for
   three intervals is closed.
5. The agent remembers recently seen job ids: a duplicate `Dispatch` of a
   running job is ignored, of a finished job replays its events. With a
   `state_dir` this journal survives an agent restart; a job interrupted by
   the restart is reported failed, never run twice.
6. `Cancel {job_id}` stops a running job (SIGTERM, then SIGKILL).

`EventAck` acknowledges one event (exact `seq`). `ApplyConfig.size = 0` means
unknown. Jobs that change the poller state (`ApplyConfig`, `ServiceControl`,
`EngineCommands`) run one at a time in dispatch order, so a reload never
overtakes the configuration push submitted before it; `RunCommands` jobs run
concurrently.

`FetchBlob {digest}` streams the blob in chunks (≤ 1 MiB). Same authentication.

## Multi-replica operation

- Tables: `replicas` (id, peer address, last seen), `sessions` (node →
  replica), `leases` (named, time-bound leader locks).
- **Submit path**: API replica commits the job, then: session is local → push
  now; session on another replica → `Peer.Wake(node_id)` to its peer address;
  no session → stays `queued`.
- **Owner replica** on wake-up, on agent connection and every few seconds:
  claims and dispatches the queued jobs of the nodes it holds.
- **Result path**: the owner replica commits events. A REST replica waiting for
  a job (long poll or SSE) is notified locally when it owns the session, and
  falls back to short store polls otherwise.
- **Singleton work** (purge, legacy `centcore.cmd` intake, node sync from the
  Centreon database) runs only on the replica holding the matching lease.
- A replica that stops heartbeating has its sessions dropped by the others.

With one replica and SQLite the same code runs with no extra component, which
is the default on-premise install.

## Store schema

Portable SQL (MariaDB and SQLite), times in Unix milliseconds.

```
nodes       (id PK, name, token_hash, created_at)
replicas    (id PK, peer_addr, last_seen)
sessions    (node_id PK, session_id, replica_id, agent_version, connected_at, last_seen)
jobs        (id PK, node_id, seq, kind, action BLOB, state, attempts,
             session_id NULL, cancel_requested, created_at, updated_at, deadline_at)
job_events  (job_id, seq, time_ms, code, data TEXT(JSON), PRIMARY KEY (job_id, seq))
blobs       (digest PK, size, created_at)
blob_chunks (digest, idx, data BLOB, PRIMARY KEY (digest, idx))
leases      (name PK, holder, expires_at)
meta        (name PK, value)   -- stable server id, shared by all replicas
```

## REST API

### Native (`/v1`)

| Method           | Path                               | Purpose                                                                                                                      |
| ---------------- | ---------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| `POST`           | `/v1/nodes/{node}/jobs`            | submit a job (JSON action); returns `{id}`                                                                                   |
| `GET`            | `/v1/jobs/{id}`                    | job with state and events; `?wait=<seconds>` long-polls until terminal                                                       |
| `GET`            | `/v1/jobs/{id}/events`             | server-sent events, closes on the terminal event                                                                             |
| `DELETE`         | `/v1/jobs/{id}`                    | cancel: immediate when queued, `Cancel` frame through the owner replica otherwise                                            |
| `PUT`            | `/v1/nodes/{node}/config/{target}` | body is a `tar.zst` bundle, or a plain tar with `Content-Type: application/x-tar`; stores the blob and submits `ApplyConfig` |
| `GET`            | `/v1/nodes`                        | nodes with connection status                                                                                                 |
| `PUT` / `DELETE` | `/v1/nodes/{node}`                 | enrol (returns the token once) / remove a node                                                                               |
| `GET`            | `/healthz`, `/readyz`, `/metrics`  | probes and Prometheus metrics                                                                                                |

JSON action shape: `{"run_commands": {"commands": [{"command": "...", "timeout_s": 30}]}}`,
`{"engine_commands": {"commands": ["..."]}}`,
`{"apply_config": {"target": "engine", "digest": "...", "size": 0}}`,
`{"service_control": {"service": "engine", "operation": "reload"}}`.

### Legacy compatibility (`/api`)

Kept so that centreon-web and the monitoring plugin work unchanged:

| Legacy route                                              | Mapping                                                                                                                                                                                       |
| --------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `POST /api/nodes/{id}/core/action/command`                | `RunCommands`; body `[{command, timeout, continue_on_error}]`; answers `{"token": id}`                                                                                                        |
| `POST /api/nodes/{id}/centreon/engine/command`            | `EngineCommands`; body `{commands: [...]}`                                                                                                                                                    |
| `GET /api/log/{token}`, `GET /api/nodes/{id}/log/{token}` | `{"message":"Logs found","token":…,"data":[{id,token,code,etime,ctime,instant,data}]}`; when empty: `{"error":"no_log","message":"No log found for token","data":[],"token":…}` with HTTP 200 |
| `GET /api/internal/constatus`                             | `{"action":"constatus","message":"ok","data":{node: {last_ping_sent, last_ping_recv, nodes:{}, ping_ok, ping_failed}}}` built from sessions                                                   |
| `GET /api/internal/information`                           | `{"action":"information","message":"ok","data":{…counters, "modules":{…}, "api_endpoints":{…}}}`                                                                                              |
| `GET /api/[nodes/{id}/]internal/thumbprint`               | `{"action":"getthumbprint","message":"ok","data":{"thumbprint": …}}` (stable server identifier; kept for the web popup)                                                                       |
| anything else                                             | `{"error":"method_unknown","message":"Method not implemented","http_response_code":"404"}`                                                                                                    |

`log_wait` / `sync_wait` query parameters are accepted and translated to a
bounded wait on the job.

### Legacy `centcore.cmd` intake

A leader-only task tails `/var/lib/centreon/centcore.cmd` and
`/var/lib/centreon/centcore/*.cmd` (`CMD:node[:param]` lines) and submits the
equivalent jobs: `EXTERNALCMD` → `EngineCommands` (batched per node),
`SENDCFGFILE` → one `ApplyConfig` per target built from
`/var/cache/centreon/config/{engine,broker,vmware}/{id}`,
`RELOAD`/`RESTART`/`START`/`STOP`/`RELOADBROKER`/`*CENTREONTRAPD`/`VMWARERESTART`
→ `ServiceControl`. Job ids are derived from the file and the line, so a
replay after a crash submits nothing twice. `SENDEXPORTFILE` and `STARTWORKER`
(remote-server import and export) are not ported. It disappears once the
writers call the API.

## Security

- TLS everywhere (rustls); optional mTLS for agents. The peer listener takes
  the same TLS settings and a shared secret.
- Agent enrolment token: random 32 bytes, shown once, stored as SHA-256.
- REST API: bearer token or basic auth, plus an allowed-CIDR list (same
  operational model as today).
- The agent runs unprivileged. Privileged operations are explicit entries of
  its local configuration (for instance `sudo systemctl reload centengine`),
  never text received from the central.
- Bundles: digest verified before any write, no absolute path, no `..`, no
  link or device entries, no write through a linked directory, file modes
  reduced to 0644/0755.
- The server fails closed: it does not start without REST credentials, binds
  loopback by default, and refuses a plaintext non-loopback listener unless
  told so explicitly.
- An agent is authenticated from the call metadata before its stream is read.
  It can only post events for its own running jobs and fetch blobs its own
  jobs reference; events per job are capped.
- Limit of the model: a node with an `engine` target lets the central define
  the commands its engine runs. That is what configuration push means; the
  target should exist only on nodes the central is meant to configure.

## Configuration

One TOML file per binary, every key overridable by environment
(`GORGONE_SERVER__…`, `GORGONE_AGENT__…`), so that containers need no file.

## Migration away from Perl

| Step                                                                                                                                  | Perl removed                                                                                                                   |
| ------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| 1. `gorgone-server` + `gorgone-agent`: commands, engine commands, config push, service control, legacy REST and `centcore.cmd` intake | core, proxy, action, engine, legacycmd (minus remote-server export), nodes, register, pull, pullwss, httpserver(ng), dbcleaner |
| 2. Statistics                                                                                                                         | better served by the broker/engine gRPC stats and metrics than by a port of `statistics`                                       |
| 3. Autodiscovery, audit                                                                                                               | port as server-side services submitting `RunCommands` jobs                                                                     |
| 4. Cron                                                                                                                               | systemd timers / Kubernetes CronJobs; no scheduler in Gorgone                                                                  |
| 5. MBI ETL, anomaly detection, judge, newtest, scom                                                                                   | owned by their products; they only need `gorgone::…` libraries today                                                           |

Mixed fleets during an upgrade: the central keeps the Perl daemon only as a
relay for pollers that still run it, until they get the agent.
