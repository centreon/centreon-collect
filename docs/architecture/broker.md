# Broker architecture

Component file of [`ARCHITECTURE.md`](../../ARCHITECTURE.md). Conventions: [`../conventions/global.md`](../conventions/global.md), then [`../conventions/broker.md`](../conventions/broker.md). Tests: [`../testing.md`](../testing.md).

Centreon Broker moves monitoring events. On each poller, `cbmod` runs inside Engine and publishes Engine events; on the central, `cbd` receives them over BBDO and writes them to MariaDB, RRD files, BAM, Lua scripts and time-series databases. Both run the same core: endpoints, a multiplexing engine and muxers, loaded modules. Design notes (muxer, feeder, BAM, global cache, gRPC module): `broker/doc/broker-doc.md`.

## Scope

| Zone | Path | Notes |
|---|---|---|
| Core | `broker/core/src/`, `broker/core/inc/com/centreon/broker/`, `broker/core/multiplexing/`, `broker/core/bbdo/`, `broker/core/sql/`, `broker/core/cache/` | Configuration, endpoints, processing, BBDO stream, compression, retention files, MySQL layer, global cache. Linked into `cbd` and into `cbmod` |
| Engine module | `broker/neb/` (`cbmod.cc`, NEB events, module `10-neb.so`); Engine side in `engine/src/broker.cc`, `engine/src/main.cc` | Runs inside `centengine`. Producers and `cbmod` change together |
| Transports | `broker/tcp/` (`50-tcp`), `broker/grpc/` (`50-grpc`), `broker/tls/` (`60-tls`, GnuTLS); compression in `broker/core/src/compression/` | Stacked under the BBDO layer |
| Database outputs | `broker/unified_sql/` (`20-unified_sql`); `broker/sql/` (`80-sql`) and `broker/storage/` (`20-storage`) for BBDO 2 only | Write `centreon_storage` |
| Other outputs | `broker/rrd/` (`70-rrd`), `broker/bam/` (`20-bam`, types `bam` and `bam_bi`), `broker/lua/` (`70-lua`), `broker/graphite/`, `broker/influxdb/`, `broker/victoria_metrics/` (on the static `broker/http_tsdb/`), `broker/otlp/` (`70-otlp`), `broker/event_script/` (`80-event_script`), `broker/stats/` (`15-stats`) | One shared library each |
| Optional | `broker/simu/` (`WITH_MODULE_SIMU`, on in CI), `broker/generator/` (`90-generator`, off), `broker/tls2/` (`60-tls2`, OpenSSL, off) | Not in packages unless enabled |
| Watchdog | `broker/watchdog/` (`cbwd`) | Forks the `cbd` instances of `watchdog.json`, restarts them, forwards `SIGHUP` |
| Contracts | `bbdo/*.proto`, `bbdo/events.hh`, legacy event mappings (`broker/neb/src/*.cc`, `bbdo/bam/`, `bbdo/storage/`, `bbdo/bbdo/`); `broker/core/src/broker.proto` | Read by peers of other versions, by retention files, by Lua scripts, by `ccc` and the Robot tests |
| Shared libraries | `common/log_v2`, `common/src/pool.cc`, `common/grpc`, `common/crypto`, `common/vault`, `common/http`, `common/engine_conf` | Also used by Engine and the CMA: global rules only |
| Tests | `broker/**/test/`, `broker/test/` (`ut_broker`), `tests/broker/`, `tests/broker-engine/`, `tests/bam/`, `tests/resources/Broker.py` | See [`../testing.md`](../testing.md) |
| Packaging | `packaging/centreon-collect/centreon-broker*.yaml`, `selinux/centreon-broker/`, `broker/config/*.json.in`, `broker/script/` | |

