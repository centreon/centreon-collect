# BAM — Business Activity Monitoring

BAM is a Broker module that aggregates monitoring states from raw services and hosts into high-level *Business Activities* (BAs). It translates low-level alerts into business-impact indicators visible to non-technical stakeholders.

---

## Core concepts

### Business Activity (BA)

A BA represents a business service (e.g. "Online Payment", "VPN"). It has:

- a **level** (0–100 for impact mode, or a ratio for other modes)
- a **state**: OK, WARNING, CRITICAL, or UNKNOWN
- **warning** and **critical** thresholds that determine when the state changes
- an optional **downtime behaviour**

Each BA is surfaced to the monitoring engine as a virtual host/service pair, so it can be acknowledged, scheduled for downtime, and graphed like any other service.

#### BA computation modes

There are five computation strategies, chosen at configuration time via `state_source`:

| Mode | Class | Behaviour |
|------|-------|-----------|
| `impact` | `ba_impact` | Level starts at 100. Each non-OK KPI subtracts its configured impact points. State is OK/WARNING/CRITICAL based on thresholds. |
| `best` | `ba_best` | State equals the best (least severe) state among all KPIs. |
| `worst` | `ba_worst` | State equals the worst (most severe) state among all KPIs. |
| `ratio_percent` | `ba_ratio_percent` | State is WARNING/CRITICAL when the percentage of critical KPIs exceeds the configured threshold. |
| `ratio_number` | `ba_ratio_number` | Same as ratio_percent but based on a raw count instead of a percentage. |

##### Impact BA details

`_level_hard` and `_level_soft` start at 100. When a KPI is not OK, its configured *nominal impact* is subtracted. Re-adding impacts every 100 operations (`_recompute_limit`) prevents floating-point drift.

```
state_ok       if _level_hard  > _level_warning
state_warning  if _level_hard <= _level_warning
state_critical if _level_hard <= _level_critical
```

Perfdata: `BA_Level=<value>;<warn>;<crit>;0;100`

##### Ratio percent details

`_level_hard` counts how many KPIs are in CRITICAL state. State is:

```
state_critical if (_level_hard / total_kpis * 100) >= _level_critical
state_warning  if (_level_hard / total_kpis * 100) >= _level_warning
```

#### Downtime behaviour

| Value | Behaviour |
|-------|-----------|
| `dt_ignore` (0) | Downtime on KPIs has no effect on the BA computation. |
| `dt_inherit` | If all non-OK KPIs are in downtime, the BA inherits a virtual downtime. |
| `dt_ignore_kpi` | KPIs currently in downtime are excluded from the impact calculation. |

---

### Key Performance Indicator (KPI)

A KPI is a leaf or intermediate node in the BA computation tree. There are four KPI types (field `kpi_type` in `mod_bam_kpi`):

| Type | Value | Class | Description |
|------|-------|-------|-------------|
| Service | `0` | `kpi_service` | A host/service check result. |
| Meta-service | `1` | — | Aggregated meta-service value. |
| BA | `2` | `kpi_ba` | Another BA used as a KPI (enables hierarchical BAs). |
| Boolean expression | `3` | `kpi_boolexp` | A boolean rule that evaluates to OK or CRITICAL. |

Every KPI has separate **hard** and **soft** impact values and an optional **downtime** and **acknowledgement** impact. The `impact_values` struct carries:

- `nominal`: the points removed from the parent BA level
- `acknowledgement`: additional points removed when the problem is acknowledged
- `downtime`: additional points removed when in downtime
- `state`: the underlying state (for ratio/best/worst modes)

For service KPIs, separate impact values are configured for WARNING, CRITICAL, and UNKNOWN states (`drop_warning`, `drop_critical`, `drop_unknown`).

---

### Boolean expressions

A boolean expression (`bool_expression`) is a tree of boolean operations evaluated against service states. It exposes a single OK/CRITICAL state to its parent `kpi_boolexp`.

#### Expression tree nodes

All nodes implement `bool_value`:

| Class | Description |
|-------|-------------|
| `bool_service` | Leaf: evaluates the state of a specific host/service. |
| `bool_constant` | Leaf: always true or always false. |
| `bool_and` | Logical AND of two sub-expressions. |
| `bool_or` | Logical OR of two sub-expressions. |
| `bool_not` | Logical NOT. |
| `bool_xor` | Logical XOR. |
| `bool_equal` | Numeric equality between two sub-expressions. |
| `bool_less_than` | Numeric less-than. |
| `bool_more_than` | Numeric greater-than. |
| `bool_not_equal` | Numeric inequality. |

The expression text is stored in `mod_bam_boolean.expression` and parsed at startup by `exp_parser` / `exp_tokenizer` / `exp_builder`.

#### Impact direction

The field `bool_state` (also called `impact_if`) controls which boolean result triggers the impact:

