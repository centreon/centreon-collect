# CMA-0011: Log through the injected spdlog logger, with a level chosen by frequency

- **Status:** Proposed
- **Date:** 2026-10-28

## Context

The agent logs with spdlog through one logger, `g_logger` (`log.hh`), named `centreon-monitoring-agent`, created in `main` / `_main` from the local configuration: stdout, file, or rotating file (`log_max_file_size` MiB × `log_max_files`). gRPC logs are redirected to the same logger (`set_grpc_logger`). Logs flush immediately from WARN and otherwise on an asio timer (`init_log_flush_timer`). On Linux, `SIGUSR1` lowers the verbosity and `SIGUSR2` raises it at run time. Objects receive the logger as a constructor argument and keep it as `_logger`. `SPDLOG_ACTIVE_LEVEL` is compiled at TRACE.

Usage at commit `a78cca3`: `SPDLOG_LOGGER_ERROR` 117, `DEBUG` 29, `INFO` 28, `TRACE` 19, `WARN` 17, `CRITICAL` 4; global `SPDLOG_*` 17, mostly before `g_logger` exists.

Alternatives weighed:

- Global spdlog macros everywhere. It lost: they bypass the configured sinks once `g_logger` exists, and objects could not be given a test logger.
- A per-component logger. It lost: one host runs one agent, and support reads one file; a single logger with the service name in each message is enough to filter.

## Decision

Log with `SPDLOG_LOGGER_<LEVEL>(_logger, ...)` and the injected logger; use global `SPDLOG_<LEVEL>` only before `g_logger` is created, and `std::cout` only for `--help`. Format with `fmt` placeholders, and protobuf messages with `common/fmt_protobuf.hh`.

Choose the level by meaning and frequency. CRITICAL: the agent cannot continue, or trust material changed (`[SECURITY]`). ERROR: a result or a connection is lost. WARN: degraded but working. INFO: lifecycle events, a few per configuration. DEBUG: per check or per message events. TRACE: payloads and full command lines. Log anything emitted per check or per message at DEBUG or TRACE, and guard expensive TRACE formatting with `if (_logger->level() <= spdlog::level::trace)`.

Name the subject in every message: service description (with host and service ids when known), command name, peer. Keep pointer values (`{:p}`) for lifecycle traces. Never log a secret (CMA-0009). Keep `SPDLOG_ACTIVE_LEVEL` at TRACE.

## Consequences

- Support raises the verbosity of a running Linux agent without restart and gets payloads at TRACE.
- Every new log line is a support interface: its wording allows a grep on the service name.
- A per-check INFO line would flood hosts running hundreds of checks; it is avoided by rule.
- Not yet followed: `config::read_custom_checks` (`config.hh`) logs with global macros, also on hot reload, so those lines miss the file sink; `main.cc` and `main_win.cc` use global `SPDLOG_CRITICAL` on one error path after the logger exists.
- Typos in messages (`retreive`, `form api` in `scheduler::check_host_timezone`) are fixed in a dedicated change, after checking that no `cma*.robot` test greps them.

## When to Amend

- The agent exports its own logs through OpenTelemetry, which would change sinks and levels.
- Windows needs a run-time verbosity switch equivalent to the Linux signals.