Dependency direction: Broker depends on `common/`, `clib/`, `bbdo/` and third-party libraries. Exceptions to the global rule exist today (see [Known limits](#known-limits-of-the-current-design)): `broker/CMakeLists.txt` puts `engine/inc` on every Broker include path, and Engine includes `com/centreon/broker/neb/cbmod.hh` and links `cbmod` (`engine/CMakeLists.txt`, target `cce_core`).

## Layout and build

```
broker/
├─ CMakeLists.txt          cbd, rokerbase/roker, module list (add_broker_module)
├─ cmake/tool.cmake        add_broker_module(NAME default) → option WITH_MODULE_<NAME>
├─ core/                   core library, BBDO, multiplexing, sql, cache, ut sources
├─ neb/                    10-neb.so (NEB events) and cbmod.so (Engine side)
├─ <module>/               inc/com/centreon/broker/<module>/, src/, test/, precomp_inc/
├─ config/                 JSON templates (central-broker, central-rrd, central-module)
├─ test/                   ut_broker target (gtest), rpc_client
└─ doc/                    broker-doc.md, doc-otlp.md
```

- `cbd` (`broker/core/src/main.cc`) links the core with `--whole-archive` and `ENABLE_EXPORTS`, so that modules resolve core symbols from the executable.
- `cbmod.so` (`broker/neb/CMakeLists.txt`) links the same core (`rokerbase`, `multiplexing`) and is linked into Engine. Listing `cbmod.so` as an Engine `broker_module` is refused (`engine/src/broker/loader.cc`); Engine reads the Broker configuration file from `broker_module_cfg_file`.
- Modules are shared libraries named `<NN>-<name>.so` in `module_directory`. The configuration parser maps each endpoint `type` to a file (`config::parser::_parse_endpoint`), and `config::applier::modules::load_file` `dlopen`s it with `RTLD_GLOBAL`, then loads the files named by its `broker_module_parents()` first (for example `10-neb.so`, and `60-tls.so` for TCP).
- A module exports `broker_module_version`, `broker_module_init(const void*)`, `broker_module_deinit()` and optionally `broker_module_parents()` and `broker_module_update` (`modules/handle.hh`). Init registers its events (`io::events`) and its protocol factory (`io::protocols`); deinit unregisters them.
- Every source is listed by hand in the module's `CMakeLists.txt`; test sources are appended to `TESTS_SOURCES` and compiled into `ut_broker`. Generated files: `broker.pb.*`, `table_max_size.hh` (from `resources/centreon*.sql` by `core/sql/src/table_max_size.py`), `grpc_stream.proto` and its bridge (`broker/grpc/generate_proto.py`).
- `ONLY_ROBOT_CMA` builds only the modules the CMA Robot tests need and no `ut_broker`.

## Components and runtime flow

| Component | Files | Role |
|---|---|---|
| Configuration | `core/src/config/parser.cc`, `config/state.cc` | JSON `centreonBroker` object → `config::state`, endpoint list, module list |
| Appliers | `core/src/config/applier/{init,state,modules,endpoint}.cc` | Load singletons, modules, then build or diff endpoints |
| Protocol stack | `io::protocols`, `io::factory`, `io::endpoint`, `io::stream` | Layers registered with an OSI range: TCP 1–4, TLS 5, compression 6, BBDO 7, outputs 1–7 |
| Events | `io::data`, `io::protobuf<T, type>`, `io::events` | Typed events, registry by type id |
| Multiplexing | `core/multiplexing/` (`engine`, `muxer`, `muxer_filter`, `publisher`) | Fan-out of every event to every muxer whose filter accepts it |
| Processing | `core/src/processing/` (`failover`, `acceptor`, `feeder`) | Run endpoints: outputs with retry and failover, inputs per connection |
| Retention | `persistent_file`, `persistent_cache`, `file/splitter` | Disk queues; BBDO + compression on split files |
| Global cache | `core/cache/` | Memory-mapped cache of hosts, services, groups, BA (conf and real-time files), loaded by the Lua, OTLP and TSDB outputs |
| gRPC API | `core/src/brokerrpc.cc`, `broker_impl.cc`, `broker.proto` | Statistics, log levels, rebuild/remove graphs, remove poller |

```
centengine                                         cbd (central)
 engine/src/broker.cc ─ cbm->write()                 acceptor (input) ─► feeder ─► muxer.write
   └─ multiplexing::publisher                                                    │
        └─ multiplexing::engine ─► muxer(output) ─► failover thread              ▼
                                       └─ bbdo → compression → tls → tcp ══► engine.publish ─► muxer(unified_sql) ─► failover ─► unified_sql::stream
                                                                                     └─► muxer(rrd output) ─► failover ─► bbdo/tcp ─► cbd (rrd)
```

1. Engine calls `broker_*` functions (`engine/src/broker.cc`), which build an event and call `cbm->write()`. With BBDO ≥ 3 the event is protobuf (`cbmod::use_protobuf()`), otherwise legacy.
2. `multiplexing::engine::publish` queues the event; `_send_to_subscribers` writes it to the global cache first, then hands the batch to every muxer, one on the current thread and the others through `asio::post`.
3. A muxer keeps events in memory up to `event_queue_max_size`, then in its retention file `<cache_dir>.queue.<endpoint>`. Its read and write filters (`muxer_filter`, one 64-bit mask per category) decide what it accepts.
4. An output is a `failover`: one thread opens the endpoint stack, reads its muxer and writes to the stream; on error it waits `retry_interval`, can start a configured `failover` endpoint, and keeps events in retention. `write()`, `flush()` and `stop()` return the number of events the stream acknowledges, and the failover passes it to `muxer::ack_events`, which removes them from the queue.
5. An input is an `acceptor` (one thread) that creates a `feeder` per connection. A feeder polls the stream with `read(d, 0)` on a pool timer and publishes what it reads; events from the engine reach it through `muxer::data_handler::on_events`.
6. On the central, `unified_sql::stream::write` dispatches by category and element (`neb_processing_table` indexed by element id).

At stop, events still in the engine go to `<cache_dir>.unprocessed`, and each muxer's memory to `<cache_dir>.memory.<endpoint>`; both are read back at start. `cache_dir` is `<cache_directory>/<broker_name>`.

### Event types

A type is `category << 16 | element` (`make_type`, `bbdo/events.hh`). Categories: `neb` 1, `bbdo` 2, `storage` 3, `dumper` 5, `bam` 6, `extcmd` 7, `generator` 8, `local` 9, `internal` 65535. Elements are enums per category in `bbdo/events.hh`. Every element added since BBDO 3 is a `de_pb_*` protobuf event. `io::events::register_event` keeps the first registration of a type id; filters name events `category` or `category:name`.

## Threading model

| Thread | Created by | Runs |
|---|---|---|
| asio pool `pool_thread<N>` | `common::pool` (`common/src/pool.cc`); size `pool_size` or `cbd -s`, default `max(cores, 3)` | TCP and gRPC I/O, signals, multiplexing fan-out, feeders, muxer handlers, module timers |
| One per output | `processing::failover::_run` | Open, read muxer, write stream, retry |
| One per input | `processing::acceptor::_callback` | Accept connections, create feeders |
| One per MySQL connection | `mysql_connection` (`connections_count`) | Queued queries and statements, commits |
| Module workers | `stats::worker`, `bam::availability_thread`, `sql::cleanup`, `storage::conflict_manager` | Module specific |
| `cbd` main thread | `main.cc` | Start-up, then sleeps until `SIGTERM` |

In `cbmod`, Engine's threads call `cbm->write()`; the pool is the one Engine shares through `common::pool`. Shared state is guarded by `absl::Mutex` with thread annotations in new code (`multiplexing::engine`, `failover`), `std::mutex` or `std::timed_mutex` elsewhere. An exception that escapes a handler ends its pool thread: `pool.cc` logs it at CRITICAL and does not start a new one.

## Contracts

### BBDO

Pollers and the central are upgraded independently; retention files and Lua scripts also read events.

- **Negotiation** (`broker/core/bbdo/stream.cc`, `stream::negotiate`): peers exchange `version_response` (BBDO ≤ 3.0.0) or `pb_welcome` with their version and extensions (TLS, compression). A different major version closes the connection; the second peer downgrades to the lower minor/patch. `bbdo_version` defaults to 2.0.0 when absent (`core/bbdo/internal.hh`); the templates set 3.0.0. With BBDO ≥ 3, `sql` and `storage` are refused (`config::applier::state::apply`).
- **Protobuf events**: a message in `bbdo/*.proto` preceded by `/* io::<category>, <ns>::de_pb_<name>, <N> */`, wrapped as `io::protobuf<Message, make_type(...)>` and registered by its module. `N` is the field number in the gRPC `CentreonEvent` oneof generated by `generate_proto.py`; a message that is not an event carries `/* Ignore */`, or the build fails.
- **Legacy events**: classes with a `mapping::entry` table (`broker/neb/src/*.cc`, `bbdo/*/`) define the BBDO 2 wire order and the `sql` columns. `bbdo2_to_bbdo3()` converts them for the cache and BAM.
- **Lua**: `broker/lua/src/broker_event.cc` exposes protobuf events by reflection, with field names, `category` and `element`: a renamed field or renumbered element breaks customer scripts.
- **gRPC transport**: events travel as a BBDO buffer or directly in the `CentreonEvent` oneof (`broker/doc/broker-doc.md` § generate_proto.py).

The Engine side (`engine/src/broker.cc`, `broker/neb/`) and the consumers change in the same pull request; rules in [`../conventions/broker.md`](../conventions/broker.md#bbdo-events).

### Broker gRPC API

`broker/core/src/broker.proto`, service `Broker`, served by `brokerrpc` on `listen_address` (default `localhost`) and `rpc_port` (default `51000 + broker_id`), without TLS (`grpc::InsecureServerCredentials`). Used by `ccc` and by the Robot tests (`tests/init-proto.sh`). Evolves by addition.

## Configuration

- One JSON file per process, object `centreonBroker`: identity (`broker_id`, `broker_name`, `poller_id`, `poller_name`), `module_directory`, `cache_directory`, `pool_size`, `event_queue_max_size`, `event_queues_total_size`, `bbdo_version`, `log`, `grpc`, `input` and `output` arrays, `stats`. Other keys go to `config::state::params()`.
- An endpoint has `name` (required, characters checked), `type`, `filters` (`category`, `event`), `failover`, `buffering_timeout`, `retry_interval`, `read_timeout`, `cache`. Its other string values land in `config::endpoint::params`; the raw JSON stays in `config::endpoint::cfg`.
- The applier builds the stack: the factory with `osi_from == 1` that recognises the endpoint, then one layer per level up to 7 (`config::applier::endpoint::_create_endpoint`). Each factory may fill defaults (`set_default_values`) and declares mandatory and forbidden filters for its stream.
- Reload of `cbd` on `SIGHUP`: parse again, apply logs, apply the new state; on failure, re-apply the previous one. Unchanged endpoints are kept and `update()`d (`_diff_endpoints`), others are stopped and rebuilt. `modules::apply` only loads modules, it never unloads one. On an Engine reload, `cbmod::reload` only sends an `InstanceConfiguration` event; the Broker configuration of `cbmod` is read once.
- Credentials: `db_password` may be `encrypt::<aes256>` (keys from `/etc/centreon-engine/engine-context.json`) or a vault reference (`common::vault`); Lua parameters may be `encrypt::`.

## Database

- `unified_sql` writes `centreon_storage`. Broker never creates or migrates tables: its sources contain no DDL outside tests. The schema it is built and tested against is `resources/centreon.sql` and `resources/centreon_storage.sql`; column sizes come from there through `table_max_size.hh`.
- `mysql` (`broker/core/sql/`) owns `connections_count` connections, each with its thread and task queue. Statements use named placeholders (`:host_id`) bound with `bind_value_as_*_k`. Bulk inserts go through `database::bulk_or_multi`: MariaDB bulk statements when the server supports them, a multi-row insert otherwise.
- Transactions: with `queries_per_transaction > 1`, autocommit is off and a connection commits after that many queries or `max_commit_delay` seconds. `unified_sql` tracks the tables touched per connection (`actions`) and commits another connection before a dependent query (`_finish_action`); events are acknowledged only after commit.
- Per-poller queries mostly use `choose_connection_by_instance` (`instance_id % connections_count`); some tables have a fixed connection (`special_conn`).

## Logging

`common/log_v2`: one spdlog logger per area, `log_v2::instance().get(log_v2::<ID>)`: `core`, `config`, `bbdo`, `tcp`, `tls`, `grpc`, `processing`, `sql`, `perfdata`, `neb`, `rrd`, `bam`, `lua`, `influxdb`, `graphite`, `victoria_metrics`, `otl`, `event_script`, `stats`. Levels per logger in `log.loggers`; files under `log.directory`. In `cbmod`, Engine owns the log file and Broker only changes levels (`allow_only_atomic_changes`). `SetLogLevel` changes a level at run time.

## Security

- TCP transport: TLS through `60-tls` (GnuTLS), keys `tls`, `public_cert`, `private_key`, `ca_certificate`, `tls_hostname`, negotiated as a BBDO extension. gRPC transport: `encryption`, `certificate`/`public_cert`, `private_key`, `ca_certificate`, `ca_name`, and an `authorization` token (also accepted by TCP).
- MariaDB TLS: `db_ssl_enabled`, `db_ssl_ca`, `db_ssl_cert`, `db_ssl_key`, `db_ssl_verify_cert`, `db_tls_version` (`broker/doc/tls_database.md`).
- The gRPC API has no authentication and listens on `localhost` by default; it offers `Aes256Encrypt`/`Aes256Decrypt` with caller-provided keys.
- SQL: values reach the database through bound statements; formatted queries escape strings with `misc::string::escape` and truncate them to the column size.

## Changes that need the owner's agreement first

Ask before writing code for:

- a change to `bbdo/*.proto`, `bbdo/events.hh`, a legacy `mapping::entry` table, the BBDO negotiation, the gRPC oneof numbers, or `broker.proto` other than a new field or RPC;
- the multiplexing engine, muxer, failover or feeder, the retention file format, the acknowledgement logic;
- the SQL connection, transaction or conflict handling of `unified_sql`, or a query on `resources`, `services`, `hosts`, `logs`, `data_bin`;
- a new thread, a new module, a new `vcpkg.json` dependency;
- TLS, credentials, vault or the gRPC API exposure;
- a change under `common/` (shared with Engine and the CMA).

## Known limits of the current design

- One 64-bit mask per category in `muxer_filter`: element ids must stay below 64 (`neb` already uses 59).
- An exception escaping a pool handler loses that thread for the life of the process.
- Engine and Broker include each other's headers (`engine/inc` on the Broker include path, `cbmod.hh` in Engine globals); the SQL modules include `com/centreon/engine/host.hh`.
- `cbd` and its modules are singletons (`config::applier::state`, `multiplexing::engine`, `io::events`, `mysql_manager`), which the unit tests share.
- `stats_exporter` has sources and a parser entry (`15-stats_exporter.so`) but is not added to the build.
- The gRPC API is unauthenticated.
