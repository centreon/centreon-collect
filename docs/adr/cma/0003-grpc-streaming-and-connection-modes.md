# CMA-0003 — gRPC bidirectional streaming and connection modes

- Status: Proposed
- Owner: @centreon/owners-cpp
- Scope: `agent/{inc,src}/bireactor.*`, `streaming_client.*`, `streaming_server.*`

## Context

The agent and Engine (module `engine/modules/opentelemetry`, folder `centreon_agent/`) exchange messages on one long-lived bidirectional gRPC stream, using the gRPC callback API. Two modes, chosen by `reversed_grpc_streaming` in the local configuration:

| Mode | Agent role | RPC | Engine side |
| --- | --- | --- | --- |
| direct (default) | client, `streaming_client` | `AgentService.Export(stream MessageFromAgent) returns (stream MessageToAgent)` | `agent_service.cc` |
| reversed | server, `streaming_server` | `ReversedAgentService.Import(stream MessageToAgent) returns (stream MessageFromAgent)` | `agent_reverse_client.cc`, `to_agent_connector.cc` |

Both reactors derive from one template, `bireactor<grpc reactor>`, explicitly instantiated twice in `bireactor.cc`. It owns a write queue (one `StartWrite` in flight), one pending read, and a static registry `_instances` that keeps each reactor alive until gRPC calls `OnDone`. `OnDone` releases the reactor through `asio::post`, because releasing it inside the gRPC callback can join the current thread and crash gRPC (comment in `bireactor.cc`).

## Decision

1. Both modes MUST stay functionally identical: same first message, same scheduler, same check builder. A feature added to one side is added to the other in the same PR.
2. The first message sent on every new stream MUST be `MessageFromAgent.init` (`AgentInfo`, built by `fill_agent_info`). Engine identifies the agent with it.
3. A scheduler is bound to a stream owner: one per `streaming_client` (kept across reconnections), one per accepted `server_reactor` (recreated on each connection). The scheduler starts with `scheduler::default_config()` (no check) and runs checks only after the first `AgentConfiguration` is received.
4. A reactor MUST be registered with `register_stream()` before `StartCall` / before being returned to gRPC, and MUST only be released by `OnDone` through `asio::post`. Deleting a reactor or resetting the last `shared_ptr` from a gRPC callback is forbidden.
5. Reactor state shared with gRPC threads (write queue, pending read, `_alive`) MUST be accessed under `_protect`. Members used only from the io thread (`server_reactor::_sched`, `_jwt_timer`) need no lock and MUST stay io-thread only. `on_incomming_request` MUST NOT call the scheduler directly: it posts to the `io_context` (CMA-0002).
6. On a read or write failure the reactor calls `on_error()` then `shutdown()`. The client recreates a reactor 10 s later with `common::defer`; the channel backoff is bounded by `second_max_reconnect_backoff`. The server waits for Engine to reconnect.
7. The server accepts one Engine stream at a time: a new `Import` shuts down the previous `server_reactor`. Tokens are checked in `Import` (`is_token_valid`) before any `server_reactor` or scheduler is created; a refusal returns an `ImmediateFinishReactor`; a JWT expiry closes the stream through `_jwt_timer`.
8. Writes MUST go through `bireactor::write()` (queue). Calling `StartWrite` elsewhere is forbidden.
9. Base classes `common::grpc::grpc_client_base` / `grpc_server_base` own channel and server creation, TLS and keepalive. The agent MUST NOT create its own `grpc::Channel` or `ServerBuilder` (exception: the CA bootstrap probe, CMA-0009).

## Consequences

- Only one write in flight: a large export delays the next one. The scheduler bounds message size (CMA-0005).
- `_instances` is never freed on purpose (threads end in an unknown order). Leak checkers report it; it is expected.
- `shutdown()` on the client calls `RemoveHold()` and `TryCancel()`; on the server it calls `Finish(CANCELLED)`. Any new terminal path MUST reuse `shutdown()`.

## Guardrails

- A new RPC or a unary call added to the streaming services instead of a new `oneof` member (CMA-0004).
- A reactor created without `register_stream`, or `delete this` / `_instances->erase` outside the posted `OnDone` lambda.
- Scheduler or check method called from `OnReadDone` without `asio::post`.
- A change in `streaming_client` without the equivalent in `streaming_server` (or the reverse), unless the PR explains why only one mode is concerned.
- Blocking wait on a gRPC completion.

## Known deviations

- Method name `on_incomming_request` is misspelled; it is part of the virtual interface. Keep the spelling until a dedicated rename PR.

## References

- `agent/inc/com/centreon/agent/bireactor.hh`, `agent/src/bireactor.cc`
- `agent/src/streaming_client.cc`, `agent/src/streaming_server.cc`
- `common/grpc/` (`grpc_client_base`, `grpc_server_base`, `grpc_config`)
- `engine/modules/opentelemetry/src/centreon_agent/`
