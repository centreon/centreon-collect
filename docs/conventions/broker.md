# Broker development conventions

Component file of [`global.md`](global.md), which applies first. Architecture: [`../architecture/broker.md`](../architecture/broker.md). Tests: [`../testing.md`](../testing.md).

These rules apply to `broker/` and to the BBDO contract seen from Broker (`bbdo/`, `engine/src/broker.cc`). In `common/` and `clib/`, only the global conventions apply. CMA rules never apply here.

## Files, names, build

- A module lives in `broker/<module>/` with `inc/com/centreon/broker/<module>/`, `src/`, `test/` and `precomp_inc/precomp.hpp` (set by `target_precompile_headers`). Namespace `com::centreon::broker::<module>`.
- Header guard: `#ifndef CCB_<MODULE>_<NAME>_HH` (`CCB_NEB_CBMOD_HH`, `CCB_IO_STREAM_HH`, `CCB_UNIFIED_SQL_STREAM_HH`).
- Include order: matching header, Broker headers (`com/centreon/broker/...`), `bbdo/...` generated headers, `com/centreon/common/...`, then third-party.
- List every source in the module's `CMakeLists.txt`. Unit tests are appended to `TESTS_SOURCES` with `PARENT_SCOPE` inside `if(WITH_TESTING)`, and the module library to `TESTS_LIBRARIES`; they build into `ut_broker`.
- A new module: a directory, an `add_broker_module(<NAME> <ON|OFF>)` line in `broker/CMakeLists.txt`, an output name `<NN>-<name>` with `PREFIX ""` installed in `PREFIX_MODULES`, the `type` → file mapping in `config::parser::_parse_endpoint`, and a package in `packaging/centreon-collect/` when it ships separately. The two-digit prefix orders loading: keep `10-neb` first.
- Module entry points in `src/main.cc`, `extern "C"`: `broker_module_version`, `broker_module_init`, `broker_module_deinit`, and `broker_module_parents` returning the modules to load first (`"10-neb.so"` when NEB events are used). Guard with a static instance counter as the existing modules do; init registers events then the protocol, deinit unregisters both.

## Streams, endpoints, events

- An event is an `io::data`; new events are `io::protobuf<Message, make_type(io::<category>, <ns>::de_pb_<name>)>`, declared with a `using pb_<name> = ...;` alias in the module's `internal.hh` or `events.hh`. Events travel as `std::shared_ptr<io::data>`; never keep a raw pointer to one after `write()` returns.
- A stream derives from `io::stream` and passes its name to the constructor. `read(d, deadline)` follows the contract of `io/stream.hh`: event → `true`; nothing yet → `true` with null `d`; timeout → `false`; end → throw `exceptions::shutdown`. Called from a feeder with deadline 0, it must not block.
- `write()`, `flush()` and `stop()` return the number of events now safely stored or sent, counted once. Return 0 until the data is committed (pattern: `unified_sql::stream::_ack` after `_finish_actions`); a lost acknowledgement duplicates events, an early one loses them.
- `stop()` flushes, stops the stream's timers and workers, and returns the final acknowledged count; it is called before destruction.
- A factory implements `has_endpoint` (match on `cfg.type`, fill `io::extension` if negotiated), `set_default_values` for missing keys, and `new_endpoint`. An endpoint declares the categories its stream needs (mandatory filter) and cannot accept (forbidden filter) through the `io::endpoint` constructor; build them as `constexpr muxer_filter` like `unified_sql::stream::get_muxer_filter()`.
- Errors in a stream: throw `msg_fmt` naming the endpoint or peer; `exceptions::connection_closed` when the peer goes away; `exceptions::shutdown` for a normal end. The failover and the feeder catch these and retry or stop; never catch them inside the stream to hide a lost connection.
- Asynchronous handlers (asio timers, sockets, gRPC callbacks) capture `shared_from_this()`, return at once on an error code, and catch every exception: one that escapes ends a pool thread for good.

## BBDO events

Adding a protobuf event, in one pull request:

