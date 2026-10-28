# CMA-0004 — Engine ↔ agent protocol contract

- Status: Proposed
- Owner: @centreon/owners-cpp
- Scope: `agent/proto/agent.proto`, `agent/native_*/src/agent_info.cc`, `agent/src/scheduler.cc` (`on_engine_request`, `update`, `force_check`)

## Context

`agent.proto` (package `com.centreon.agent`, proto3) is compiled by both the agent and Engine. Agents and pollers are upgraded independently: a customer can run an agent older or newer than its Engine for months. The file already carries compatibility scars: field 2 of `AgentConfiguration` (`check_interval`) removed and its number kept free, field 8 `unused`.

Messages:

| Direction | Message | Members |
| --- | --- | --- |
| agent → Engine | `MessageFromAgent` (`oneof content`) | `init` = `AgentInfo` (first message), `otel_request` = OTLP `ExportMetricsServiceRequest` |
| Engine → agent | `MessageToAgent` (`oneof content`) | `config` = `AgentConfiguration`, `otel_response`, `force_check` = `ForceCheck` |
| CA bootstrap | `CACertificateService.GetCACertificate` | unary, see CMA-0009 |

`AgentConfiguration` is a full snapshot (services, timeperiods, limits, key/salt), never a delta. On reception the scheduler rebuilds every check (CMA-0006).

## Decision

1. `agent.proto` changes MUST be backward and forward compatible:
   - new fields only, with new numbers; never renumber, retype or reuse a number; a removed field becomes `reserved` (number and name);
   - a new message kind is a new member of the existing `oneof`, never a new RPC on the streaming services;
   - a new field's zero value MUST mean "previous behaviour" (examples: `check_interval == 0` → 300 s, `retry_interval == 0` → 60 s, `max_attempts == 0` → 3, empty `check_period_name` → always in period, `has_utc_offset()` false → no timezone check).
2. Presence-sensitive scalars MUST be `optional` (as `utc_offset`, `dst_active`) when "absent" and "zero" differ.
3. The agent MUST ignore a `MessageToAgent` it does not understand (unknown `oneof` member or none set): `scheduler::on_engine_request` handles `config` and `force_check` only, and that stays the rule.
4. A capability that Engine must know before sending something MUST be announced in `AgentInfo` (pattern: `encryption_ready`). Engine MUST NOT send the new content to an agent that did not announce it.
5. `AgentInfo` fields follow OpenTelemetry semantic conventions when one exists (`os_type` = `os.type`, `os_name` = `os.name`, `arch` = `host.arch`, `machine_id` = `host.id`). Both `native_linux/src/agent_info.cc` and `native_windows/src/agent_info.cc` MUST fill every new field.
6. A proto change MUST land in the same PR on both sides (agent and `engine/modules/opentelemetry/src/centreon_agent/`) with a robot test in `tests/broker-engine/cma*.robot` covering old-peer behaviour when the change alters behaviour.
7. Every new field MUST carry a comment saying its unit (seconds, megabytes) and its default when zero. Existing `Service` and `AgentInfo` fields mostly lack it; add it when touching them.
8. `timeperiod.proto` is owned by `common/engine_conf`; the agent consumes it read-only.

## Consequences

- Old agents keep working with new pollers and the reverse, without negotiation beyond `AgentInfo`.
- Field semantics cannot be changed in place: a changed meaning needs a new field.

## Guardrails

- Diff of `agent.proto` that changes a field number, type or name of an existing field, or deletes a field without `reserved`.
- A new `rpc` in `AgentService` or `ReversedAgentService`.
- A new required-by-convention field whose zero value breaks old Engines or old agents.
- `AgentInfo` field filled on one platform only.
- Proto change without the Engine counterpart or without a test.

## Known deviations

- `AgentConfiguration` field 2 is documented as removed in a comment but not declared `reserved`, and field 8 is a placeholder `unused`. Next proto PR: replace both with `reserved 2, 8;` after checking Engine does not set field 8.

## References

- `agent/proto/agent.proto`, `common/engine_conf/timeperiod.proto`
- `agent/native_linux/src/agent_info.cc`, `agent/native_windows/src/agent_info.cc`
- `engine/modules/opentelemetry/src/centreon_agent/agent_impl.cc` (`_calc_and_send_config_if_needed`)
- `tests/broker-engine/cma.robot`, `tests/broker-engine/cma2.robot`
