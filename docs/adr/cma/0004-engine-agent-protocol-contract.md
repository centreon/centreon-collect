# CMA-0004: Evolve `agent.proto` by addition only

- **Status:** Proposed
- **Date:** 2026-10-28

## Context

`agent/proto/agent.proto` (package `com.centreon.agent`, proto3) is compiled by the agent and by Engine. Agents and pollers are upgraded independently, so an agent can run against an older or a newer Engine for months.

The contract: the agent sends `MessageFromAgent` (`oneof`: `init` = `AgentInfo` as first message, `otel_request` = OTLP export); Engine sends `MessageToAgent` (`oneof`: `config` = `AgentConfiguration`, `otel_response`, `force_check` = `ForceCheck`). `CACertificateService.GetCACertificate` is a unary call used by the CA bootstrap. `AgentConfiguration` is a full snapshot (services, timeperiods, limits, key and salt), never a delta.

Compatibility already rests on conventions that are not written down: field 2 of `AgentConfiguration` (`check_interval`) was removed and its number left free; field 8 is a placeholder named `unused`; a zero value means "previous behaviour" (`check_interval` 0 → 300 s, `retry_interval` 0 → 60 s, `max_attempts` 0 → 3, empty `check_period_name` → always in period); `utc_offset` and `dst_active` are `optional` because absent and zero differ. Encrypted credentials are a capability: Engine sends `key` and `salt` only if the agent announced `AgentInfo.encryption_ready`, and only on a TLS stream (`agent_impl.cc`). `scheduler::on_engine_request` handles `config` and `force_check` and ignores anything else.

Alternatives weighed:

- A version number in `AgentInfo` with behaviour switched on it. It lost: every combination of versions would need code on both sides; capabilities announced field by field scale better.
- A new RPC per new kind of message. It lost: it needs a second stream or call, with its own lifecycle, for data that fits in the existing `oneof`.

## Decision

Add fields with new numbers only. Never renumber, retype or reuse a field number; declare a removed field `reserved`, number and name. Add a new kind of message as a new member of the existing `oneof`. Give every new field a zero value that means the previous behaviour, and make it `optional` when absent and zero must differ. Comment every new field with its unit and its default.

Ignore, on the agent side, any `MessageToAgent` content not understood. Announce in `AgentInfo` any capability Engine must know before sending something new, as `encryption_ready` does, and fill every new `AgentInfo` field on both platforms (`native_linux/src/agent_info.cc`, `native_windows/src/agent_info.cc`), following the OpenTelemetry semantic conventions where one exists (`os.type`, `os.name`, `host.arch`, `host.id`).

Change the Engine side in the same pull request, and add a robot case in `tests/broker-engine/cma*.robot` when the change alters behaviour.

## Consequences

- Old agents keep working with new pollers and the reverse, without negotiation beyond `AgentInfo`.
- The meaning of a field cannot change in place: a new meaning needs a new field.
- `timeperiod.proto` belongs to `common/engine_conf`; the agent only reads it.
- Not yet followed: field 2 is removed only in a comment and field 8 is a placeholder. The next change to `AgentConfiguration` replaces both with `reserved 2, 8;` (Engine does not set field 8).
- Not yet followed: most existing `Service` and `AgentInfo` fields have no unit or default comment. They get one when touched.

## When to Amend

- Support for agents older than a given version is dropped, which would allow removing fields.
- A change cannot be expressed by addition and requires a breaking protocol version.
