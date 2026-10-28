# CMA-0003: Talk to Engine over one bidirectional gRPC stream, in direct or reversed mode

- **Status:** Proposed
- **Date:** 2026-10-28

## Context

The agent and Engine (`engine/modules/opentelemetry/src/centreon_agent/`) exchange every message on one long-lived bidirectional stream, using the gRPC callback API. The local option `reversed_grpc_streaming` picks the side that connects:

- direct, the default: the agent is the client (`streaming_client`) and calls `AgentService.Export`;
- reversed: the agent is the server (`streaming_server`) and Engine calls `ReversedAgentService.Import`, when the connection has to be opened from the poller side.

Both reactors derive from one template, `bireactor<grpc reactor>`, instantiated twice in `bireactor.cc`. It owns a write queue with one `StartWrite` in flight, one pending read, and a static registry `_instances` that keeps a reactor alive until gRPC calls `OnDone`. `OnDone` releases the reactor through `asio::post`, because releasing it inside the gRPC callback can join the current thread and crash gRPC.

The client keeps one scheduler across reconnections and recreates a reactor 10 s after an error (`common::defer`); the channel backoff is bounded by `second_max_reconnect_backoff`. The server checks the token in `Import` (`is_token_valid`), answers a refusal with an `ImmediateFinishReactor`, accepts one Engine stream at a time (a new one shuts the previous `server_reactor` down) and closes the stream when the JWT expires (`_jwt_timer`).

Alternatives weighed:

- Unary RPCs (one call per configuration, one per result batch). They lost: Engine must push configurations and force checks at any time, which needs a channel open from Engine to the agent.
- Two separate code paths for the two modes. They lost: the modes differ only by who connects; one template keeps queueing, release and error handling identical.

## Decision

Keep both modes functionally identical: same first message, same scheduler, same check builder. Add a feature to both sides in the same change.

Send `MessageFromAgent.init` (`AgentInfo`, from `fill_agent_info`) as the first message of every stream. Start each scheduler with `scheduler::default_config()` and run checks only once the first `AgentConfiguration` arrives.

Register every reactor with `register_stream()` before `StartCall` or before handing it to gRPC, and release it only in the posted `OnDone` lambda. Write through `bireactor::write()` only. Access the state shared with gRPC threads (write queue, pending read, `_alive`) under `_protect`, and post incoming requests to the `io_context` instead of calling the scheduler. Leave channel and server creation, TLS and keepalive to `common::grpc::grpc_client_base` and `grpc_server_base`. End a stream through `shutdown()`.

## Consequences

- With one write in flight, a large export delays the next one; the scheduler bounds message size (CMA-0005).
- `_instances` is never freed, on purpose, because threads end in an unknown order. Leak checkers report it.
- `server_reactor::_sched` and `_jwt_timer` are used from the `io_context` thread only, so they need no lock.
- A new message kind is a new member of the existing `oneof` (CMA-0004), never a new RPC on these services.
- The only channel created outside the base classes is the CA bootstrap probe (CMA-0009).
- The virtual method `on_incomming_request` is misspelled. It is renamed in a dedicated change, never inside a feature.

## When to Amend

- Engine needs several concurrent streams to one agent, or one agent serves several Engines.
- A gRPC release fixes the `OnDone` deadlock, so the posted release becomes unnecessary.