- `impact_if = true` → the KPI fires (is CRITICAL) when the expression evaluates to **true**
- `impact_if = false` → the KPI fires when the expression evaluates to **false**

---

### Computable tree

The entire BA/KPI/boolean graph implements the `computable` interface:

```
computable
├── ba  (abstract base for all BA types)
│   ├── ba_impact
│   ├── ba_best
│   ├── ba_worst
│   ├── ba_ratio_percent
│   └── ba_ratio_number
├── kpi  (abstract base for all KPI types)
│   ├── kpi_service
│   ├── kpi_ba
│   └── kpi_boolexp
└── bool_expression
    └── bool_value subtree (bool_and, bool_or, bool_service, …)
```

When a leaf value changes (e.g. a service check result arrives), the change propagates upward via:

1. `update_from(child, visitor)` — recomputes this node given the child that changed.
2. `notify_parents_of_change(visitor)` — if this node's state changed, calls `update_from` on every parent.

The `visitor` (an `io::stream*`) collects the events produced during the propagation (BA status, KPI status, BA events).

---

## `service_book`

`service_book` is a dispatch table that routes incoming NEB events to every KPI that depends on a given host/service pair. It is owned by `configuration::applier::state` and shared by all appliers.

### Internal structure

The book holds a `std::unordered_map` keyed on `(host_id, service_id)`. Each entry contains:

- a list of `service_listener*` — the `kpi_service` and `bool_service` objects that care about this service
- a `service_state` snapshot — the last known state (current state, last hard state, state type, last check, acknowledged flag)

### Registration

When a `kpi_service` or a `bool_service` is created by its applier, it calls `service_book::listen(host_id, service_id, this)`. The reverse call `unlisten()` removes the registration on teardown. Multiple KPIs can listen to the same service simultaneously.

### Event dispatch

`monitoring_stream` forwards every relevant NEB event to the book via one of the overloaded `update()` methods:

| Event type | State field updated |
|------------|---------------------|
| `service_status` / `pb_service` / `pb_service_status` | `current_state`, `last_hard_state`, `state_type`, `last_check` |
| `pb_adaptive_service_status` | forwarded as-is (scheduled downtime flag, etc.) |
| `acknowledgement` / `pb_acknowledgement` | `acknowledged` |
| `downtime` / `pb_downtime` | forwarded as-is (no local state field) |

Each `update()` call looks up the `(host_id, service_id)` key, updates the cached `service_state` when applicable, and then calls `service_listener::service_update()` on every registered listener so the KPI can recompute its impact and propagate the change upward through the BA tree.

### Persistence

On clean shutdown, `save_to_cache()` serializes all `service_state` snapshots into the `persistent_cache` as a `ServicesBookState` protobuf message. On the next startup, `apply_services_state()` restores those snapshots and immediately re-notifies every listener, so KPIs start with a consistent state before the first live check arrives.

---

## `monitoring_stream`

`monitoring_stream` is the real-time computation engine. It is an `io::stream` that:

1. **Receives** NEB events from the broker multiplexer:
   - `service_status` / `pb_service_status` / `pb_service` → feeds `kpi_service`
   - `acknowledgement` / `pb_acknowledgement` → updates acknowledged state
   - `downtime` / `pb_downtime` → propagates downtime to BAs and KPIs

   It also **produces** a `rebuild` event: on `update()`, `_rebuild()` queries `mod_bam` for BAs flagged `must_be_rebuild='1'` and publishes a `rebuild` event listing their IDs. `reporting_stream` is the consumer — its `_process_rebuild()` uses it to trigger an availability rebuild.

2. **Maintains** the computable tree in memory via `configuration::applier::state`, which is populated from the `centreon` database at startup and on `update()`.

3. **Writes BA/KPI statuses** to the `centreon_storage` database:
   - `mod_bam`: current BA levels and states
   - `mod_bam_kpi`: current KPI states and last impact

4. **Sends external commands** to the monitoring engine via the Engine command pipe (`_ext_cmd_file`):
   - **Forced service checks** — because each BA maps to a virtual service, the engine must be told to re-check that service when the BA state changes. Checks are deduplicated and batched over a 5-second window to avoid flooding the engine with redundant checks when a BA tree re-evaluates multiple times in rapid succession.
   - **Downtimes** — when a BA inherits a downtime from its KPIs, a corresponding downtime command is sent to the virtual service.

5. **Persists a cache** of inherited downtimes across restarts via `persistent_cache`.

### Startup sequence

```
monitoring_stream()
  └── _prepare()          -- prepare SQL statements
  └── update()
        ├── _applier.apply()  -- load configuration from DB, build computable tree
        ├── _rebuild()        -- publish a rebuild event for BAs flagged must_be_rebuild
        ├── initialize()      -- publish initial states of all BAs/KPIs
        └── _read_cache()     -- restore inherited downtimes from persistent cache
```

