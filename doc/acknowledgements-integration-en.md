# Acknowledgements — Engine ↔ Broker integration

<!-- TOC -->
* [Acknowledgements — Engine ↔ Broker integration](#acknowledgements--engine--broker-integration)
  * [What is an acknowledgement?](#what-is-an-acknowledgement)
  * [Overview](#overview)
  * [Where the acknowledgement state lives](#where-the-acknowledgement-state-lives)
  * [The acknowledgement state (Engine side)](#the-acknowledgement-state-engine-side)
  * [Creation](#creation)
  * [Closure (the only "update")](#closure-the-only-update)
    * [Recovery and non-sticky state change](#recovery-and-non-sticky-state-change)
    * [Explicit removal](#explicit-removal)
  * [Broker side: the cache](#broker-side-the-cache)
    * [Storing an acknowledgement](#storing-an-acknowledgement)
    * [Closing an acknowledgement](#closing-an-acknowledgement)
  * [BBDO2 vs BBDO3](#bbdo2-vs-bbdo3)
  * [Persistence](#persistence)
  * [The acknowledgement comment](#the-acknowledgement-comment)
  * [Broker side (unified_sql)](#broker-side-unified_sql)
  * [gRPC: GetAcknowledgements](#grpc-getacknowledgements)
  * [Relation to poller HA](#relation-to-poller-ha)
  * [Before / after](#before--after)
<!-- TOC -->

---

## What is an acknowledgement?

When a host or service goes into a problem state (DOWN, UNREACHABLE, WARNING, CRITICAL…), Centreon
notifies the relevant contacts and **repeats** those notifications at a regular interval for as long
as the problem lasts. An **acknowledgement** is the action by which an operator says *"I've seen this
problem, I'm taking care of it"*: it **silences the reminder notifications** for that resource
without changing anything about its monitoring — the resource stays in its problem state, it is
merely flagged as "handled". It usually carries a comment (author, a note) that remains visible in
the UI.

An acknowledgement therefore always applies to **the state of a host/service in problem**, never to a
specific notification. You can only acknowledge something that is actually in a problem state, and
the acknowledgement **goes away on its own** once the resource recovers (OK/UP). There are two
flavours:

- **normal**: the acknowledgement drops as soon as the state changes;
- **sticky**: it persists until the resource has fully recovered, even if it moves from one problem
  state to another (e.g. CRITICAL → WARNING).

The rest of this document describes how that state — **decided** by Engine — is now **stored, closed
and persisted** by Broker.

---

## Overview

Acknowledgement *tracking* used to live in **cbmod** (the NEB module linked into `centengine`):
a per-resource map remembered the open acknowledgement so that, when the resource recovered, cbmod
could emit the closing event. That tracking has been moved into Broker's **global cache**
(`broker_cache`, in `cbd`). Engine still **decides** acknowledgements (it owns the notifier state and
mints the events); Broker now **stores, closes and persists** them.

This matters for poller HA: the acknowledgement state is now held at the **center** and is
**durable** across a `cbd` restart, instead of living only in the (fungible) poller process. See
[Relation to poller HA](#relation-to-poller-ha).

> **Note.** The acknowledgement *effect* (suppressing notifications) is still applied by Engine's
> notifier; only the *bookkeeping* needed to close the `acknowledgements` table row moved to Broker.
> Unlike downtimes, there is no shared `common/acknowledgements` library — the cache directly tracks
> the events that flow through the multiplexer.

---

## Where the acknowledgement state lives

There are three layers; only the first two hold live state.

| Layer | What it holds | Survives… |
|---|---|---|
| **Engine** (`centengine`, notifier) | `_acknowledgement_type`, `_acknowledgement_comment_id`, `_last_acknowledgement` — drives notification suppression | an Engine restart (via Engine **retention**) |
| **Broker** (`cbd`, `broker_cache`) | the open acknowledgements map `_acknowledgements`, keyed `(host_id, service_id)` — drives the closing event | a `cbd` restart (via the persisted **cache file**, see [Persistence](#persistence)) |
| **Database** | `acknowledgements` table (history) + `acknowledged` / `acknowledgement_type` flags on `hosts`/`services`/`resources` + the ack `comment` | always |

The source of truth for "is this resource acknowledged" is the **Engine notifier**; the cache and DB
are downstream copies. The cache map only contains **open** acknowledgements.

---

## The acknowledgement state (Engine side)

`engine/inc/com/centreon/engine/notifier.hh`, `engine/src/notifier.cc`

| Field | Type | Meaning |
|---|---|---|
| `_acknowledgement_type` | `AckType { NONE = 0, NORMAL, STICKY }` (`bbdo/neb.proto`) | Current acknowledgement (NONE = not acknowledged) |
| `_acknowledgement_comment_id` | `uint64_t` | Id of the associated comment (non-persistent only; see [The acknowledgement comment](#the-acknowledgement-comment)) |
| `_last_acknowledgement` | `time_t` | When it was set |

`set_acknowledgement(AckType)` (`notifier.cc:1125`) only mutates the in-memory field; it emits no
event by itself. These three fields are written to and restored from Engine retention
(`retention/applier/host.cc`, `retention/applier/service.cc`), so the acknowledgement survives an
**Engine** restart.

---

## Creation

`ACKNOWLEDGE_HOST_PROBLEM` / `ACKNOWLEDGE_SVC_PROBLEM` (external command, `commands.cc`) or the gRPC
acknowledge handlers set the notifier and emit an acknowledgement event:

```
acknowledge_*_problem  (commands.cc / gRPC)
   ├─ notifier::set_acknowledgement(NORMAL | STICKY)
   ├─ create the acknowledgement comment   (entry_type = acknowledgment)
   └─ broker_acknowledgement_data(...)      (engine/src/broker.cc)
        └─ forward_pb_acknowledgement  →  pb_acknowledgement   (BBDO3)
           forward_acknowledgement     →  neb::acknowledgement (BBDO2)
                                         ─▶ Broker
```

The emitted event carries `host_id`/`service_id` (`service_id == 0` ⇒ host ack), `sticky`, `state`
(the state at acknowledgement time), `author`, `comment_data`, `entry_time`, and `deletion_time`
left at `0` (an **open** acknowledgement).

---

## Closure (the only "update")

An acknowledgement is immutable; the only transition is **closure** (stamping `deletion_time`).
Closure is **not** emitted directly — it is *derived from the resource status* by Broker. There are
two situations.

### Recovery and non-sticky state change

When Engine clears the acknowledgement internally (the resource recovered, or a non-sticky ack moved
to a different state), the next status event carries `acknowledgement_type == NONE`. Broker's cache
detects the transition (a cached ack + a status saying NONE) in
`broker_cache::_take_expired_acknowledgement()` and:

* **erases** the entry from the cache map, and
* publishes a **closing** acknowledgement (stamping `deletion_time`) **unless** the resource simply
  recovered to OK or a non-sticky ack moved to a different state — exactly the legacy condition
  `!(!state || (!sticky && state != ack_state))`.

So a plain recovery to OK clears the cache entry without writing a `deletion_time` (a sticky ack
remains open in the DB until explicitly removed), preserving the historical behaviour.

### Explicit removal

`REMOVE_HOST_ACKNOWLEDGEMENT` / `REMOVE_SVC_ACKNOWLEDGEMENT` →
`remove_host_acknowledgement` / `remove_service_acknowledgement` (`engine/src/commands/commands.cc`):

```cpp
svc->set_acknowledgement(AckType::NONE);            // clear the notifier
svc->update_status(host::STATUS_ACKNOWLEDGEMENT);   // emit a status carrying ack_type = NONE
svc->delete_acknowledgement_comment();              // delete the comment (separate)
```

There is **no** dedicated acknowledgement-deletion event: the closure of the `acknowledgements` row
relies on the same status-detection path as above. Because the service is still in a non-OK state at
removal time, the condition closes the row (`deletion_time` is stamped).

> This is why routing **legacy status** events into the cache matters in BBDO2 — see
> [BBDO2 vs BBDO3](#bbdo2-vs-bbdo3). Before that routing existed, an explicit removal in BBDO2 left
> the `acknowledgements` row open.

---

## Broker side: the cache

`broker/core/cache/broker_cache.cc`

Acknowledgement events and resource-status events both flow through `broker_cache::_publish()`.

### Storing an acknowledgement

`update_acknowledgement()` is gated on the section matching the resource — `CACHE_HOSTS` for a host
ack (`service_id == 0`), `CACHE_SERVICES` for a service ack (both are enabled by a `unified_sql`
output). It distinguishes open from closing events on `deletion_time`:

```cpp
if (obj.deletion_time() > 0)
  _acknowledgements.erase({host_id, service_id});            // closing event → remove
else
  _acknowledgements.insert_or_assign({host_id, service_id}, ack);  // open event → store
```

This guard is essential: the closing event published by the cache itself is fed back through the
multiplexer, so a closing event must **remove** the entry, not re-insert it.

### Closing an acknowledgement

Each status handler (`update_host` / `update_service`, both the full `pb_*_status` and the
`pb_adaptive_*_status` forms) calls `_take_expired_acknowledgement(host_id, service_id, ack_type,
state)` **under the cache lock**. That method removes the entry and, when the ack must be closed,
stamps `deletion_time` and returns the event so the **caller publishes it after releasing the lock**
— publishing under the lock would re-enter `_publish()`/`update_acknowledgement()` and risk a
re-entrant / lock-order deadlock with the multiplexer.

---

## BBDO2 vs BBDO3

The cache reasons in protobuf. Native **BBDO3** events (`pb_acknowledgement`, `pb_service_status`,
`pb_host_status`, and the adaptive forms) are handled directly. **BBDO2** legacy events are converted
first, in `broker/neb/src/bbdo2_to_bbdo3.cc`:

| Legacy event | Converter | `_publish` case |
|---|---|---|
| `neb::acknowledgement` | `_acknowledgement_to_pb` | `de_acknowledgement` |
| `neb::service_status` | `_service_status_to_pb` | `de_service_status` |
| `neb::host_status` | `_host_status_to_pb` | `de_host_status` |

Both modes run a `unified_sql` output by default, so the cache sections are enabled in **both**. The
only BBDO-version-specific point is the **event type**: a BBDO2 poller emits legacy `service_status`
/ `host_status`, which must be routed to `update_service` / `update_host` (via `bbdo2_to_bbdo3`) for
the closure detection to run. Without the `de_service_status` / `de_host_status` cases, a BBDO2
acknowledgement is created and stored but **never closed** on recovery or explicit removal.

---

## Persistence

`broker_cache` persists its open acknowledgements so they survive a **`cbd`** restart, mirroring the
active-downtime persistence:

* `BrokerCache.acknowledgements` — a `repeated Acknowledgement` field in `bbdo/neb.proto`.
* `_save_cache()` writes the live `_acknowledgements` map (both legacy and centralized mode).
* `_load_cache()` reads them back into the map at construction **without republishing** — the DB rows
  already exist; only the in-memory tracking is rebuilt, so the cache can still close an
  acknowledgement on a later recovery.

This closes a gap in the old design: cbmod's tracking map was **not** persisted and was **not**
rebuilt on restart (Engine retention restores the notifier field but does not re-emit an
acknowledgement event), so the close-on-recovery bookkeeping was lost across a restart. The
acknowledgement *effect* always survived an **Engine** restart through retention.

---

## The acknowledgement comment

Creating an acknowledgement also creates a `comment` with `entry_type = acknowledgment` (value `4`).
A **non-persistent** ack comment's id is kept on `notifier::_acknowledgement_comment_id` and deleted
when the acknowledgement is cleared; a persistent one is not tracked and survives. See
[comments-integration-en.md](comments-integration-en.md).

---

## Broker side (unified_sql)

`broker/unified_sql/src/stream_sql.cc` — `_process_pb_acknowledgement` (BBDO3) and
`_process_acknowledgement` (BBDO2):

* **Creation** (`deletion_time` unset) → `INSERT ... ON DUPLICATE KEY UPDATE` into the
  `acknowledgements` table (unique key `(entry_time, host_id, service_id)`).
* **Closure** (`deletion_time` set) → the same row is updated with its `deletion_time`.

In parallel, the `acknowledged` / `acknowledgement_type` flags on the `hosts` / `services` /
`resources` rows are maintained from the status events — so the UI's "acknowledged" flag clears on
recovery even when no closing acknowledgement event is written (e.g. a plain BBDO2 recovery).

---

## `notification_mode = broker`: Broker is the acknowledgement authority

`broker/core/inc/com/centreon/broker/broker_acknowledgement_manager.hh`, `broker/core/src/broker_acknowledgement_manager.cc`

Everything above describes the `engine` mode: Engine sets and clears the acknowledgement, Broker
tracks it. In `notification_mode = broker`, Engine is **never told** about acknowledgements (see
[Centralized downtime and acknowledgement management](./nego-engine-broker-en.md#centralized-downtime-and-acknowledgement-management)).
Broker receives them through `BrokerRpc`, stores them, exports them to the database and clears them
itself. The `broker_acknowledgement_manager` is a singleton loaded by `broker_state` in that mode
only, next to the `downtime_manager`; all the state lives in `broker_cache`, the class only
orchestrates.

### gRPC endpoints

```proto
rpc AcknowledgeHostProblem(AcknowledgementRequest) returns (google.protobuf.Empty) {}
rpc AcknowledgeServiceProblem(AcknowledgementRequest) returns (google.protobuf.Empty) {}
rpc RemoveHostAcknowledgement(HostIdentifier) returns (google.protobuf.Empty) {}
rpc RemoveServiceAcknowledgement(ServiceIdentifier) returns (google.protobuf.Empty) {}

message AcknowledgementRequest {   // same fields as EngineAcknowledgement (engine.proto)
  string host_name = 1; string service_desc = 2;
  string ack_author = 3; string ack_data = 4;
  enum Type { NORMAL = 0; STICKY = 1; } Type type = 5;
  bool notify = 6; bool persistent = 7;
}
message HostIdentifier { oneof host { string host_name = 1; uint64 host_id = 2; } }
```

`AcknowledgementRequest` copies Engine's request field for field: PHP switches the gRPC target and
the RPC name, not the payload. Failures go through the gRPC status: `UNAVAILABLE` outside broker
mode, `NOT_FOUND` for a resource unknown to the cache, `FAILED_PRECONDITION` when the resource is
UP/OK ("cannot acknowledge a non-existent problem", like Engine), `INVALID_ARGUMENT` on an empty
identifier.

### Setting an acknowledgement: Engine's flow, replayed by Broker

`broker_acknowledgement_manager::acknowledge()` mirrors Engine's `acknowledge_host_problem()` /
`acknowledge_service_problem()` step by step:

| Engine step | Broker counterpart |
|---|---|
| refuse when UP/OK | state read from the cache, `FAILED_PRECONDITION` |
| internal `acknowledgment` comment | `pb_comment` (entry_type ACKNOWLEDGMENT, source INTERNAL, id from Broker's partitioned range, **same `entry_time` as the ack**: the GUI joins both rows on it) |
| `broker_acknowledgement_data` → `pb_acknowledgement` | `pb_acknowledgement` published, with the host's poller `instance_id`, the current `state` and the new `comment_id` field (13) |
| `set_acknowledgement(type)` + `update_status(STATUS_ACKNOWLEDGEMENT)` | **synchronous** `broker_cache::set_acknowledgement_type()` (the next notification decision must see the flag), then `pb_adaptive_*_status{acknowledgement_type}` for the database |
| `notify(reason_acknowledgement)` when `notify` | `notification_manager::notify(reason_acknowledgement, author, data)`; the execution goes to the poller through `pb_notification_execute` |
| `EXTERNAL COMMAND: ACKNOWLEDGE_*` line → `logs` | `pb_log_entry` msg_type `SERVICE_ACKNOWLEDGE_PROBLEM` (10) / `HOST_ACKNOWLEDGE_PROBLEM` (11), author in `notification_contact`, comment in `output` |

A new acknowledgement on an already acknowledged resource replaces the previous one and deletes its
non-persistent comment, like Engine's `delete_acknowledgement_comment()`.

### Clearing

* **Explicit** (`remove()`): `set_acknowledgement_type(NONE)` applies the closing rule of
  `_take_expired_acknowledgement` with the current state — the resource is still in a problem state,
  so the ack gets its `deletion_time` and is republished; the non-persistent comment is deleted; a
  `NONE` adaptive status re-aligns `hosts`/`services`/`resources`.
* **Automatic** (`clear_on_state_change()`), called by `broker_notification_dispatcher` for **every**
  `pb_host_status`/`pb_service_status` (SOFT included), *before* the notification decision, with the
  rule of `notifier::handle_state()`: a NORMAL ack is cleared on any state change, a STICKY one only
  on the return to UP/OK. The order matters: leaving a non-sticky ack (WARNING → CRITICAL) must be
  notified, so the flag has to be down when `notify()` reads `get_state().acknowledged`. The closing
  rule is applied with the state **of the event**, not the cached one, to stay deterministic: on
  recovery the ack is dropped from the cache without `deletion_time`, exactly like engine mode (see
  "Closure").

### The guards: never let Engine overwrite Broker

Engine does not know the ack, so every status it sends carries `acknowledgement_type = NONE`.
Without a guard the cache would reset the type and unified_sql `acknowledged = 0` on every check.
Same fix as for `scheduled_downtime_depth`:

* `broker_cache::update_host/update_service(pb_*_status)` apply the type coming from Engine only
  when `notification_manager::is_loaded()` is false;
* `unified_sql` binds `NULL` on `acknowledged` / `acknowledgement_type` in the status statements
  (`hosts`, `services`, `resources`), which carry `COALESCE(?, column)`.

"Broker owns it" is read through `notification_manager::is_loaded()`, loaded at the same place as
the acknowledgement manager.

### Restarts

* **Broker restarts**: the acks are reloaded from `BrokerCache.acknowledgements` (comment_id
  included), but the cached hosts/services are rebuilt with type NONE, and Engine cannot restore it.
  Two mechanisms do: `broker_cache::_restore_acknowledgement_type()` when a `pb_host`/`pb_service`
  definition is inserted (non-centralized BBDO3), and `reinject_pending_acknowledgements()` at the
  two downtime re-injection points (startup barrier, `_process_engine_state` after the merge in
  centralized mode). Both republish an adaptive status, because the definitions that rebuilt the
  resources in the database wrote `acknowledged = 0`.
* **Engine restarts**: it resends its definitions with type NONE; `_restore_acknowledgement_type`
  puts the flag back (copy-on-write, the incoming event is shared with the output streams).

### What Engine loses in this mode (to be addressed)

* The `$TOTAL*UNHANDLED$` macros count an acknowledged problem as unhandled (already true for
  downtimes) — fix planned through a downward mirror to the poller, see the dedicated note in
  `nego-engine-broker`.
* Expiration through `acknowledgement_timeout` is an Engine timer: it no longer fires. Not ported to
  Broker for now.

### Tests

`tests/broker-engine/acknowledgements-broker.robot` (`BEACKBRK1` to `BEACKBRK5`): set and cleared on
recovery, explicit removal, sticky/normal on state change, host ack surviving a Broker restart, and
the notification decision (CRITICAL, ACKNOWLEDGEMENT, suppression while acknowledged, RECOVERY).
UT: `BrokerNotificationDeliverTest.{EngineStatusKeepsBrokerAcknowledgement,
AcknowledgementClosingRule, ReinjectPendingAcknowledgements}`.

---

## gRPC: GetAcknowledgements

`broker/core/brokerrpc/broker.proto`, `broker_impl.cc`

```proto
rpc GetAcknowledgements(google.protobuf.Empty) returns (AcknowledgementList) {}
message AcknowledgementList { repeated Acknowledgement entries = 1; }  // Acknowledgement from neb.proto
```

`broker_impl::GetAcknowledgements` returns one `Acknowledgement` per cached acknowledgement (host
acks carry `service_id == 0`), or `UNAVAILABLE` when neither `CACHE_HOSTS` nor `CACHE_SERVICES` is
enabled. It reads a snapshot via `broker_cache::acknowledgements()`. This endpoint exposes the cache
contents for diagnostics and tests (it is what the `BEACK9`/`BEACK10` Robot tests assert on).

---

## Relation to poller HA

By moving acknowledgement tracking from cbmod (the fungible poller process) into `broker_cache` (the
center) and making it durable, the acknowledgement state can follow a resource that is relocated
from one poller to another: the center holds it, instead of it being tied to a single poller's
retention. This is one of the "Broker carries durable state" steps described in
[Target architecture — toward poller HA](./ha-target-architecture-en.md).

---

## Before / after

| Aspect | Before (cbmod) | Now (broker_cache) |
|---|---|---|
| Open-ack tracking | `cbmod::_acknowledgements` map, in the **Engine** process | `broker_cache::_acknowledgements`, in **`cbd`** |
| Closure on recovery / removal | emitted by cbmod on each status | emitted by the cache via `_take_expired_acknowledgement` |
| BBDO2 closure | done by cbmod (output-independent) | needs legacy status routed into the cache (`de_service_status` / `de_host_status`) |
| Persistence across `cbd` restart | none (map lost, not rebuilt) | persisted in `BrokerCache.acknowledgements`, reloaded without republish |
| Observability | none | gRPC `GetAcknowledgements` |
| Notification suppression | Engine notifier | Engine notifier in `notification_mode = engine`; `notification_manager` on Broker in `notification_mode = broker` |
