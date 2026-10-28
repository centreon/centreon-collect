# CMA-0006 — Scheduler: spreading, slots, soft/hard state, timeperiods

- Status: Proposed
- Owner: @centreon/owners-cpp
- Scope: `agent/{inc,src}/scheduler.*`, `agent/inc/com/centreon/agent/check.hh` (`time_step`, start-expected logic), `agent/src/check.cc` (`calcul_status_confirmation`)

## Context

The agent replaces Engine's scheduling for the services it runs: Engine sends the service list, the agent decides when each check runs, and reports state, attempt and HARD/SOFT itself (CMA-0005). The algorithm is described with examples in `agent/doc/agent-doc.md` § Scheduler. Main facts from the code:

- On each `AgentConfiguration`, `update()` clears the queue and rebuilds every check. Running checks finish but their result is dropped (`check->get_conf() != _conf`).
- Checks are grouped by `min(check_interval, retry_interval)` and interleaved; first starts are spread by `first_inter_check_delay = min_interval / nb_checks`, starting 1 s after reception.
- Time is cut in slots (`time_step`, roughly half the inter-check delay, with a random part, reduced until every interval is a multiple within 5 s). The waiting queue is an `absl::btree_map<slot index, check>`: at most one check per slot; a busy slot pushes the check to the next one.
- A 100 ms-or-slot timer starts every due check while `_active_check < max_concurrent_checks`.
- Next start uses absolute time points (`time_step::increment_to_after_min`), not "end + interval": no drift.
- Two cadences per check, normal (`check_interval`) and retry (`retry_interval`); switching SOFT↔HARD jumps to the other cadence and waits one full step.
- Timeperiods: a check outside its `check_period_name` is not run and is re-queued at the next valid time; "no valid time in 366 days" defers it one year. A config update rebuilds the queue anyway.
- Force check (`ForceCheck`) moves a waiting check to immediate start; a running check is not forced.

State machine (`check::calcul_status_confirmation`, `max_attempts` floored at 1):

| Previous | New status | Result |
| --- | --- | --- |
| OK (HARD) | OK | unchanged, attempt 1 |
| OK SOFT recovery | OK | HARD, attempt 1 |
| non-OK HARD | OK | HARD recovery, attempt 1 |
| non-OK SOFT | OK | SOFT recovery, attempt + 1 |
| OK | non-OK | SOFT, attempt 1 (HARD at once if `max_attempts` = 1) |
| non-OK | non-OK | attempt + 1 while < `max_attempts`; HARD when attempt ≥ `max_attempts` |

## Decision

1. The scheduler MUST keep these properties; a change to any of them needs an ADR update and the unit test proving it:
   - no drift: next start computed from absolute time points;
   - spreading: first starts spread over the smallest interval;
   - one check per slot, slot collisions resolved by moving forward, never by dropping;
   - `max_concurrent_checks` never exceeded;
   - results of a previous configuration never stored nor re-queued.
2. Configuration handling MUST stay "full snapshot, rebuild all": no incremental diff of checks. Defaults for zero values (300 s / 60 s / 3) are applied in `update()`; the `check` constructor repeats 300 s / 60 s as a fallback for tests. A changed default MUST change both.
3. A check MUST be re-queued only by `_check_handler` (on completion) or by the timeperiod deferral, and removed from the queue before it starts. A check is never in the queue while running.
4. Changes to `calcul_status_confirmation` MUST keep Engine semantics (HARD/SOFT, attempts) and MUST add or update the `nagios_confirmation_*` / `first_check_*` cases of `agent/test/check_test.cc`; Engine trusts `status_confirmed` and `current_attempt` as sent.
5. Timeperiod evaluation MUST use `common::is_time_in_period_by_name` / `next_valid_time_in_period_by_name` from `common/` (same code as Engine). The agent MUST NOT reimplement timeperiod logic.
6. The host check is the `Service` with an empty `service_description`. Timezone diagnostics (`check_host_timezone`) run only for it and only log; they MUST NOT change scheduling.
7. Expensive work per due check (decrypt, parse) is done at build time in `update()`, not at each start.
8. Logging inside the scheduling loop MUST be `DEBUG` or `TRACE`. New log lines print the full command line only at `TRACE` (CMA-0009).

## Consequences

- Load is flat over time, predictable, and independent of check duration.
- A configuration push restarts every schedule: a poller reload causes one burst of new spreading, not a storm, thanks to the 1 s offset and spreading.
- Long checks delay later checks only when `max_concurrent_checks` is reached.

## Guardrails

- `std::this_thread::sleep`, relative `expires_after(interval)` for check scheduling, or "now + interval" computations replacing `time_step`.
- A new container for waiting checks, or a second path that starts a check without `_start_check` (counter bypass).
- A change in default intervals or attempts without a proto/Engine discussion.
- Edits to `calcul_status_confirmation` without new test cases in `check_test.cc`.

## References

- `agent/src/scheduler.cc`, `agent/inc/com/centreon/agent/scheduler.hh`
- `agent/inc/com/centreon/agent/check.hh`, `agent/src/check.cc`
- `agent/doc/agent-doc.md` § Scheduler
- `common/inc/com/centreon/common/check_timeperiod.hh`
- `agent/test/scheduler_test.cc`, `agent/test/check_test.cc`, `agent/test/check_timeperiod_test.cc`