### Initial event restoration (`set_initial_event`)

On restart, BA and KPI objects must be seeded with the event that was still open when broker last stopped, so the reporting timeline is continuous and no gap appears between sessions.

The flow has three stages:

**1. Database read (`reader_v2`)** — executed during `monitoring_stream::update()`.

- For each BA row in `mod_bam`, `reader_v2` reads `last_state_change` (col 5), `current_status` (col 6), and `in_downtime` (col 7). If `last_state_change` is not `NULL`, it builds a `pb_ba_event` with no `end_time` and stores it in the configuration object via `configuration::ba::set_opened_event()`.
- For each KPI row in `mod_bam_kpi`, `reader_v2` reads `last_state_change` (col 16), `in_downtime` (col 17), and `last_impact` (col 18). If `last_state_change` is not `NULL`, it builds a `KpiEvent` and stores it via `configuration::kpi::set_opened_event()`.

**2. Object construction (configuration appliers)** — when the computable tree is built from the loaded configuration.

- `applier::ba::_new_ba()` checks `cfg.get_opened_event().obj().ba_id()`: if non-zero, it calls `ba::set_initial_event()`, which stores the event as `_event` (the currently open period) and appends it to `_initial_events`.
- `applier::kpi::_resolve_kpi()` checks `cfg.get_opened_event().kpi_id()`: if non-zero, it calls `kpi::set_initial_event()`. This variant also reconciles the impact: if the stored `impact_level` differs from the value computed by the current configuration (the KPI's `impact_hard()` output), the old event is closed at `now` and a fresh event is opened — both are pushed to `_initial_events` — so that stale impact values do not carry over into the new session.

**3. Flushing to the broker bus (`initialize()`)** — the last step of the startup sequence.

```
monitoring_stream::initialize()
  └── event_cache_visitor ev_cache
  └── _applier.visit(&ev_cache)
        ├── ba_applier.visit()   → ba::visit()  → ba::_commit_initial_events(ev_cache)
        └── kpi_applier.visit()  → kpi::visit() → kpi::commit_initial_events(ev_cache)
  └── ev_cache.commit_to(publisher)
        └── reporting_stream receives events
              ├── writes open BA events  → mod_bam_reporting_ba_events
              └── writes open KPI events → mod_bam_reporting_kpi_events
```

`_commit_initial_events()` / `commit_initial_events()` write every entry in `_initial_events` into the visitor, then clear the vector. `ev_cache.commit_to(publisher)` forwards all collected events to the multiplexing publisher; `reporting_stream` receives them and persists them, closing the gap left by the restart.

---

## `reporting_stream`

`reporting_stream` is the historical recording engine (stream name: `BAM-BI`). It persists the timeline of BA and KPI state changes for reporting and SLA computation.

### Events processed

| Event type | Method | Target table |
|------------|--------|--------------|
| `ba_event` | `_process_ba_event` | `mod_bam_reporting_ba_events` |
| `pb_ba_event` | `_process_pb_ba_event` | `mod_bam_reporting_ba_events` |
| `pb_ba_duration_event` | `_process_pb_ba_duration_event` | `mod_bam_reporting_ba_events_durations` |
| `kpi_event` | `_process_kpi_event` | `mod_bam_reporting_kpi_events` |
| `pb_kpi_event` | `_process_pb_kpi_event` | `mod_bam_reporting_kpi_events` |
| Dimension events (legacy) | `_process_dimension` | `mod_bam_reporting_ba`, `_bv`, `_kpi`, `_timeperiods`, … |
| Dimension events (`pb_dimension_*`) | `_process_pb_dimension` | `mod_bam_reporting_ba`, `_bv`, `_kpi`, `_timeperiods`, … |
| `rebuild` | `_process_rebuild` | triggers availability rebuild |

Dimension events carry the configuration snapshot (BA names, BV memberships, KPI definitions, time periods). They are received at startup or after a configuration change and used to populate the `mod_bam_reporting_*` dimension tables.

### BA event lifecycle

A BA event records a continuous period during which a BA was in a given state:

```sql
-- mod_bam_reporting_ba_events
ba_event_id   -- auto-increment PK
ba_id         -- which BA
start_time    -- epoch when the state started
end_time      -- epoch when the state ended (NULL = still open)
status        -- 0=OK, 1=WARNING, 2=CRITICAL, 3=UNKNOWN
in_downtime   -- was the BA in downtime during this event?
first_level   -- BA level at the start of the event
```

On startup, the stream closes any events that have no `end_time` (inconsistent events from a previous crash), then sets `end_time` to now on all remaining open events (`_close_all_events`). New events are opened as statuses arrive.

### KPI event lifecycle

Similar to BA events but per KPI:

```sql
-- mod_bam_reporting_kpi_events
kpi_event_id
kpi_id
start_time
end_time
status
in_downtime
impact_level
first_output
first_perfdata
```

### Startup sequence

```
reporting_stream()
  └── _prepare()                   -- prepare SQL statements
  └── _load_timeperiods()          -- load timeperiod definitions from DB
  └── _load_kpi_ba_events()        -- load open events into memory cache
  └── _close_inconsistent_events() -- close events open in only one table
  └── _close_all_events()          -- close remaining open events
  └── availability_thread.start()  -- start the availability computation thread
```

---

## Availability computation

Availability is computed by `availability_thread`, a background thread owned by `reporting_stream`.

### Trigger

The thread wakes up once per day, just after midnight, and also on-demand when a `rebuild` event is received naming specific BAs.

### Algorithm (`availability_builder`)

For each BA × time period combination:

1. Query `mod_bam_reporting_ba_events` for all events that overlap the target day.
2. For each event, compute the intersection of its `[start_time, end_time]` interval with the time period (via `timeperiod::duration_intersect`).
3. Accumulate the intersection duration into the appropriate counter.

```
available    -- seconds BA was OK, within the time period
degraded     -- seconds BA was WARNING
unavailable  -- seconds BA was CRITICAL
unknown      -- seconds BA was UNKNOWN
downtime     -- seconds BA was in downtime (regardless of state)
```

Alert counters record how many distinct events *opened* during the day:

```
alert_unavailable_opened
alert_degraded_opened
alert_unknown_opened
nb_downtime
```

### Output table

```sql
-- mod_bam_reporting_ba_availabilities
ba_id
time_id              -- epoch of day start (midnight)
timeperiod_id
available            -- seconds
unavailable          -- seconds
degraded             -- seconds
unknown              -- seconds
downtime             -- seconds
alert_unavailable_opened
alert_degraded_opened
alert_unknown_opened
nb_downtime
timeperiod_is_default
```

Each BA can have multiple time periods; the default time period is flagged with `timeperiod_is_default = true`.

---

## Database schema overview

### Configuration tables (read by `monitoring_stream`)

| Table | Purpose |
|-------|---------|
| `mod_bam` | BA definitions (id, name, type, thresholds, downtime behaviour) |
| `mod_bam_kpi` | KPI definitions (type, host/service/ba/bool references, impact values) |
| `mod_bam_boolean` | Boolean expression definitions (name, expression text, `bool_state`) |
| `mod_bam_impacts` | Named impact values referenced by KPIs |
| `mod_bam_ba_groups` / `mod_bam_bagroup_ba_relation` | Business Views (BV) and BA memberships |
| `mod_bam_relations_ba_timeperiods` | Time period assignments per BA |

### Reporting tables (written by `reporting_stream`)

| Table | Purpose |
|-------|---------|
| `mod_bam_reporting_ba` | BA dimension (name, description) |
| `mod_bam_reporting_bv` | BV dimension |
| `mod_bam_reporting_kpi` | KPI dimension |
| `mod_bam_reporting_ba_events` | BA state timeline |
| `mod_bam_reporting_kpi_events` | KPI state timeline |
| `mod_bam_reporting_ba_events_durations` | Derived durations per BA event × time period |
| `mod_bam_reporting_ba_availabilities` | Daily availability per BA × time period |
| `mod_bam_reporting_timeperiods` | Time period definitions |
| `mod_bam_reporting_relations_ba_bv` | BA ↔ BV memberships |
| `mod_bam_reporting_relations_ba_kpi_events` | Links BA events to KPI events |
| `mod_bam_reporting_relations_ba_timeperiods` | BA ↔ time period assignments |

---

## Data flow summary

```
Engine checks
     │
     ▼ NEB events (service_status, downtime, acknowledgement)
monitoring_stream
     │
     ▼ update(event, visitor)
service_book  (dispatch table: (host_id, service_id) → [kpi_service, bool_service, …])
     │
     ▼ service_listener::service_update()
     ├── updates computable tree (kpi_service → ba)
     │         propagates via update_from() / notify_parents_of_change()
     │
     ├── writes BA/KPI current statuses → centreon_storage (mod_bam, mod_bam_kpi)
     │
     ├── sends forced service check commands → Engine named pipe
     │         (batched, deduplicated, 5 s window)
     │
     └── publishes BAM events to broker bus
              │
              ▼ BAM events (ba_event, kpi_event, dimension events)
         reporting_stream
              │
              ├── writes event timeline → mod_bam_reporting_ba_events
              │                         → mod_bam_reporting_kpi_events
              │
              ├── writes dimension data → mod_bam_reporting_ba, _bv, _kpi, …
              │
              └── availability_thread (wakes at midnight)
                       └── reads ba_events, intersects with timeperiods
                       └── writes → mod_bam_reporting_ba_availabilities
```
