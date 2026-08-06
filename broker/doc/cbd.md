# How `cbd` (Centreon Broker) runs

From process start to a row in MariaDB.

> For the detailed, layer-by-layer version — each layer in its own file, with links to the exact
> functions — see [cbd/README.md](cbd/README.md).

- [1. Startup sequence](#1-startup-sequence)
- [2. How an endpoint becomes a running object](#2-how-an-endpoint-becomes-a-running-object)
- [3. The multiplexing core](#3-the-multiplexing-core)
- [4. Engine → Broker → SQL](#4-engine--broker--sql)

---

## 1. Startup sequence

Source: `broker/core/src/main.cc`

```mermaid
%%{init: {"sequence": {"useMaxWidth": false, "actorFontSize": 15, "messageFontSize": 14, "noteFontSize": 13}}}%%
sequenceDiagram
    participant OS
    participant main as main() cbd
    participant log as log_v2
    participant pool as common::pool (asio)
    participant parser as config::parser
    participant init as applier::init
    participant state as applier::state
    participant mods as applier::modules
    participant endp as applier::endpoint
    participant mux as multiplexing::engine
    participant rpc as brokerrpc (gRPC)

    OS->>main: exec cbd /etc/centreon-broker/central-broker.json
    main->>log: load("cbd")
    main->>pool: load(g_io_context) — thread pool
    main->>main: signals->async_wait(SIGHUP, SIGTERM)
    main->>parser: parse(configfile)
    parser-->>main: config::state conf
    main->>log: apply(log_conf) (before threads start)
    main->>init: init(BROKER, conf)
    init->>state: load() / mysql_manager::load()
    init->>init: disk_accessor::load(), io::protocols::load(), io::events::load()
    init->>mux: engine::load()  (singleton, state=not_started)
    init->>endp: endpoint::load()
    main->>state: apply(conf, run_mux=true)
    state->>mods: apply(module_list, module_dir)
    mods->>mods: dlopen *.so → each broker_module_init()<br/>registers itself in io::protocols (OSI layers)
    state->>mux: muxer::event_queue_max_size(conf)
    state->>endp: apply(endpoints, params) → build acceptors/failovers
    state->>mux: engine::start()  (state=running, flush cache file)
    main->>rpc: new brokerrpc(listen_addr, 51000+broker_id)
    loop while (!gl_term)
        main->>main: sleep 1s
    end
    OS->>main: SIGTERM → gl_term = true
    main->>init: applier::deinit() (feeders → failovers → engine::stop)
    Note over mux: engine::stop() writes unprocessed<br/>events to the cache file
```

Two things worth noting:

- the main thread does nothing but `sleep(1)` — all real work happens on the asio pool and on
  per-endpoint threads;
- `SIGHUP` re-parses the config and calls `state::apply(conf)` again, rolling back to `gl_state`
  if it throws (`broker/core/src/main.cc:135-182`).

---

## 2. How an endpoint becomes a running object

Source: `broker/core/src/config/applier/endpoint.cc`

Each `input`/`output` block in the JSON is turned into an OSI-like **stack of `io::endpoint`s**,
built bottom-up by `_create_endpoint()`. Modules register themselves with an OSI range at load
time:

| Layer | Registered by | Examples |
|---|---|---|
| 1–4 (transport) | `tcp`, `grpc`, `file`, `RRD`, `unified_sql`, `OTLP`, `graphite`, `bam`… | `broker/unified_sql/src/main.cc:160`, `broker/rrd/src/main.cc:122` |
| 5 | `TLS` | `broker/tls/src/main.cc:69` |
| 6 | compression | |
| 7 | `BBDO` | `broker/core/bbdo/internal.cc:60` |

```mermaid
flowchart TB
    cfg["config::endpoint (JSON block)"] --> ce["_create_endpoint()<br/>walk io::protocols levels 1→7"]
    ce --> isacc{"is_acceptor?"}

    isacc -->|"yes — e.g. TCP listen 5669"| A["processing::acceptor<br/>own std::thread"]
    A -->|"accept() per connection"| F["processing::feeder<br/>+ its own muxer"]

    isacc -->|"no — output: unified_sql, RRD, TCP connector"| M["muxer::create(name, r_filter, w_filter)"]
    M --> FO["processing::failover<br/>own std::thread _run()"]

    subgraph stack["stream stack (one per connection)"]
      direction TB
      bbdo["bbdo::stream (7) — serialize/unserialize io::data"]
      comp["compression (6)"]
      tls["tls::stream (5)"]
      tcp["tcp::stream (1-4)"]
      bbdo --> comp --> tls --> tcp
    end

    F --- stack
    FO --- stack
```

- **acceptor** = server side. One thread doing `accept()`; every accepted connection spawns a
  `feeder` (`broker/core/src/processing/acceptor.cc:68-94`).
- **failover** = client/output side. One dedicated thread with a
  `read stream → write muxer / read muxer → write stream` loop
  (`broker/core/src/processing/failover.cc:295-400`).
- **feeder** is event-driven instead of a loop: it registers itself as the muxer's `data_handler`
  and gets called back from the asio pool (`broker/core/src/processing/feeder.cc:67-72`).

---

## 3. The multiplexing core

One engine, N muxers.

```mermaid
flowchart LR
    subgraph inputs
      i1["feeder A<br/>(poller 1 connection)"]
      i2["feeder B<br/>(poller 2 connection)"]
    end

    i1 -->|"muxer::write"| E
    i2 -->|"muxer::write"| E

    E["multiplexing::engine (singleton)<br/>_kiew deque + _muxers vector"]

    E -->|"global_cache::write(evt) first"| GC[(global_cache)]
    E -->|"publish → 1st muxer on this thread,<br/>others via asio::post"| m1
    E --> m2
    E --> m3

    m1["muxer central-broker-master-sql"] --> fo1["failover"] --> sql["unified_sql::stream"]
    m2["muxer central-broker-master-rrd"] --> fo2["failover"] --> rrd["TCP→cbd rrd / rrd::stream"]
    m3["muxer centreon-bam-monitoring"] --> fo3["failover"] --> bam["bam::stream"]

    sql -.->|"republish pb_metric / pb_status<br/>multiplexing::publisher().write()"| E
```

The muxer is the buffering + back-pressure unit
(`broker/core/multiplexing/inc/com/centreon/broker/multiplexing/muxer.hh:29-50`):

```mermaid
flowchart TB
    P["engine → muxer::publish(deque)"] --> WF{"_write_filter.allows(type)?"}
    WF -->|"no"| drop["dropped"]
    WF -->|"yes"| full{"_events_size &lt; event_queue_max_size?"}
    full -->|"yes"| Q["_push_to_queue → _events (memory list)"]
    full -->|"no"| RF["persistent_file(_queue_file_name)<br/>= retention file on disk"]
    Q --> R["_execute_reader_if_needed()<br/>asio::post → data_handler on_events()"]
    R --> W["stream write(event)"]
    W --> ACK["muxer::ack_events(n)<br/>pop_front + refill from retention file"]
```

The key invariant: an event is **only removed from the queue when the destination acknowledges
it** (`_pos` marks the read cursor, `_events.begin()` the un-acked head). That is what makes
retention survive a broker or DB outage.

---

## 4. Engine → Broker → SQL

`centengine` does not talk BBDO itself. It embeds a *whole broker core* via **cbmod** — that is
why `cbmod::cbmod()` calls `config::applier::init(ENGINE, ...)` (`broker/neb/src/cbmod.cc:49-90`).
So there is a multiplexing engine + muxer + failover *inside the centengine process*.

The entry points are the `broker_*()` callbacks in `engine/src/broker.cc` — e.g.
`broker_service_status()` at `engine/src/broker.cc:4796`, which forwards to
`cbm->write(...)` (`broker/neb/src/cbmod.cc:110`).

The full path is split below into three stages, because it crosses two processes:

```mermaid
flowchart LR
    A["Stage A — inside centengine<br/>(cbmod)"] -->|"BBDO over TLS/TCP :5669"| B["Stage B — cbd input side<br/>(acceptor → feeder)"]
    B -->|"re-injected in the engine"| C["Stage C — cbd output side<br/>(muxer → unified_sql → DB)"]
```

### Stage A — from the check result to the wire (centengine process)

```mermaid
%%{init: {"sequence": {"useMaxWidth": false, "actorFontSize": 15, "messageFontSize": 14, "noteFontSize": 13}}}%%
sequenceDiagram
    autonumber
    participant svc as engine::service<br/>(check finished)
    participant cb as broker_service_status()
    participant cbm as neb::cbmod
    participant pub as multiplexing::publisher
    participant e1 as engine<br/>(in centengine)
    participant mx1 as muxer<br/>central-module-master-output
    participant fo1 as failover →<br/>bbdo::stream → TLS → tcp

    svc->>cb: check result / status change
    cb->>cbm: cbm->write(pb_service_status)
    cbm->>pub: _publisher.write(msg)
    pub->>e1: engine::publish(d) → _kiew
    e1->>mx1: _send_to_subscribers → publish()
    mx1->>fo1: on_events / read()
    fo1->>fo1: _stream->write(d) → serialize BBDO
    Note right of fo1: TCP/TLS bytes leave the process<br/>towards cbd :5669
```

### Stage B — reception in cbd, back up to the engine

```mermaid
%%{init: {"sequence": {"useMaxWidth": false, "actorFontSize": 15, "messageFontSize": 14, "noteFontSize": 13}}}%%
sequenceDiagram
    autonumber
    participant tcpA as tcp acceptor :5669<br/>(in cbd)
    participant accp as processing::acceptor
    participant fdr as processing::feeder
    participant bs2 as bbdo::stream<br/>(unserialize)
    participant mx2 as muxer of that feeder
    participant e2 as engine<br/>(in cbd)

    tcpA->>accp: accept()
    accp->>fdr: feeder::create(name, engine::instance_ptr(),<br/>stream, filters)
    Note over fdr: creates its OWN muxer + registers<br/>set_action_on_new_data(this)
    fdr->>bs2: _read_from_stream_timer → _client->read(d)
    bs2-->>fdr: io::data (unserialized pb_service_status)
    fdr->>mx2: _muxer->write(d)
    mx2->>e2: engine::publish(d) ← re-injected at the root
```

### Stage C — fan-out to unified_sql, and the acknowledgement path back

```mermaid
%%{init: {"sequence": {"useMaxWidth": false, "actorFontSize": 15, "messageFontSize": 14, "noteFontSize": 13}}}%%
sequenceDiagram
    autonumber
    participant e2 as engine<br/>(in cbd)
    participant mxs as muxer<br/>central-broker-master-sql
    participant fos as failover
    participant us as unified_sql::stream
    participant my as mysql_connection<br/>threads
    participant db as MariaDB

    e2->>e2: global_cache::write(evt)
    e2->>mxs: publish() to ALL muxers (sql, rrd, bam…)
    mxs->>fos: read()
    fos->>us: _stream->write(d)
    us->>us: neb_processing_table[elem]<br/>→ _process_pb_service_status()
    us->>my: bulk_bind on choose_connection_by_instance()
    my->>db: prepared/bulk INSERT..ON DUPLICATE KEY UPDATE
    us-->>fos: return _ack (n events committed)
    fos->>mxs: muxer::ack_events(n) → drop from queue/retention
```

### Where exactly the data "lands"

**Reception point:** `processing::acceptor::accept()`
(`broker/core/src/processing/acceptor.cc:68`) — every incoming poller connection creates a
`feeder`, and **the feeder creates its own muxer** (`broker/core/src/processing/feeder.cc:91-95`).
So yes, there is a muxer on the input side too. It is used in the *other* direction than you
would expect:

- `muxer::write(d)` (called by feeder/failover) does **not** enqueue — it forwards to
  `engine::publish()`, i.e. the root of the tree.
- `muxer::publish(deque)` (called only by the engine) enqueues into that muxer's own queue, for
  the stream to consume.

That asymmetry is the whole trick: input muxers push *up* to the engine, output muxers buffer
*down* to their stream. See the class doc at
`broker/core/multiplexing/inc/com/centreon/broker/multiplexing/muxer.hh:38-48`.

**Fan-out:** `engine::_send_to_subscribers()` (`broker/core/multiplexing/src/engine.cc`) writes
the batch to `global_cache` **first** (outputs may need it), then delivers to every muxer — the
first one on the calling thread, the rest via `asio::post` on the pool.

**SQL landing:** `unified_sql::stream::write()` (`broker/unified_sql/src/stream.cc:711`) is a pure
dispatcher:

```cpp
uint16_t cat  = category_of_type(type);
uint16_t elem = element_of_type(type);
if (cat == io::neb)
  (this->*(neb_processing_table[elem]))(data);   // 50-entry jump table
```

`neb_processing_table` (`broker/unified_sql/src/stream.cc:49-108`) maps BBDO element ids to
`_process_*` handlers. For a service status that is `_process_pb_service_status()`
(`broker/unified_sql/src/stream_sql.cc:4570`), which:

1. drops the event if the host is not in `_cache_host_instance` (unknown poller);
2. picks a DB connection with `_mysql.choose_connection_by_instance(instance_id)` — connections
   are sharded per poller so ordering per poller is preserved;
3. fills a `bulk_bind` row (or a plain prepared statement if bulk is not available);
4. lets the `mysql_connection` worker thread execute it asynchronously.

Actual flushing is time/count driven, back in `stream::write()`:

```cpp
if (now >= _next_loop_timeout || _count >= _max_pending_queries) {
  _count = 0; _next_loop_timeout = now + 10;
  _finish_actions();          // commits, then bumps _ack
}
int32_t retval = _ack; _ack -= retval;
return retval;                // ← this is what un-blocks the muxer queue
```

So the number returned by `write()` is the acknowledgement count that flows back through
`failover` → `muxer::ack_events()`. Until the DB commits, the events stay in the muxer's memory
queue, and past `event_queue_max_size` they spill into the retention file. That is the same
return-value contract an OTLP output has to honour (see `otlp_output_architecture.md`).

Finally, perfdata takes a second hop: `unified_sql` writes `data_bin`/`metrics` **and
republishes** `storage::pb_metric` / `pb_status` into the multiplexing engine via
`multiplexing::publisher().write(...)` (`broker/unified_sql/src/stream_storage.cc:124` and
`:365`), where the RRD muxer picks them up — which is why RRD graphs depend on `unified_sql`
being alive.
