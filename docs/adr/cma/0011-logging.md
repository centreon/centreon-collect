# CMA-0011 — Logging

- Status: Proposed
- Owner: @centreon/owners-cpp
- Scope: `agent/**`

## Context

The agent logs with spdlog through one logger, `g_logger` (`log.hh`), named `centreon-monitoring-agent`, created in `main` / `_main` from the local configuration: stdout, file, or rotating file (`log_max_file_size` MiB × `log_max_files`). gRPC logs are redirected into the same logger (`set_grpc_logger`, abseil log sink or `gpr_set_log_function` depending on gRPC version). Flushing: immediate from `warn`, plus a periodic asio timer (`init_log_flush_timer`); `spdlog::flush_every` is banned (its thread holds a mutex inherited by forked children, CMA-0002). On Linux, `SIGUSR1` lowers verbosity and `SIGUSR2` raises it at run time. Objects receive the logger as a `std::shared_ptr<spdlog::logger>` constructor argument and store it as `_logger`.

Usage in the code (counts at `a78cca3`): `SPDLOG_LOGGER_ERROR` 117, `INFO` 28, `DEBUG` 29, `TRACE` 19, `WARN` 17, `CRITICAL` 4; global `SPDLOG_*` 17, mostly before `g_logger` exists (configuration parsing, start-up).

## Decision

1. Code MUST log with the `SPDLOG_LOGGER_<LEVEL>(_logger, fmt, args...)` macros and the injected logger. Global `SPDLOG_<LEVEL>` is allowed only before `g_logger` is created. `std::cout` / `printf` only for `--help`.
2. Messages use `fmt` placeholders, never string concatenation in the argument list. Protobuf messages are formatted with `common/fmt_protobuf.hh` (`{}` on a message).
3. Levels:
   - `CRITICAL`: the agent cannot continue, or trust material changed (`[SECURITY]` tag, CMA-0009);
   - `ERROR`: an operation failed and a result or connection is lost (check failed to start, stream read failure);
   - `WARN`: degraded but working (plain-text transport, CA persisted for this session only);
   - `INFO`: lifecycle events, at most a few per configuration (start, connection, configuration received, custom checks reloaded);
   - `DEBUG`: per check or per message events;
   - `TRACE`: payloads and full command lines.
4. Anything emitted per check execution or per received message MUST be `DEBUG` or `TRACE`. A per-check `INFO` floods hosts running hundreds of checks.
5. When a log at `TRACE` requires expensive formatting, guard it with `if (_logger->level() <= spdlog::level::trace)` as the scheduler does.
6. Each message MUST identify its subject: service description (and host/service ids when available), command name, peer. Pointer values (`{:p}`) are for object lifecycle traces only.
7. Secrets are never logged (CMA-0009 rules 3–6).
8. The `SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE` compile definition stays: verbosity is a run-time decision.

## Consequences

- Support can raise verbosity on a running agent without restart (Linux signals) and get payloads at `TRACE`.
- Every new log line is a support interface: wording should allow a grep on the service name.

## Guardrails

- `SPDLOG_INFO(...)` / `spdlog::info(...)` after start-up, or a new `spdlog::logger` created outside `main`.
- `INFO` or higher inside the scheduler loop, check start or completion paths (except errors).
- A log without the service or peer it concerns.
- `spdlog::flush_every`.

## Known deviations

- `config::read_custom_checks` (`config.hh`) logs with global `SPDLOG_INFO`/`SPDLOG_ERROR`, also on hot reload when `g_logger` exists, so those lines miss the file sink. `main.cc` / `main_win.cc` also use global `SPDLOG_CRITICAL` after logger creation on one error path. Do not copy.
- Several messages contain typos (`retreive`, `form api` in `scheduler::check_host_timezone`). Fixing them is allowed in a dedicated PR; robot tests may grep log text, so check `tests/broker-engine/cma*.robot` first.

## References

- `agent/inc/com/centreon/agent/log.hh`, `agent/src/log.cc`
- `agent/src/main.cc`, `agent/src/main_win.cc`
- `common/inc/com/centreon/common/fmt_protobuf.hh`