1. Message in `bbdo/<area>.proto`, preceded by `/* io::<category>, <ns>::de_pb_<name>, <N> */`, where the message name is `<name>` in PascalCase and `N` is the next unused number of the gRPC oneof (`generate_proto.py` checks both and fails the build otherwise). A helper message that is not an event carries `/* Ignore */`.
2. `de_pb_<name>` at the end of its enum in `bbdo/events.hh`, below 64.
3. The `pb_<name>` alias and its `register_event(type, "<Name>", &pb_<name>::operations, "<table>")` in the module's `broker_module_init` (`broker/neb/src/broker.cc` for NEB events). Names must be unique: the first registration wins.
4. Producer: `engine/src/broker.cc` (through `cbm->write()`), only when `cbm->use_protobuf()`, or the Broker module that creates it.
5. Consumers: an entry in `unified_sql::stream::neb_processing_table` at the element's index (`nullptr` if ignored), the global cache when outputs need it (`broker/doc/broker-doc.md` § Adding a new proto class to the cache), BAM if it is a BAM input.
6. Unit test of the serialisation and a Robot test of the producer–consumer path.

Rules:

- Fields: add only, with new numbers; never renumber, retype, rename or reuse one; declare removed ones `reserved`. Field names are public to Lua scripts.
- Never renumber an element or a oneof number, never reuse one. A removed element stays in the enum as a comment (pattern: `// de_pb_module = 43, Not used`).
- Legacy events (`mapping::entry` tables) are frozen: no new legacy event, no change to an existing table.
- A new event must be harmless to an older peer of the same major version: an unregistered event is not decoded by the BBDO stream (`cannot unserialize event of ID`), so do not make existing data depend on it.
- Free text placed in a message goes through `common::check_string_utf8` (pattern: `cbmod::add_downtime`).

## Configuration keys

- Endpoint key: read from `cfg.params` (string values only; numbers and booleans are strings in the JSON, as in `broker/config/*.json.in`), with a default in `set_default_values` or in the factory, and a clear `msg_fmt` naming the endpoint and the key on an invalid value. Structured values are read from `cfg.cfg` (raw JSON).
- Global key: a branch in `config::parser::parse`, a member in `config::state`, a test in `broker/core/test/config/parser.cc`.
- Every new key has a default that keeps the previous behaviour. The Robot templates (`ctn_config_broker` in `tests/resources/Broker.py`) and the JSON templates are updated when the default is not enough.
- A secret value accepts the `encrypt::` prefix and vault references as `db_password` does (`database_config.cc`).

## SQL

- Queries with values use prepared statements: `mysql::prepare_query` with named placeholders (`:host_id`), `bind_value_as_*_k`, `bind_null_*_k`, `mysql::run_statement`. Bulk inserts use `database::bulk_or_multi`, which handles servers without bulk support.
- Never concatenate a value into SQL. Existing formatted queries go through `misc::string::escape` with the column size; do not add new ones.
- Truncate strings to the column size from `table_max_size.hh` (`get_<db>_<table>_col_size`) with `common::truncate_utf8`, never `substr`.
- Choose the connection like the surrounding code (`choose_connection_by_instance`, `special_conn`), mark the tables touched with `_add_action`, and commit a dependent connection first with `_finish_action`. Acknowledge events only after their commit.
- Never block a pool thread on a query result: `run_*_and_get_*` answers through a `std::promise`; wait on its future only from the output's failover thread, where `unified_sql::stream::write` runs.
- Broker contains no DDL: a query that needs a new column or index needs a schema change agreed with the owner first, and `resources/*.sql` is updated in the same pull request so that `table_max_size.hh` and the tests follow.

## Threading

- Use the shared pool (`common::pool::io_context()`) for asynchronous work. A new `std::thread` needs the owner's agreement; existing ones are listed in the architecture file.
- New shared state: `absl::Mutex` with `ABSL_GUARDED_BY` / `ABSL_LOCKS_EXCLUDED`, as in `multiplexing::engine` and `failover`.
- Global cache: hold a `global_cache::lock` while using a pointer it returned, never while calling `write()`, and handle `interprocess::bad_alloc` outside any lock (`global_cache.hh`).
- `processing::failover` and `acceptor` own their thread; stream code never creates the thread that drives it.

