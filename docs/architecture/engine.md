# Engine architecture

Component file of [`ARCHITECTURE.md`](../../ARCHITECTURE.md). Conventions: [`../conventions/global.md`](../conventions/global.md), then [`../conventions/engine.md`](../conventions/engine.md). Tests: [`../testing.md`](../testing.md).

Centreon Engine (binary `centengine`) is the poller scheduler, a fork of Nagios 3. It reads the poller configuration, schedules host and service checks, runs them (plugins, connectors, OpenTelemetry/CMA), computes states (HARD/SOFT), sends notifications, runs event handlers and keeps downtimes, comments, acknowledgements and retention. Every state change is written to Broker through `cbmod`, linked into the process. Design notes (whitelist, extended configuration, OpenTelemetry): `engine/doc/engine-doc.md`, `engine/modules/opentelemetry/doc/opentelemetry.md`.

## Scope

| Zone | Path | Notes |
|---|---|---|
| Core | `engine/src/`, `engine/inc/com/centreon/engine/` | Scheduler, objects, checks, notifications, retention, macros, external command implementations |
| Nagios compatibility | `engine/src/compatibility/`, `engine/inc/compatibility/` | C headers (`*.h`) and globals kept for NEB modules |
| Configuration model | `common/engine_conf/` (`state.proto`, `parser.cc`, `*_helper.cc`) | Owned by Engine, compiled as `centreon_engine_conf`; also parsed by Broker `unified_sql` (`broker/unified_sql/src/stream_sql.cc`) |
| Configuration appliers | `engine/src/configuration/applier/`, `engine/src/configuration/{whitelist,extended_conf}.cc` | Turn a `configuration::State` into live objects |
| Modules | `engine/modules/external_commands/` (`externalcmd.so`), `engine/modules/opentelemetry/` (`libopentelemetry.so`) | Loaded by `broker_module=` lines through the NEB loader |
| CMA side of the OpenTelemetry module | `engine/modules/opentelemetry/**/centreon_agent/` | Governed by [`cma.md`](cma.md), not by this file |
| gRPC API | `engine/enginerpc/` (`engine.proto`, `engine_impl.cc`, `enginerpc.cc`) | Contract used by `ccc` and the Robot tests |
| Broker bridge | `engine/src/broker.cc`, `engine/inc/com/centreon/engine/broker.hh`; `broker/neb/` (`cbmod`) | Engine fills BBDO events; `cbmod` is Broker code |
| Tests | `engine/tests/` (`ut_engine`), `tests/engine/`, `tests/broker-engine/` (except `cma*`), `tests/resources/Engine.py` | See [`../testing.md`](../testing.md) |
| Packaging | `packaging/centreon-collect/centreon-engine*.yaml`, `selinux/centreon-engine/`, `engine/scripts/`, `engine/conf/` | |

Engine depends on `common/`, `clib/`, `bbdo/` and on `broker/neb` + `broker/core` headers for `cbmod` (`engine/CMakeLists.txt` adds both include directories and links `cce_core` to `cbmod`). It reaches the agent through `agent.proto` only.

**Reference code for new work:** `command_manager.cc`, `commands/raw_v2.cc`, `commands/otel_connector.cc`, `configuration/whitelist.cc`, the protobuf appliers in `configuration/applier/`, `enginerpc/engine_impl.cc`. **Legacy, not a model:** the Nagios-derived GPL files (`commands/commands.cc`, `commands/processing.cc`, `events/`, `macros/`, `nebmods.cc`, `compatibility/`, `retention/`, `xsddefault.cc`), the `engine_logger(...) <<` stream logging and the `engine_error() <<` exceptions.

## Layout and build

```
engine/
├─ src/, inc/com/centreon/engine/   core (cce_core) + main.cc (centengine)
│   ├─ configuration/applier/        State → live objects, reload
│   ├─ checks/  commands/            checker (reaper), raw_v2, connector, forward, otel_connector
│   ├─ events/                       timed events and main loop
│   ├─ downtimes/ retention/ macros/ logging/ broker/ (NEB loader)
│   └─ compatibility/                Nagios C API
├─ modules/external_commands/        externalcmd.so: command file (FIFO) reader
├─ modules/opentelemetry/            libopentelemetry.so: OTLP server, Telegraf, CMA
├─ enginerpc/                        engine.proto, gRPC service
├─ tests/                            ut_engine (gtest)
├─ precomp_inc/precomp.hh            precompiled header
└─ conf/, scripts/, doc/
```

