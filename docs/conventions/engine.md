# Engine development conventions

Component file of [`global.md`](global.md), which applies first. Architecture: [`../architecture/engine.md`](../architecture/engine.md). Tests: [`../testing.md`](../testing.md).

These rules apply to `engine/` and `common/engine_conf/`, except `engine/modules/opentelemetry/**/centreon_agent/`, which follows [`cma.md`](cma.md). In the rest of `common/`, only the global conventions apply.

## Language and legacy code

- C++17, Linux only. Nagios-derived code still uses C idioms (`char*` arguments, `my_strtok`, `NULL`, `OK`/`ERROR` return codes, global variables). Do not extend them: new code takes `std::string_view` / `const std::string&`, parses with `absl::StrSplit`, returns `bool` or throws.
- A file with a GPL v2 header is Nagios-derived: keep its header, do not copy its style. New files are Apache 2.0 (model: `src/commands/otel_connector.cc`).
- List every new source in the `CMakeLists.txt` of its directory (`FILES ... PARENT_SCOPE`) and every new test in `ut_sources` of `engine/tests/CMakeLists.txt`.
- New third-party headers go to `engine/precomp_inc/precomp.hh` (module: its own `precomp_inc/precomp.hh`).

## Naming and files

- Namespace `com::centreon::engine`, sub-namespaces by directory (`configuration::applier`, `commands`, `checks`, `events`, `downtimes`, `retention`, `logging`). Modules: `com::centreon::engine::modules::<module>`. Do not add functions to the global namespace; the existing global `broker_*()`, `cmd_*()` and Nagios functions stay where they are.
- Header guard: `CCE_<PATH>_<NAME>_HH` (core: `CCE_CHECKS_CHECKER_HH`); modules: `CCE_MOD_<MODULE>_<NAME>_HH` (`CCE_MOD_EXTCMD_UTILS_HH`, `CCE_MOD_OTL_...`).
- Public headers in `engine/inc/com/centreon/engine/<dir>/`; module headers in `engine/modules/<module>/inc/com/centreon/engine/modules/<module>/`.
- Include order: matching header, `com/centreon/engine/...`, then `com/centreon/common/...` or `common/...` (both forms are in use; follow the neighbouring file), then `com/centreon/exceptions/...` and clib.

## Objects and ownership

- Live objects are owned by static maps of `shared_ptr`: `host::hosts` / `hosts_by_id`, `service::services` / `services_by_id`, `commands::command::commands`, `commands::connector::connectors`, `timeperiod::timeperiods`, `comment::comments`. Only appliers (`configuration/applier/`) insert into or erase from them.
- Links between objects (`check_period_ptr`, `_event_handler_ptr`, `_notification_period_ptr`, `service::_host_ptr` as `weak_ptr`) are set in `resolve_object` and rebuilt on each reload. Never keep such a pointer outside the object graph across a loop iteration.
- A notifier that goes away must not leave results behind: `notifier::~notifier` calls `checker::forget(this)`; keep that path when changing object removal.
- Read configuration from `pb_config` in new code, not from the legacy globals mirrored by `applier::globals`.

## Threading

- Touch Engine objects, `pb_config`, downtimes, comments and macros from the main loop only.
- From another thread (gRPC, asio, module thread), wrap the work in a `std::packaged_task<int()>` and `command_manager::instance().enqueue()` it; wait on its future only from a thread that is not the loop. Model: `engine_impl::GetHost`.
- Check results produced off the loop go through `checker::finished` or `checker::add_check_result_to_reap` (queues under `_mut_reap`), never directly into `handle_async_check_result`.
- Never block the loop: no synchronous network call, no `future::get()`, no wait on another thread. Processes run asynchronously through `common::process` on `g_io_context` (`raw_v2`), not through `system()` or `popen()`.
- Do not mark a new external command `thread_safe` in `processing.cc`.

## Adding a configuration directive

In the same change:

1. Field in `common/engine_conf/state.proto` (`State` for a global, `Host`, `Service`... for an object), with a new number. The `.cfg` key is the field name.
2. In the matching helper (`state_helper.cc`, `host_helper.cc`...): default in `_init()` when not the zero value, old names in the correspondence table, special syntax in `hook()`, cross-field checks in `check_validity()`.
3. Global: copy it in `applier::state::_apply(const configuration::State&, error_cnt&)` (fields are copied one by one); if it cannot change on reload, add the warning there. Object: set it in `add_object` and `modify_object` of its applier and on the engine object.
4. If Broker needs it: the `broker_*()` event (both encodings) and the BBDO field (see below). If it must survive a restart: retention (see below).
5. Unit test in `engine/tests/configuration/` (parsing, default) and, when it changes runtime behaviour, a Robot test.

`state.proto` is also parsed by Broker `unified_sql`: a field is only added, never renumbered or retyped.

## Adding an external command

1. A `CMD_<NAME>` id in `engine/inc/com/centreon/engine/common.hh` (new number).
2. An entry in `processing::_lst_command` (`src/commands/processing.cc`) with a `_redirector*` wrapper; not `thread_safe`.
3. The implementation, in `commands.cc` or near the object it changes; parse arguments with `absl::StrSplit`, return `ERROR` on missing or invalid arguments, log the reason at WARN with `external_command_logger` naming the host/service.
4. A test in `engine/tests/external_commands/` and, for user-visible effects, in `tests/broker-engine/external-commands*.robot`.

Never rename a command or change its argument order.

## Adding a Broker event or field

