# CMA-0006: Schedule checks on absolute time slots, rebuilt from each configuration

- **Status:** Proposed
- **Date:** 2026-10-28

## Context

The agent schedules the services it runs, instead of Engine, and reports state, attempt and HARD/SOFT itself (CMA-0005). `agent/doc/agent-doc.md` § Scheduler describes the algorithm with worked examples. What the code does today:

- On each `AgentConfiguration`, `scheduler::update()` applies defaults (300 s, 60 s, 3 attempts), clears the queue and rebuilds every check. Running checks finish but their result is dropped (`check->get_conf() != _conf`).
- Checks are grouped by `min(check_interval, retry_interval)` and interleaved. First starts are spread by `min_interval / nb_checks`, from 1 s after reception.
- Time is cut into slots (`time_step`, about half the inter-check delay plus a random part, reduced until every interval fits within 5 s). The queue is an `absl::btree_map<slot index, check>`: one check per slot, a busy slot pushes the check to the next one.
- A timer starts every due check while `_active_check < max_concurrent_checks`.
- The next start is computed from absolute time points (`increment_to_after_min`), not "end + interval". Each check has a normal and a retry cadence; switching between SOFT and HARD jumps to the other cadence and waits one full step.
- A check outside its `check_period_name` is re-queued at the next valid time, computed by `common::next_valid_time_in_period_by_name`; with no valid time in 366 days it is deferred one year.
- `ForceCheck` starts a waiting check at once; a running check is not forced.
- `check::calcul_status_confirmation` (`max_attempts` floored at 1) sets the state: OK after HARD OK stays HARD, attempt 1; OK after a SOFT recovery becomes HARD, attempt 1; OK after a HARD problem is a HARD recovery, attempt 1; OK after a SOFT problem is a SOFT recovery, attempt + 1; a problem after OK is SOFT, attempt 1 (HARD at once if `max_attempts` is 1); a problem after a problem increments the attempt and becomes HARD when it reaches `max_attempts`.

Alternatives weighed:

- Re-arm each check "end + interval". It lost: intervals drift with check duration, and checks pile up after a slow period.
- Apply configuration changes as a diff of the check list. It lost: Engine sends a full snapshot; a full rebuild is simpler and the 1 s offset plus spreading avoids a burst.

## Decision

Keep the scheduler free of drift, spread first starts over the smallest interval, put one check per slot and resolve collisions by moving forward, never exceed `max_concurrent_checks`, and never store or re-queue a result from a previous configuration. Rebuild every check on each configuration, and apply zero-value defaults in `update()`.

Re-queue a check only on completion (`_check_handler`) or on a timeperiod deferral, and remove it from the queue before it starts. Evaluate timeperiods with the `common/` functions shared with Engine. Do per-check expensive work (decrypt, parse) when building checks, not when starting them.

Change `calcul_status_confirmation` only with new cases in `agent/test/check_test.cc` (`nagios_confirmation_*`, `first_check_*`): Engine trusts `status_confirmed` and `current_attempt` as sent.

## Consequences

- Load is flat over time and does not depend on check duration; a long check delays later ones only when `max_concurrent_checks` is reached.
- A poller reload restarts every schedule, spread again from scratch.
- The host check is the service with an empty `service_description`. Timezone diagnostics (`check_host_timezone`) run for it only and only log.
- The `check` constructor repeats the 300 s and 60 s defaults as a fallback for tests; a changed default changes both.
- Logs in the scheduling loop are DEBUG or TRACE (CMA-0011).

## When to Amend

- Engine starts sending configuration deltas, or one agent must hold checks for several hosts.
- A measured case shows the 5 s interval accuracy or the one-check-per-slot rule delaying checks beyond their interval.