| Target | Kind | Defined in |
|---|---|---|
| `cce_core` | static library (shared with `WITH_SHARED_LIB`) | `engine/CMakeLists.txt`, sources in `FILES` extended by each `src/*/CMakeLists.txt` |
| `centengine` | executable (`main.cc`, `ENABLE_EXPORTS` so that modules resolve Engine symbols) | `engine/CMakeLists.txt` |
| `centenginestats` | executable | `engine/CMakeLists.txt` |
| `enginerpc`, `cerpc` | static libraries (service, generated code) | `engine/enginerpc/CMakeLists.txt` |
| `externalcmd`, `opentelemetry` | shared modules, installed in `lib64/centreon-engine/` | `engine/modules/*/CMakeLists.txt` |
| `centreon_engine_conf` | shared library (Linux only) | `common/engine_conf/CMakeLists.txt` |
| `ut_engine`, `rpc_client_engine`, `bin_connector_test_run` | tests (`WITH_TESTING`, not with `ONLY_ROBOT_CMA`) | `engine/tests/CMakeLists.txt` |
| `simumod` | test NEB module (`WITH_SIMU`) | `engine/src/simumod/` |

- Every source is listed by hand: core files in `engine/CMakeLists.txt` or the `src/<dir>/CMakeLists.txt` of their directory (`PARENT_SCOPE` lists), tests in `ut_sources` of `engine/tests/CMakeLists.txt`. A file not listed is not built.
- `WITH_DEBUG_CONFIG` adds pointer consistency checks after each apply (`applier::state::_check_*`); developers only.
- `engine.proto` is generated into the build tree; `agent.proto` and the OTLP protos are generated into `modules/opentelemetry/src/`.

## Components and runtime flow

| Component | Files | Role |
|---|---|---|
| Entry point | `src/main.cc` | Options (`-v`, `-s`, `-c` extended JSON, `-k` CMA CA), parse, start enginerpc, load modules, apply, create `cbm`, run the loop, save retention on exit |
| Main loop | `events/loop.cc`, `events/timed_event.cc` | Two time-ordered lists (`high`, `low`) of `timed_event`; reload; drains `command_manager` |
| Scheduling | `configuration/applier/scheduler.cc`, `host.cc`, `service.cc` | Initial spreading (inter-check delay, interleave), recurring events (reaper, retention save, status save, orphan, freshness, expirations) |
| Check execution | `checkable.cc`, `service::run_async_check`, `host::run_async_check`, `commands/` | Macros, whitelist, then `command::run`: `raw_v2` (process through `common::process` on `g_io_context`), `forward` to a `connector` (Perl/SSH) or to an `otel_connector` |
| Result reaping | `checks/checker.cc` | `finished()` (any thread) queues results under `_mut_reap`; `reap()` (event `EVENT_CHECK_REAPER`) hands them to `handle_async_check_result` on the loop |
| State computation | `service::handle_async_check_result`, `host::process_check_result_3x`, `flapping.cc` | Attempts, HARD/SOFT, dependencies, flapping (Nagios 3 semantics) |
| Notifications, handlers | `notifier.cc`, `notification.cc`, escalations, `sehandlers.cc` | Contacts, escalations, event handlers, OCSP/OCHP |
| Downtimes, comments, acks | `downtimes/downtime_manager.cc`, `comment.cc` (`comment::comments`), `notifier` acknowledgement state | Scheduled and expired by timed events; also mirrored in `cbm` |
| Retention and status | `retention/` (`parser`, `applier`, `dump`), `statusdata.cc`, `xsddefault.cc` | `retention.dat` read at start, written by `EVENT_RETENTION_SAVE` and on shutdown; `status.dat` |
| External commands | `commands/processing.cc` (table `_lst_command`), `commands/commands.cc` (`cmd_*`), `modules/external_commands/` | FIFO reader thread → `external_command_buffer` → executed on the loop |
| Command queue | `command_manager.cc` | `std::packaged_task` queue executed by the loop; used by enginerpc and the OpenTelemetry module |
| Broker bridge | `broker.cc` | `broker_*()` build legacy `neb::*` or protobuf `neb::pb_*` events (choice: `cbm->use_protobuf()`) and `cbm->write()` them |
| NEB modules | `nebmods.cc`, `broker/loader.cc`, `broker/handle.cc` | `dlopen` of `broker_module` entries (any `cbmod.so` entry is skipped), callback registry, `neb_make_callbacks` |