Both sides change in the same pull request ([`ARCHITECTURE.md`](../../ARCHITECTURE.md#cross-component-rules)):

- `bbdo/neb.proto` message or field (with the `/* io::neb, neb::de_pb_<name>, <id> */` comment for a new message), `bbdo/events.hh` id, `broker/neb/inc/com/centreon/broker/neb/internal.hh` alias, registration in `broker/neb/src/broker.cc`, and the Broker consumers.
- Engine: a `broker_<name>()` in `engine/src/broker.cc` declared in `broker.hh`, starting with `if (!cbm || !(pb_config.event_broker_options() & BROKER_<FLAG>)) return;`. New events are protobuf only; extend the legacy `forward_*` path only for a field that BBDO 2 already carries.
- Pass every string from a plugin or the configuration through `common::check_string_utf8`.
- Call it from the loop, after the state change it reports.

## Adding an enginerpc method

1. `rpc` in `engine/enginerpc/engine.proto`, request/response messages added, never renumbered.
2. Override in `engine_impl.hh` / `engine_impl.cc`: validate arguments first and answer `INVALID_ARGUMENT` with a message naming the field; do the work in a task enqueued in `command_manager`; return `UNKNOWN` or `NOT_FOUND` with a message on failure.
3. Test in `engine/tests/enginerpc/pbenginerpc.cc` (and `client.cc` when the test drives it); update `ccc` or `tests/resources/Engine.py` if they use it.

## Retention

A retained attribute is written in `retention/dump.cc` (`dump::host`, `dump::service`...), read by a setter in the `_setters` table of `retention/<object>.cc`, and applied in `retention/applier/<object>.cc`. Keep the key once released: an older or newer Engine reads the same file.

## Logging

- Use the spdlog loggers of `globals.hh` with `SPDLOG_LOGGER_<LEVEL>(<x>_logger, ...)`. Pick the logger by subject: `checks_logger` for check scheduling and results, `commands_logger` for execution, `config_logger` for parsing and apply, `notifications_logger`, `downtimes_logger`, `external_command_logger`, `neb_logger` for `broker.cc`, `runtime_logger` for runtime errors, `process_logger` for lifecycle.
- Do not add `engine_logger(...) <<` calls; when touching a line that has both, keep the spdlog one.
- INFO and above also reach the Centreon event log through `broker_sink_mt`: nothing per check above DEBUG.
- `functions_logger` at TRACE for function entry only.
- Name the subject: host name, `host_id/service_id`, command name, downtime or comment id.
- Never log a processed command line (macros resolved, `$USERn$` and `encrypt::` values decrypted); log the command name.
- Robot tests grep Engine logs (`Ctn Find In Log With Timeout`): before changing a wording, search `tests/` for it.

## Errors

- New code throws `com::centreon::exceptions::msg_fmt`; `engine_error() << ...` is legacy.
- In an applier, throw on an invalid object: `applier::state` counts it as a configuration error during `-v` and restores the previous configuration on reload.
- Nothing thrown from `broker_*()`, timed events or queued tasks reaches the loop: catch, log with the object name, continue.

## Known deviations — do not copy

The code below predates these conventions. Do not use it as a model, and fix it only in a dedicated pull request.

| Place | Deviation |
|---|---|
| `service::run_async_check` (DEBUG `run id=...`, whitelist rejection), `raw_v2::run` (TRACE `cmd=...`) | Processed command lines, possibly with decrypted secrets, are logged |
| `modules/external_commands/src/utils.cc` + `processing.cc` `thread_safe` entries | `PROCESS_SERVICE_CHECK_RESULT` / `PROCESS_HOST_CHECK_RESULT` read `host::hosts` and `service::services` on the FIFO thread without `applier::state::lock()`, while enginerpc uses `command_manager` for the same work |
| `events/loop.cc` `_dispatching` | `std::async` future discarded at the end of its block: the reload is synchronous and `reloading` is dead logic |
| `checks/checker.cc`, `commands/commands.cc`, `command_manager.cc`, `modules/opentelemetry/src/telegraf/nagios_check_result_builder.cc` | Designated initialisers (`timeval{.tv_sec = ...}`), not C++17 |
| `configuration/extended_conf.cc` `update_state` | `JsonStringToMessage` status ignored: an invalid JSON file is silently merged as empty |
| `engine/doc/engine-doc.md` § Extended configuration, § macros encryption | Describes `apply_from_cfg`/`apply_from_json`, a `secret::` prefix and a `credential_encryption` flag that no longer exist (code: `encrypt::`, no flag) |
| `inc/com/centreon/engine/restart_stats.hh`, `my_lock.hh` | No header guard; `my_lock.hh` also has no licence header and is unused |
| `inc/com/centreon/engine/dependency.hh` | Guard `CENTREON_ENGINE_DEPENDENCY_HH` instead of `CCE_` |
| `command_manager.hh` | `#endif /* !CCE_STATISTICS_HH */` does not match its guard |
| `globals.hh` | Declares `otel_logger`, never defined (the logger is `otl_logger`) |
| `commands/processing.cc` (`execute`) | Local `log_passive_check` read from `pb_config` but the legacy global `log_passive_checks` tested |
| `src/deleter/*.cc`, `src/commands/raw.cc` | Not built (`deleter`) or unused in production (`raw`, replaced by `raw_v2`) |
| `engine/tests/**` files without the `pb` prefix (`checks/service_check.cc`, `configuration/applier/applier-*.cc`...) | Not in `ut_sources`; some include the removed `common/engine_legacy_conf/` |
| `contactgroup.hh` | `#ifdef LEGACY_CONF` block for a removed build mode |
| `src/logging/CMakeLists.txt` | Commented-out `dumpers` entries |
| `main.cc` | Third-party includes and a namespace alias before Engine headers; global `SPDLOG_CRITICAL` |
| `modules/external_commands/src/main.cc` | Module info "Merethis", "Copyright 2011", "GPL version 2" |
