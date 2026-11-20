# Comments — Engine ↔ Broker integration

<!-- TOC -->
* [Comments — Engine ↔ Broker integration](#comments--engine--broker-integration)
  * [Overview](#overview)
  * [The comment object (Engine side)](#the-comment-object-engine-side)
  * [Who remembers a comment id](#who-remembers-a-comment-id)
  * [Creation](#creation)
  * [Deletion](#deletion)
    * [Deletion by id](#deletion-by-id)
    * [Bulk deletion](#bulk-deletion)
  * [Broker side (unified_sql)](#broker-side-unified_sql)
  * [Expiration (removed)](#expiration-removed)
  * [Retention](#retention)
  * [status.dat](#statusdat)
  * [Per-type lifecycle](#per-type-lifecycle)
    * [user](#user)
    * [downtime](#downtime)
    * [flapping](#flapping)
    * [acknowledgement](#acknowledgement)
  * [Comment identity: comment_id vs internal_id](#comment-identity-comment_id-vs-internal_id)
  * [Before / after](#before--after)
<!-- TOC -->

---

## Overview

Comment management used to live **entirely in Engine**, in a static in-memory map
(`comment::comments`). It has been reworked so that **Engine keeps no comment in memory**: Broker
**owns** the comments (the `centreon_storage.comments` table). Engine only:

* emits a creation event when a comment is created;
* emits a deletion event (by id, or in bulk by target) when a comment must be removed;
* remembers, on the owning runtime object, the few comment ids it needs to drive those deletions.

> **Note.** Unlike downtimes, comments did **not** get a shared `common/comments` library with a
> `comment_manager` singleton and injected callbacks. A leaner approach was taken: the `comment`
> object stays in Engine but becomes **transient** (it exists only long enough to emit its creation
> event), and Broker owns the storage. There is therefore no `comment_manager` / `comment_callbacks`
> abstraction to implement.

A comment has one of four **entry types** (`entry_type`): `user`, `downtime`, `flapping`,
`acknowledgment`.

---

## The comment object (Engine side)

`engine/inc/com/centreon/engine/comment.hh`, `engine/src/comment.cc`

| Field | Type | Meaning |
|---|---|---|
| `_comment_type` | `enum type { host = 1, service }` | Carrying object |
| `_entry_type` | `enum e_type { user = 1, downtime, flapping, acknowledgment }` | Comment origin |
| `_comment_id` | `uint64_t` | Identifier minted by Engine (stored in the DB as `internal_id`) |
| `_source` | `enum src { internal, external }` | Created by the engine or by an external command |
| `_persistent` | `bool` | Survives restarts / not auto-deleted |
| `_entry_time` | `time_t` | Creation time |
| `_expires` / `_expire_time` | `bool` / `time_t` | Present but **always `false`/`0`** — expiration is unused (see [Expiration](#expiration-removed)) |
| `_host_id` / `_service_id` | `uint64_t` | Target object (`_service_id == 0` ⇒ host comment) |
| `_author` | `std::string` | Author |
| `_comment_data` | `std::string` | Text |

The object is **transient**: it is built on the stack at the creation site, its constructor mints an
id and emits the creation event, and it is then destroyed. There is **no in-memory map** anymore.

The id is minted monotonically: `_comment_id = _next_comment_id++`. `_next_comment_id` is persisted
in the retention file (`retention/program.cc`, key `next_comment_id`) so ids keep growing across
restarts, and is reset to `1` on configuration reload (`applier/state.cc`).

---

## Who remembers a comment id

Engine must be able to delete a comment later. Rather than keeping every comment in memory, the id is
stored on the **owning runtime object** — except for `user` comments, whose id is supplied by the UI
at deletion time.

| `entry_type` | Created by | Where the id is kept | Deleted when |
|---|---|---|---|
| `user` | `cmd_add_comment` — `ADD_HOST_COMMENT` / `ADD_SVC_COMMENT` (and the gRPC `AddHostComment`/`AddServiceComment`) | — (the UI supplies the id) | `DEL_*_COMMENT` by id, `DEL_ALL_*`, or object deletion |
| `downtime` | `engine_downtime_callbacks` when a downtime subscribes | `downtime::_comment_id` | downtime object destruction |
| `flapping` | `host::set_flap()` / `service::set_flap()` | `notifier::_flapping_comment_id` | `clear_flap()` (flapping ends) |
| `acknowledgment` | `cmd_acknowledge_*_problem` (and the gRPC ack handlers) | `notifier::_acknowledgement_comment_id` (**non-persistent only**) | ack cleared → delete by stored id |

A persistent acknowledgement comment is intentionally **not** tracked (its id stays `0`) so it
survives the acknowledgement being cleared — same semantics as before, without a map scan.

---

## Creation

```
creation site (command / gRPC / flapping / ack / downtime)
   └─ build a transient `comment(...)`
        ├─ mint internal_id (_next_comment_id++)
        └─ broker_comment_data(NEBTYPE_COMMENT_ADD, ...)  ─▶ pb_comment ─▶ Broker INSERT
   └─ (if needed) store the id on the owning object, then drop the comment object
```

The constructor emits the creation event **only** when it mints the id (no id supplied). A comment
built with an explicit id (the retention backward-compat path) emits nothing.

---

## Deletion

Engine no longer rebuilds the full comment tuple for a deletion: it addresses the row directly.

### Deletion by id

Used by `DEL_HOST_COMMENT` / `DEL_SVC_COMMENT` and by the targeted deletions (downtime destruction,
`clear_flap()`, acknowledgement clear).

`comment::delete_comment(id)` emits a `pb_comment` carrying only `internal_id` (= the id) and
`deletion_time`; `host_id`/`service_id` are left at `0`. Broker matches the row on
**`(internal_id, instance_id)`** (the poller id is filled by cbmod).

### Bulk deletion

Used by `DEL_ALL_HOST_COMMENTS` / `DEL_ALL_SVC_COMMENTS` and by object deletion (a host/service
removed from the configuration).

`comment::delete_host_comments(host_id)` / `delete_service_comments(host_id, service_id)` emit a
**single** event with `internal_id = 0` (the bulk sentinel) plus the target `host_id`
(`+ service_id`). Broker deletes every matching row in one statement. No map iteration.

---

## Broker side (unified_sql)

`broker/unified_sql/src/stream_sql.cc`, in **both** comment handlers —
`_process_pb_comment` (BBDO3 `pb_comment`) and `_process_comment` (BBDO2 `neb::comment`):

* **Creation** (`deletion_time` not set) → the existing `INSERT INTO comments ... ON DUPLICATE KEY
  UPDATE` (batched through the `_comments` bulk).
* **Deletion** (`deletion_time` set) → a dedicated `UPDATE`, because the partial event can no longer
  match the unique key:
  * `internal_id != 0` → `UPDATE comments SET deletion_time=? WHERE internal_id=? AND instance_id=?`
  * `internal_id == 0` and `service_id != 0` → `... WHERE host_id=? AND service_id=? AND instance_id=?`
  * `internal_id == 0` and `service_id == 0` → `... WHERE host_id=? AND (service_id=0 OR service_id IS NULL) AND instance_id=?` (host comments only)

**Ordering.** Before running the deletion `UPDATE`, the handler flushes any pending comment
*creation* still in the `_comments` bulk, on the **same** connection
(`special_conn::comment`), so the `UPDATE` is applied after the `INSERT` (FIFO preserved). This
matters when a comment is created and deleted within the same flush window.

`internal_id == 0` is a safe sentinel: Engine mints ids from `1`, so `0` never matches a real
comment.

---

## Expiration (removed)

The comment expiration machinery was **dead code** and has been removed. Every comment is created
with `expires = false` (external commands carry no expire parameter; flapping, downtime and ack
comments all pass `false`), so nothing ever produced an expiring comment. The
`EVENT_EXPIRE_COMMENT` timed event, its handler, and `comment::remove_if_expired_comment()` are
gone. The `expires` / `expire_time` columns remain in the proto/DB for compatibility but are always
unset.

---

## Retention

Engine no longer writes the **comment list** to its retention file — Broker owns the comments, and
they survive an Engine restart in the database (Broker is not restarted with Engine). What Engine
still persists is just what it needs to keep driving deletions:

* `flapping_comment_id` and `acknowledgement_comment_id` on each **host/service** retention block
  (`retention/host.cc`, `retention/service.cc`, their appliers and `retention/dump.cc`);
* the `next_comment_id` counter (unchanged).

For a smooth upgrade, an **old** retention file that still contains `comment { ... }` blocks is still
parsed: `retention/applier/comment.cc` uses those blocks only to (1) re-link a kept non-persistent
acknowledgement comment id onto its notifier and (2) purge non-persistent comments at boot. It no
longer creates any comment object.

**Behaviour change (accepted):** non-persistent *user* comments are no longer purged at restart
(there is no owning object to hold them); the database retention purge
(`len_storage_comments`) handles their long-term cleanup instead.

---

## status.dat

Comments are **no longer written** to `status.dat` (`xsddefault.cc`). That legacy file is not used by
Centreon to read comments (the UI reads the database); only external consumers of `status.dat` are
affected.

---

## Per-type lifecycle

### user

`ADD_HOST_COMMENT` / `ADD_SVC_COMMENT` (or the gRPC equivalents) create the comment. Deletion is
driven by the UI, which sends the `internal_id` in `DEL_*_COMMENT` (see
[Comment identity](#comment-identity-comment_id-vs-internal_id)); Engine relays it to Broker as a
delete-by-id. `DEL_ALL_*` and host/service removal trigger a bulk deletion.

### downtime

See [downtimes-integration-en.md](downtimes-integration-en.md). The downtime creates its comment when
it subscribes and stores `_comment_id`; it deletes that id on destruction. The comment therefore
lives exactly as long as the downtime object. The `START`/`STOP` transition does **not** touch the
comment.

### flapping

`host::set_flap()` / `service::set_flap()` create a `flapping` comment and store its id in
`notifier::_flapping_comment_id`; `clear_flap()` deletes it by id. The comment lives for the duration
of the flapping episode. The id is persisted in retention, so it survives an Engine restart.

### acknowledgement

`cmd_acknowledge_*_problem` (and the gRPC ack handlers) create the comment. A **non-persistent** ack
comment's id is stored on `notifier::_acknowledgement_comment_id`; when the acknowledgement is
cleared, `notifier::delete_acknowledgement_comment()` deletes it by id and resets the field. A
persistent ack comment is not tracked and survives the clear. The id is persisted in retention; an
old retention file relinks it through `retention/applier/comment.cc`.

---

## Comment identity: comment_id vs internal_id

The `centreon_storage.comments` table carries **two** identifiers:

```sql
comment_id  int NOT NULL AUTO_INCREMENT,        -- PRIMARY KEY, generated by the database
internal_id int NOT NULL,                        -- = Engine's minted comment id
UNIQUE KEY (entry_time, host_id, service_id, instance_id, internal_id),
KEY (internal_id)
```

* `comment_id` (PK) is fully managed by MariaDB; it is **not** used to address a comment today — the
  UI never sends it back.
* `internal_id` is the id minted by Engine, and it is **load-bearing**:
  1. **UI deletion handle.** When an operator deletes a comment, the Centreon UI/API sends an external
     command `DEL_HOST_COMMENT` / `DEL_SVC_COMMENT` carrying the **`internal_id`** — *not* the
     `comment_id` PK (`www/include/monitoring/comments/common-Func.php`). Engine relays that id; Broker
     deletes the row whose `internal_id` matches.
  2. **Idempotency key.** It disambiguates the unique key (two comments created the same second on the
     same host/service from the same poller would otherwise collide) and, because the create
     `INSERT ... ON DUPLICATE KEY UPDATE` never writes `comment_id`, it is what makes a replayed BBDO
     `pb_comment` upsert in place instead of duplicating.

**Possible future evolution (not done):** hand identity over to the `comment_id` PK. This is a
coordinated **cross-repo migration** (Centreon web PHP + Broker), precisely because the UI addresses
comments by `internal_id` today: the UI would send `comment_id`, Broker would delete by `comment_id`,
and Engine would stop minting ids. Even then, `internal_id` (or an equivalent stable key) is still
needed for idempotency as long as Engine is the *producer* and BBDO can replay — it disappears only
if comment **origination** itself moves into Broker.

---

## Before / after

| Aspect | Engine before | Engine now |
|---|---|---|
| Storage | static `comment::comments` map | none — Broker owns the `comments` table |
| `comment` object | held in the map | transient (emits its creation event, then destroyed) |
| Id minting | `next_comment_id` + map scan | monotonic `next_comment_id++` |
| Deletion by id | rebuilds the full tuple from the map | emits `internal_id` only; Broker `UPDATE ... WHERE internal_id AND instance_id` |
| Bulk deletion | map iteration, one event per comment | one event (`internal_id = 0`); Broker `UPDATE ... WHERE host_id [AND service_id]` |
| Acknowledgement deletion | scan the map by `entry_type == ack` | delete by id stored on the notifier |
| Expiration | `EVENT_EXPIRE_COMMENT` (never fired) | removed (dead code) |
| Retention | full comment list + `next_comment_id` | ids on host/service objects + `next_comment_id` |
| status.dat | comments written | comments not written |