```
main.cc
  └─ parser (common/engine_conf) → State ─ extended_conf (JSON) ─ applier::state::apply
  └─ enginerpc (gRPC thread pool) ──────────────┐
  └─ externalcmd.so (FIFO worker thread) ───────┤ command_manager / external_command_buffer
  └─ libopentelemetry.so (gRPC, asio) ──────────┘
  └─ events::loop::run()  (main thread)
        ├─ EVENT_SERVICE_CHECK / HOST_CHECK → command::run → common::process (g_io_context)
        │                                        └─ checker::finished → _to_reap_partial
        ├─ EVENT_CHECK_REAPER → handle_async_check_result → notify / handlers / broker_*()
        ├─ command_manager::execute()   (queued tasks)
        └─ broker_*() → cbm->write() → cbmod (Broker code, same process)
```

## Threading model

The Nagios core is single-threaded: objects (`host::hosts`, `service::services`, timeperiods, commands, downtimes, comments) and `pb_config` are touched from the main loop. Each loop iteration holds `applier::state::lock()`.

| Thread | Work | Boundary with the loop |
|---|---|---|
| Main thread | `events::loop::_dispatching()` | — |
| `common::pool` threads running `g_io_context` (at least 1, `pool::set_pool_size`) | Process I/O and completion, `cbmod`/Broker I/O, asio timers of modules | Check results through `checker::finished` (mutex queue) |
| enginerpc gRPC sync server threads | `engine_impl::*` | `command_manager::enqueue` a `packaged_task`, then `future.get()` when a response is needed |
| OpenTelemetry module (gRPC, asio) | OTLP / CMA / Telegraf servers | `command_manager::enqueue`; `checker::add_check_result_to_reap` |
| External command worker (`modules/external_commands/src/utils.cc`) | Reads the FIFO | Pushes to `external_command_buffer`, processed on the loop by `check_for_external_commands()`; commands flagged `thread_safe` (`PROCESS_SERVICE_CHECK_RESULT`, `PROCESS_HOST_CHECK_RESULT`) are executed on the worker itself |
| Reload (`std::async` in `loop::_dispatching`) | `apply_conf`: parse, diff, apply | Apply section under `_apply_lock` |

`command_manager::execute()` runs from the idle branches of the loop and, when the loop stays busy, at least every 5 s (`is_execution_overdue`).

## Contracts

| Contract | Files | Consumers |
|---|---|---|
| BBDO events written by Engine | `engine/src/broker.cc`, `bbdo/neb.proto`, `bbdo/events.hh`, `broker/neb/inc/com/centreon/broker/neb/internal.hh` | Broker (`unified_sql`, `lua`, `bam`, ...) of any supported version |
| enginerpc | `engine/enginerpc/engine.proto` (package `com.centreon.engine`, service `Engine`) | `ccc`, `tests/resources/Engine.py` (stubs from `tests/init-proto.sh`), `engine/tests/enginerpc/client.cc` |
| NEB API | `nebcallbacks.hh`, `nebstructs.hh`, `nebmodules.hh`, `inc/compatibility/*.h` | `externalcmd.so`, `libopentelemetry.so`, third-party modules |
| Configuration syntax | `common/engine_conf/state.proto` and the parser | Files generated by Centreon Web, deployed by Gorgone; Broker `unified_sql` parses the same files |
| External commands | `processing.cc` names and argument order | Centreon Web, Gorgone, scripts writing to `centengine.cmd` |
| Retention file | `retention/dump.cc`, `retention/*.cc` setters | The next Engine, possibly of another version; unknown keys are ignored |
| CMA and OTLP | `engine/modules/opentelemetry/**/centreon_agent/` | See [`cma.md`](cma.md) |

`event_broker_options` (bit mask in `broker.hh`) filters which `broker_*()` send anything; `cbm` is null during `-v`/`-s` runs and before the loop starts, and every `broker_*()` checks it.

## Configuration

