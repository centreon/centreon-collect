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

| Crate             | Role                                                                                 |
| ----------------- | ------------------------------------------------------------------------------------ |
| `gorgone-proto`   | gRPC contracts (`proto/`): agent gateway and peer service                            |
| `gorgone-core`    | Shared domain: config bundles, command allowlist, legacy codes                       |
| `gorgone-server`  | Central control plane: REST API, agent gateway, job store, dispatcher                |
| `gorgone-agent`   | Poller executor: runs jobs received on its session                                   |
| `centreon-engine` | Monitoring engine run by the `centreon-engine` program, in place of Engine and cbmod |

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
  `nagios_server.id` of Centreon, as a string). Has a token (stored hashed):
  given by `PUT /v1/nodes/{node}`, or the credential its agent chose when it
  enrolled (see [Enrolment](#enrolment)).
- **Enrolment token**: given by the central to add a node with nothing but
  the server address; stored as its SHA-256, bound to a node (single use) or
  a fleet token (node chosen later by the central), always expiring, with an
  optional number of uses.
- **Enrolment**: one use of a token by one agent credential: `pending`
  (fleet token, no node yet), `approved` (an administrator accepted it, the
  token asked for it), `enrolled`, `completed` (the central finished
  setting the node up), `rejected` or `revoked` (its node was deleted).
- **Job**: `id` (the legacy "token"), `node_id`, one typed `Action`, a
  deadline, a state.
- **Job event**: append-only, `(job_id, seq)`; `seq` starts at 1 and is
  assigned by whoever produces the event (server uses seq 0 for "accepted").
- **Blob**: content-addressed (`sha256` hex) byte string, stored in chunks.

### Actions

| Action           | Payload                                                        | Agent behaviour                                                                                                                                                                                                  |
| ---------------- | -------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `RunCommands`    | list of `{command, timeout_s, continue_on_error}`              | each command must match the agent allowlist entirely, hold no control character, and name its program with an absolute path free of `.`/`..`; executed without a shell unless the agent is configured otherwise  |
| `EngineCommands` | list of external command lines                                 | each command name must match the agent's list of allowed external commands; sent to the engine gRPC API when the agent has its address, else written to the engine command pipe                                  |
| `ApplyConfig`    | `target` (e.g. `engine`, `broker`, `vmware`), `digest`, `size` | fetch blob, verify digest, have an engine configuration checked, then sync into the directory the agent maps to `target`; no-op when the digest is already applied                                               |
| `ValidateConfig` | `target`, `digest`, `size`                                     | same check as `ApplyConfig`, nothing applied                                                                                                                                                                     |
| `ServiceControl` | `service`, `operation` (start, stop, reload, restart)          | runs what the agent maps to `(service, operation)`: a local command, an engine gRPC call (reload, shutdown), HTTP POSTs to a control API, or a signal to the process of a pid file (`cbd` in a broker container) |

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
checks the claiming session is still the current one of the node and
component the job is for, so that two replicas never dispatch it twice and a
replaced session claims nothing. A terminal event from the agent sets the
final state. Jobs past their deadline become `expired` (sweeper). A job that
is `dispatched` to a session that no longer exists goes back to `queued`;
this happens before anything else is dispatched to that component of the
node.

**Order.** The store gives every job a per-node sequence number inside the
submit transaction. Jobs of a component of a node are claimed and sent in
that order, one drain at a time per node and component.

**Prerequisites.** A job can name jobs of the same node that must have
succeeded (`requires`), or ask for the latest configuration push of each
target (`requires_latest_config`). The server holds it, and everything after it
for that node, until they finish, and ends it as `failed` without sending it if
one did not succeed. A restart therefore never runs on top of a failed
configuration push.

**Retries.** A client may give the job id. Submitting the same id again
returns the existing job, so a request replayed after a timeout creates
nothing.

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

`Enroll {token, credential, name, address, agent_version, component}` is the
only call without node identity, on the same listener and TLS as the
sessions; see [Enrolment](#enrolment). `RotateCredential {new_credential}`
is authenticated like a session; see [Credential rotation](#credential-rotation).

`Session` is a bidirectional stream:

1. Agent sends `Hello {node_id, agent_version, capabilities, component,
targets}`. The node token (or the credential chosen at enrolment) travels
   in the `authorization: Bearer` metadata and always identifies the node. With mTLS, the client certificate
   additionally proves the agent is trusted by the CA; it is not read for
   identity. `component` (empty: `engine`) and `targets` (the configuration
   targets the agent maps) are described in [Components](#components-of-a-node).
2. Server answers `Welcome {session_id, heartbeat_interval_s,
rotate_credential}` and registers
   the session in the store (`(node_id, component) → replica`). A newer
   session for the same node and component replaces the older one; the
   sessions of the other components of the node are kept.
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

## Enrolment

Adding a node needs only the server address and an enrolment token:

```
centreon-web                     gorgone-server                      new agent
 POST enrolment (admin) ──► PUT /v1/enrolment-tokens/{id}
   token shown once           {token_hash, node_id?, expires_in_s, max_uses?}
                                                ◄── Enroll(token, credential, name, address)
                              bound token: node := node_id, its token hash := SHA-256(credential)
                              fleet token: enrolment pending ──► PENDING (asks again later)
 cron: GET /v1/enrolments ──►
   creates the poller,
   POST /v1/enrolments/{id}/assign {node_id} ──► node created with the credential hash
                                                ◄── Enroll(same token, same credential)
                                                ──► ENROLLED {node_id}
                                                ◄── Session (node_id, Bearer credential)
 cron: node connected, never configured: export, then POST /v1/enrolments/{id}/complete
 poller deleted: DELETE /v1/nodes/{id} ──► enrolment revoked, sessions refused
```

- **Credential chosen by the agent.** The agent generates 32 random bytes,
  keeps them in its `state_dir` (file `identity`, mode 0600, written
  atomically) before its first call, and sends them with the token. The
  server stores only their SHA-256, as the token hash of the node: sessions
  then authenticate exactly as with a token of `PUT /v1/nodes/{node}`. No
  secret travels from the server to the agent, and a lost answer is
  harmless: the same token and credential always get the same answer.
- **Uses** are counted once per credential, with a compare-and-set in the
  transaction that records the enrolment: concurrent agents never exceed
  `max_uses`. A token bound to a node is single use. Expiry uses the
  database clock (`expires_in_s` is relative).
- **Fleet tokens** leave the node to the central: the server records a
  `pending` enrolment with the name and address the agent reported (its
  host name by default), and the central assigns the node id it chose (the
  `nagios_server.id` of the poller it created). Node ids stay the poller ids
  of the central; the server never invents one.
- **Errors** do not tell an unknown, expired or used-up token apart
  (`PERMISSION_DENIED`, the reason in the server log only); the agent
  retries with backoff, so it can start before its token is pushed.
- **Revocation.** Deleting a node revokes the enrolments that gave it its
  credential: its sessions are closed and refused, and `auto_enroll` (the
  development mode that accepts unknown nodes with any token) refuses that
  node id too, so it cannot come back without a new enrolment.
- **Existing agents** with a configured `node_id` and token, or with
  `auto_enroll`, work as before.
- **Approval** (opt-in per fleet token, `approval_required`). The
  enrolments of such a token stay `pending` until an administrator approves
  them (`POST /v1/enrolments/{id}/approve` → `approved`); `assign` refuses a
  pending one (409), so the central creates no poller before. `reject`
  (from `pending` or `approved`) is final: the agent presenting that token
  and credential gets `REJECTED` (not an error: only the holder of both
  learns it), logs it and asks again once an hour, so that a supervisor does
  not restart it in a loop and a new token given in its configuration is
  used at its next start. A token bound to a node never asks for approval.
- **Purge.** The replica holding the `purge` lease deletes, every minute,
  the tokens expired, or used up, for longer than
  `enrolment_token_retention_s` (one day). A token one of whose enrolments
  is `pending` or `approved` is kept: its agent still asks with it. The
  retention covers an agent that enrolled but died before keeping its node
  id: it asks again with the same token and credential. Enrolments are kept
  (history, revocation by node).
- **Rate limit.** Each replica keeps, per source address, a token bucket of
  refused `Enroll` attempts (`enrol_rate_per_minute`, 10, and
  `enrol_rate_burst`, 20; 0 disables it). Only refusals (unknown, expired,
  used-up token, malformed request) take a token, so agents asking again
  while their enrolment waits are never slowed down; an address out of
  budget gets `RESOURCE_EXHAUSTED` before the store is read. Limits: in
  memory, per replica (an address gets one budget per replica the balancer
  sends it to), and the address is the one the replica sees (the
  balancer's when it hides the source); at most 10 000 addresses are
  tracked, full buckets are forgotten first.

### Credential rotation

The credential an agent chose at enrolment is replaced by a new one it
generates, every `credential_rotation_days` (agent setting, 30; 0: on
request only; the age is the modification time of the identity file) and
when the central asks (`POST /v1/nodes/{id}/rotate-credential`: the node is
marked, its sessions are closed, and the `Welcome` of the next one carries
`rotate_credential`). Agents whose token comes from their configuration do
not rotate (they log the request).

```
agent                                        server (nodes row)
1. identity file: credential=C, next_credential=N   token_hash=h(C)
2. RotateCredential(N)  [Bearer C]  ──►      next_token_hash=h(N): C and N accepted
3. RotateCredential(N)  [Bearer N]  ──►      token_hash=h(N), next=NULL, sessions dropped
   (or any session opened with N)            C refused from now on
4. identity file: credential=N
```

- Each step is idempotent, and the agent runs steps 2 to 4 again at start
  when its file still holds `next_credential`, so a crash anywhere leaves a
  working credential: before step 2 the server only knows C; between 2 and
  3 both work; after 3 the file still has N, which works (step 2 is then
  refused, step 3 confirms). Proposing the credential already in force
  answers `done`.
- The switch is a compare-and-set on `next_token_hash` in the same
  transaction as the removal of the node's sessions: a session opened with
  the former credential is closed (at once on the replica that switched,
  at the next heartbeat on the others) and the agent reconnects with N.
- A rotation that never finishes (agent gone with its file) leaves C
  valid; the next proposal replaces N. A node accepted by `auto_enroll`
  has no credential to rotate (`FAILED_PRECONDITION`, REST 409).

## Components of a node

A node may be driven by several agents, one per component, each with its own
session: on a central whose broker runs in its own container, the agent next
to the engine is the `engine` component and the agent next to `cbd` the
`broker` component. An agent declares its component (`engine` by default)
and the names of the configuration targets it maps; the server keeps both per
node (`node_components`), also while the agent is away, since they decide
where jobs go. One agent per node, without a component, is the `engine`
component and works as before.

Each job is for one component, chosen at submission from the components the
node had then:

| Job                             | Component                                                                                                                                |
| ------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `ApplyConfig`                   | every component that declared the target; the engine too unless it declared targets without this one while another component declared it |
| `ValidateConfig`                | the first of these, the engine when it is one: one check is enough                                                                       |
| `ServiceControl`                | the component named like the service when the node has one (`broker` reload goes to the broker component), the engine otherwise          |
| `RunCommands`, `EngineCommands` | the engine                                                                                                                               |

A configuration push that goes to several components is one job for the
first one (the engine when it is one) and a copy `<id>.<component>` for each
other one, committed in the same transaction and ahead of it in the order of
the node. Each copy has the prerequisites of the job; the job also requires
every copy. So the push succeeds only when every component applied it, as an
`ApplyConfig` to several destinations of one agent did, a restart that
requires the latest push waits for all of them, and a failed copy fails the
job without sending it. Cancelling the job cancels its copies. Both
`engine_context` (the engine and `cbd` decrypt with it) and `broker` (the
engine module and `cbd` read it) are pushed this way to the central's two
containers. A component must have connected once for pushes to be routed to
it: a push submitted before goes to the components known then.

Order and holding are per node and component: a job waiting for a
prerequisite holds the jobs after it of its own component, and a component
whose agent is away holds nothing of the others. Prerequisites (`requires`,
`requires_latest_config`) may be jobs of any component of the node. When a
prerequisite ends, the drains of the node held by this replica resume at
once; a component held by another replica resumes at its next sweep.

An agent posts events for the jobs of its own component only, and a session
that replaces another one requeues only the jobs of its component.

## Configuration validation

A configuration is checked where it will run, by the engine that will load
it, never on the central:

- before applying a bundle to a target that holds an engine configuration, the
  agent unpacks it aside, inside the target directory, and has the engine
  verify it (through its API, or with a local command). An invalid bundle is
  refused and the live files are not touched. A push queued for a
  disconnected poller is therefore checked when the poller comes back;
- `ValidateConfig` runs the same check without applying, for immediate
  feedback at export time. It expires quickly: nobody waits for it once the
  export page is gone.

The central needs no engine binary, and a configuration is checked by the
engine version that will run it.

## centreon-engine (no C++ Engine on the poller)

The program `centreon-engine` (second binary of the agent crate, built with
the cargo feature `scheduler`) is the agent with its built-in engine always
on: the whole poller. The crate `centreon-engine` takes the poller
configuration, runs the checks with Engine's semantics (SOFT/HARD,
intervals, timeouts, output and perfdata parsing) and sends the results to
the central broker over BBDO on TCP, as `cbmod` does: no `centengine`, no
`cbd`, no C++ on the poller. `gorgone-agent` built with the feature and
`[scheduler] enabled` runs the same engine. Design, protocol and C++
sources: [scheduler-design.md](scheduler-design.md).

- Configuration input (`[scheduler] input`): `structured` (default of
  `centreon-engine`) takes the JSON document of
  [`schemas/engine-configuration.v5.json`](../schemas/engine-configuration.v5.json)
  (or versions 1 to 4) through `ApplyEngineConfiguration` jobs, with no file on
  the poller, after the `engine_context` key its `encrypt::` values need;
  `files` reads the `.cfg` files of the `engine` target, checked by the
  engine's own parser (same staging, same `Total Errors:` report). Both give
  the same model, checked by the same code.
- `ApplyEngineConfiguration{target, digest, size}`: the blob is the document
  compressed with zstd, fetched with `FetchBlob` like a bundle. The engine
  checks it, refuses it as a whole when invalid (the configuration in force
  stays), keeps it in `state_dir` to restart with it, applies it and sends
  the central only what changed. The node then reports the digest as
  applied to the target (`engine`): `requires_latest_config` treats it as
  the push of that target.
- `ServiceControl` reload and restart map to `scheduler:reload`: an
  in-process reload of the files, or of the kept document.
- `EngineCommands`: the forced and planned checks and the passive results
  are handled; any other command fails the job ("not supported by the
  built-in scheduler").
- The heartbeat reports the engine (`reachable` once a configuration is in
  force, version `centreon-engine x.y.z`).
- Out of scope for now: notifications, downtimes, acknowledgements,
  dependencies, timeperiods (24x7), event handlers, flapping, freshness,
  retention across restarts, CMA, severities and tags.

## Node state

Each heartbeat reports what the node really runs: the digest applied for every
configuration target, and whether the engine answers on its API, with its
version. The server stores it when it changes and serves it with the node, so
that a pushed configuration can be told from an applied one. Each component
reports its own; the node shows the engine's, and every component's under
`components`.

## Multi-replica operation

- Tables: `replicas` (id, peer address, last seen), `agent_sessions` (node
  and component → replica), `leases` (named, time-bound leader locks).
- **Submit path**: API replica commits the job, then: session of its
  component is local → push now; session on another replica →
  `Peer.Wake(node_id)` to its peer address, which drains every component of
  the node it holds; no session → stays `queued`.
- **Owner replica** on wake-up, on agent connection and every few seconds:
  claims and dispatches the queued jobs of the nodes it holds.
- **Result path**: the owner replica commits events. A REST replica waiting for
  a job (long poll or SSE) is notified locally when it owns the session, and
  falls back to short store polls otherwise.
- **Singleton work** (purge of jobs, blobs and enrolment tokens, legacy
  `centcore.cmd` intake, node sync from the Centreon database) runs only on
  the replica holding the matching lease.
- A replica that stops heartbeating has its sessions dropped by the others.

With one replica and SQLite the same code runs with no extra component, which
is the default on-premise install.

## Store schema

Portable SQL (MariaDB and SQLite), times in Unix milliseconds.

```
nodes       (id PK, name, token_hash, created_at, state NULL, state_at NULL,
             next_token_hash NULL, rotate_requested, credential_rotated_at NULL)
replicas    (id PK, peer_addr, last_seen)
agent_sessions (node_id, component, session_id, replica_id, agent_version,
             connected_at, last_seen, PRIMARY KEY (node_id, component))
             -- replaces `sessions` (node_id PK): its rows move as engine sessions
node_components (node_id, component, targets TEXT(JSON), state TEXT(JSON) NULL,
             state_at NULL, updated_at, PRIMARY KEY (node_id, component))
jobs        (id PK, node_id, component DEFAULT 'engine', seq, kind, action BLOB,
             state, attempts, session_id NULL, cancel_requested, created_at,
             updated_at, deadline_at, parent_id NULL -- job a copy belongs to)
job_events  (job_id, seq, time_ms, code, data TEXT(JSON), PRIMARY KEY (job_id, seq))
blobs       (digest PK, size, created_at)
blob_chunks (digest, idx, data BLOB, PRIMARY KEY (digest, idx))
leases      (name PK, holder, expires_at)
meta        (name PK, value)   -- stable server id, shared by all replicas
enrolment_tokens (id PK, token_hash UNIQUE, node_id NULL, name NULL,
             expires_at, max_uses NULL, uses, created_at, approval_required,
             last_used_at NULL)
enrolments  (id PK, token_id, credential_hash, state, node_id NULL, name,
             address, agent_version, component, created_at, updated_at,
             fleet, approval_required, UNIQUE (token_id, credential_hash))
```

## REST API

### Native (`/v1`)

| Method           | Path                                          | Purpose                                                                                                                                                                                                                                                                                                                                            |
| ---------------- | --------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `POST`           | `/v1/nodes/{node}/jobs`                       | submit a job (JSON action, optional `id`, `requires`, `requires_latest_config`); returns `{id}`                                                                                                                                                                                                                                                    |
| `GET`            | `/v1/jobs/{id}`                               | job with state and events; `?wait=<seconds>` long-polls until terminal                                                                                                                                                                                                                                                                             |
| `GET`            | `/v1/jobs/{id}/events`                        | server-sent events, closes on the terminal event                                                                                                                                                                                                                                                                                                   |
| `DELETE`         | `/v1/jobs/{id}`                               | cancel: immediate when queued, `Cancel` frame through the owner replica otherwise                                                                                                                                                                                                                                                                  |
| `PUT`            | `/v1/nodes/{node}/config/{target}`            | body is a `tar.zst` bundle, or a plain tar with `Content-Type: application/x-tar`; stores the blob and submits `ApplyConfig`; optional `?id=` and `?requires=`; `?digest=` with an empty body reuses a stored bundle                                                                                                                               |
| `POST`           | `/v1/nodes/{node}/config/{target}/validation` | same body; submits `ValidateConfig`                                                                                                                                                                                                                                                                                                                |
| `PUT`            | `/v1/nodes/{node}/engine-configuration`       | body is the JSON document of a structured engine configuration (version checked here: 1 or 2, and 409 when the engine of the node reports in its heartbeat that it reads an older one only; the rest by the engine); stored compressed (zstd) and submits `ApplyEngineConfiguration`; optional `?id=`, `?requires=`, `?target=` (default `engine`) |
| `GET`            | `/v1/nodes`, `/v1/nodes/{node}`               | nodes with connection status, applied configuration digests and engine state (of the engine component), and `components`: each one's session, declared targets and reported state                                                                                                                                                                  |
| `PUT` / `DELETE` | `/v1/nodes/{node}`                            | enrol (returns the token once) / remove a node, which revokes the enrolments that gave it its credential                                                                                                                                                                                                                                           |
| `GET`            | `/v1/enrolment-tokens`                        | enrolment tokens, without their hash, with `approval_required` and `last_used_at`                                                                                                                                                                                                                                                                  |
| `PUT` / `DELETE` | `/v1/enrolment-tokens/{id}`                   | record a token `{token_hash, node_id?, name?, expires_in_s, max_uses?, approval_required?}` (201, or 200 when the same one is there) / delete it                                                                                                                                                                                                   |
| `GET`            | `/v1/enrolments`, `/v1/enrolments/{id}`       | enrolments with state, node, reported name, address and version                                                                                                                                                                                                                                                                                    |
| `POST`           | `/v1/enrolments/{id}/assign`                  | `{node_id, name?}`: gives a pending enrolment its node, created with the credential of the agent                                                                                                                                                                                                                                                   |
| `POST`           | `/v1/enrolments/{id}/approve`                 | a pending enrolment may get its node (`approved`)                                                                                                                                                                                                                                                                                                  |
| `POST`           | `/v1/enrolments/{id}/reject`, `/complete`     | refuse a pending or approved enrolment (its agent is told) / mark an enrolled one completed                                                                                                                                                                                                                                                        |
| `POST`           | `/v1/nodes/{node}/rotate-credential`          | 202: the agent of the node rotates its credential at its next session, opened at once (`credential_rotated_at`, `rotation_requested` of the node)                                                                                                                                                                                                  |
| `GET`            | `/healthz`, `/readyz`, `/metrics`             | probes and Prometheus metrics                                                                                                                                                                                                                                                                                                                      |

JSON action shape: `{"run_commands": {"commands": [{"command": "...", "timeout_s": 30}]}}`,
`{"engine_commands": {"commands": ["..."]}}`,
`{"apply_config": {"target": "engine", "digest": "...", "size": 0}}`,
`{"apply_engine_configuration": {"digest": "..."}}` (a stored document),
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
- Node token: random 32 bytes, shown once, stored as SHA-256; or the
  credential the agent generated at enrolment, stored the same way and
  rotated by the agent ([Credential rotation](#credential-rotation)).
- Enrolment tokens: generated by the central; only their SHA-256 reaches the
  server; they expire and their uses are counted (see
  [Enrolment](#enrolment)).
- REST API: bearer token or basic auth, plus an allowed-CIDR list (same
  operational model as today).
- The agent runs unprivileged. Privileged operations are explicit entries of
  its local configuration (for instance `sudo systemctl reload centengine`),
  never text received from the central.
- Bundles: digest verified before any write, no absolute path, no `..`, no
  link or device entries, no write through a linked directory, file modes
  reduced to 0644/0755.
- Targets may share a directory: each one lists the files it wrote
  (`.gorgone-<target>.files`) and pruning a target never removes another
  target's files, nor enters a directory holding another target.
- The agent reaches the engine API through a Unix socket shared with it when
  they are separate containers: that API has no authentication, so it is not
  put on the network.
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