## Logging

- Logger of the area: `log_v2::instance().get(log_v2::<ID>)`, stored as `_logger` (or `_logger_<area>`) at construction: `SQL` for `unified_sql`, `PERFDATA` for metrics, `NEB` in `cbmod`, `TCP`, `TLS`, `GRPC`, `BBDO`, `PROCESSING`, `RRD`, `BAM`, `LUA`, `OTL`, `GRAPHITE`, `INFLUXDB`, `VICTORIA_METRICS`, `EVENT_SCRIPT`, `STATS`; `CORE` and `CONFIG` for the core.
- `SPDLOG_LOGGER_<LEVEL>` macros. INFO for lifecycle (module loaded, endpoint created, connection opened, poller stopped), WARN for deprecated configuration, ERROR for a lost connection or a failed query. Per-event lines are DEBUG or TRACE; format an event with `{}` (its `fmt::formatter`), only under `_logger->level() <= spdlog::level::trace` when it is large.
- Prefix messages with the module and name the endpoint, peer, poller id or host/service ids. Robot tests grep log lines (`Ctn Find In Log`): check `tests/` before changing a wording.

## Known deviations — do not copy

The code below predates these conventions. Do not use it as a model, and fix it only in a dedicated pull request.

| Place | Deviation |
|---|---|
| `cbmod::write` (`broker/neb/src/cbmod.cc`) | Logs every event at INFO (`cbmod: write event of type`) in Engine's log |
| `unified_sql::stream::_process_custom_variable`, `_process_pb_custom_variable` | One INFO line per custom variable |
| `broker/unified_sql/src/main.cc` | `bbdo::de_remove_poller` registered twice, first under the name `remove_graphs`; the first registration wins |
| `broker/core/bbdo/internal.cc` | `bbdo::load()` / `unload()` never called; duplicate the registrations of `io::events::events()` |
| `broker/unified_sql/bulk_queries.hh` | Stray copy, different from `inc/.../bulk_queries.hh`, not built |
| `broker/neb/inc/com/centreon/{exceptions,io,logging,misc}/` | Old copies of `clib` headers, different from `clib/inc`, on the Broker include path |
| `broker/CMakeLists.txt` (`engine/inc`), `sql/src/stream.cc`, `storage/src/conflict_manager_sql.cc`, `unified_sql/src/stream_sql.cc` | Broker modules include Engine headers |
| `unified_sql/src/stream_sql.cc` (comments, downtimes, logs) | Strings cut with `substr` (bytes) instead of `common::truncate_utf8` |
| `unified_sql/src/stream_sql.cc` (`_cv`, `_cvs` and other bulk queries) | Values formatted into SQL with `misc::string::escape` |
| `config::applier::endpoint` | Raw `processing::endpoint*` in `_endpoints`, `release()->start()` and manual `delete` |
| `brokerrpc.hh`, `broker_impl.hh`, `stats/helper.hh`, `tcp/tcp_async.hh`, `tcp/tcp_connection.hh`, `exceptions/config.hh`, `misc/buffer.hh`, `misc/fifo_client.hh`, `bam/configuration/bool_expression.hh` | Header guards outside `CCB_<MODULE>_<NAME>_HH` |
| `rrd/cached.hh`, `rrd/exceptions/open.hh`, `rrd/exceptions/update.hh` | `using namespace` in a header |
| `processing/failover.hh`, `time/*.hh`, `ceof/*.hh`, `stats/builder.hh`, `stats/parser.hh`, `sql/cleanup.hh` | Dynamic exception specification `throw()` |
| `broker_module_deinit` in `broker/neb/src/broker.cc`; `unified_sql/stream.hh` (`// bool _exit;`) | Commented-out code |
| Many files (`modules.cc`, `cbmod.cc`, `engine.cc`...) | `_logger->info(...)` method calls instead of `SPDLOG_LOGGER_*` macros |
| `broker/stats_exporter/` | Not in the build, while `config::parser` adds `15-stats_exporter.so` |
| `broker/CMakeLists.txt` | `add_broker_module(DUMP OFF)` without a `broker/dump/` directory |