- **Read.** `configuration::parser::parse` (`common/engine_conf/parser.cc`) reads `centengine.cfg`, its `cfg_file`/`cfg_dir` and `resource_file`s into one `configuration::State` protobuf. Keys are mapped by protobuf reflection: key name = field name, aliases in the helper's correspondence table, special parsing in `hook()`, defaults in `_init()`, object checks in `check_validity()`. Templates are resolved in the parser. An unknown global key is logged at ERROR and ignored.
- **Extend.** `-c file.json` (repeatable) merges a JSON `State` over the parsed one at start and on each reload (`extended_conf::update_state`).
- **Apply.** `applier::state::apply`: expand objects, compute a `pb_difference` per object type, then under `_apply_lock` apply logging, globals (copied field by field into `pb_config`, mirrored into legacy globals by `applier::globals`), macros, then each type through its applier (`modify_object`, `remove_object`, `add_object`, `resolve_object`), retention (first load), scheduler, credentials key (`/etc/centreon-engine/engine-context.json`), whitelist. On failure after the first load, the saved configuration is re-applied.
- **Reload.** `SIGHUP` or a restart command sets `sighup`; the loop re-parses `cfg_main` and applies the difference. Broker modules, module directory, command file, external command buffer size and `use_timezone` cannot change on reload (warning). After a reload `cbm->reload()` and `neb_reload_all_modules()` run.
- **Verify.** `centengine -v` parses and applies with a default `cbmod`, prints counts and exits with the error count.

## Logging

- spdlog loggers from `common/log_v2` (`log_v2::load("centengine")`), held in globals set by `init_loggers()`: `checks_logger`, `commands_logger`, `config_logger`, `downtimes_logger`, `eventbroker_logger`, `events_logger`, `external_command_logger`, `functions_logger`, `macros_logger`, `neb_logger`, `notifications_logger`, `process_logger`, `runtime_logger`, `otl_logger`. Levels per logger come from `log_level_<name>` in `centengine.cfg`; output from `log_v2_logger` (`file` or syslog) and `log_file`.
- With `log_v2_enabled` (default), a `broker_sink_mt` at INFO is attached to every Engine logger: INFO and above also go to Broker as log entries (the Centreon event log).
- The legacy `engine_logger(type, verbosity) <<` stream (`logging/logger.hh`, clib `logging::engine`) is still called next to most spdlog calls; it only writes when `log_legacy_enabled` is set (default off).

## Security model

Engine runs as `centreon-engine` on the poller and executes commands built from a configuration written by the central server.

- **Command whitelist.** YAML/JSON rules in `/etc/centreon-engine-whitelist` and `/usr/share/centreon-engine/whitelist.conf.d` (`whitelist::reload`). When rules exist, checks, notifications, event handlers and obsess commands run only if their processed command line matches (`checkable::command_is_allowed_by_whitelist`); a separate rule set applies to CMA commands.
- **Process execution.** `raw_v2` splits the command line with `common::process::parse_cmd_line` and starts it through `common::process`; macros are also exported as environment variables (`enable_environment_macros`).
- **Secrets in macros.** `$USERn$` come from `resource_file`. With a valid `engine-context.json`, a macro value starting with `encrypt::` is decrypted (AES-256, `credentials_decrypt`) at macro resolution (`macros/grab_value.cc`), `raw::` is stripped. Processed command lines therefore contain plain-text secrets.
- **Local interfaces.** The command FIFO (`command_file`) and enginerpc both accept unauthenticated commands. enginerpc listens with `InsecureServerCredentials` on `rpc_listen_address` (default `localhost`) and `grpc_port` (random in 50000–50999 when 0).
- **CMA CA.** `centengine -k` generates a self-signed CA and key for agent TLS (`main.cc::gen_cma_key`).

## Changes that need the owner's agreement first

Ask before writing code for:

- a change to `state.proto` semantics or the `.cfg` syntax (Centreon Web, Gorgone and Broker `unified_sql` depend on it), to external command names or arguments, or to the retention file keys;
- a change to `engine.proto`, to a BBDO event sent by `broker.cc`, or to the NEB API;
- a change to the loop, the reload sequence, `checker`, HARD/SOFT computation, notification or downtime logic;
- a new thread, a new access to Engine objects outside the loop, a new `vcpkg.json` dependency;
- anything in the security model above;
- a change under `common/engine_conf/` or elsewhere in `common/` (shared code), or in `broker/neb/` (`cbmod`, owned by Broker).

## Known limits of the current design

- The loop polls: when idle it sleeps `sleep_time`, so queued tasks and results wait up to that delay.
- The reload is started with `std::async`, but the future is destroyed at the end of its block: the loop waits for the whole reload, and the "Already reloading" branch is never reached.
- Engine includes and links Broker code (`cbmod`); a change in `broker/neb` rebuilds and can break Engine.
- Configuration is held twice: `pb_config` and the legacy globals copied by `applier::globals`. The protobuf setters in `applier::state::_apply` list every global field by hand.
- Logging is duplicated (`engine_logger` stream and spdlog) in most legacy files.
- `broker.cc` keeps two encodings of each event (legacy BBDO and protobuf).
- enginerpc and the command FIFO have no authentication; whoever can reach them can run any external command.
