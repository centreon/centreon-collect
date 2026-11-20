# Acknowledgements — Engine ↔ Broker integration

<!-- TOC -->
* [Acknowledgements — Engine ↔ Broker integration](#acknowledgements--engine--broker-integration)
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
| Notification suppression | Engine notifier | Engine notifier (unchanged) |
