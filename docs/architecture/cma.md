# Centreon Monitoring Agent architecture

Component file of [`ARCHITECTURE.md`](../../ARCHITECTURE.md). Conventions: [`../conventions/global.md`](../conventions/global.md), then [`../conventions/cma.md`](../conventions/cma.md). Tests: [`../testing.md`](../testing.md#centreon-monitoring-agent).

The Centreon Monitoring Agent (CMA, binary `centagent`) runs checks on Linux and Windows hosts for a Centreon poller. It receives its check list from Engine over gRPC, schedules the checks itself, and sends results as OpenTelemetry (OTLP) metrics. Design notes with worked examples (scheduler, filters): `agent/doc/agent-doc.md`.

## Scope

| Zone | Path | Notes |
|---|---|---|
| Contracts | `agent/proto/agent.proto`, the OTLP encoding in `agent/src/scheduler.cc`, `engine/modules/opentelemetry/src/centreon_agent/` | Read by agents and pollers of other versions. Both sides change in the same pull request |
| Agent | `agent/src/`, `agent/inc/com/centreon/agent/`, `agent/native_linux/`, `agent/native_windows/` | All CMA rules apply |
| Install and packaging | `agent/installer/`, `agent/conf/`, `packaging/centreon-monitoring-agent/` | Configuration parity and security |
| Shared libraries | `common/grpc/`, `common/process/`, `common/crypto/`, `common/inc/`, `common/engine_conf/` | Also used by Engine and Broker: global rules only, CMA rules do not apply |
| Tests | `agent/test/`, `tests/broker-engine/cma*.robot`, `tests/resources/Agent.py` | See [`../testing.md`](../testing.md#centreon-monitoring-agent) |

The agent depends on `common/` and third-party libraries only, never on `engine/` or `broker/`. Engine depends on the agent through `agent.proto` only.

The agent code is the reference for agent conventions. Engine and Broker are older siblings: check a pattern against this file and the surrounding `agent/` code before copying it from them.

## Layout and build

```
agent/
├─ inc/com/centreon/agent/ + src/   portable core, C++17: config, scheduler, checks, gRPC, logs
├─ native_linux/                     Linux checks and agent_info
├─ native_windows/                   Windows checks and agent_info, C++20 allowed
├─ proto/agent.proto                 Engine contract
├─ precomp_inc/precomp.hh            precompiled header
├─ test/                             ut_agent (gtest)
├─ installer/                        Linux script, Windows Inno Setup and PowerShell
├─ conf/                             JSON template, registry file, custom checks example
└─ doc/agent-doc.md                  design notes
```

- The agent ships as RPM and DEB (`packaging/centreon-monitoring-agent*.yaml`) and as a Windows installer, from one code base. The Linux binary links `libstdc++` and `libgcc` statically (`-static-libgcc -static-libstdc++`), so that one package runs on every supported distribution.
- Platform code lives in `native_linux/` or `native_windows/`, with the same header name when both platforms implement a check (`check_cpu.hh`). `#ifdef _WIN32` in the core is limited to registering platform checks or reading an OS value in a few lines. A path tells a reviewer which platform a change can break; the Linux CI never compiles `native_windows/`.
- Every source file is listed by hand in `agent/CMakeLists.txt`: `SRC_COMMON` (both platforms; it also receives `${NATIVE_SRC}/agent_info.cc` and `check_cpu.cc` of the current platform), `SRC_LINUX` (`config.cc`), `SRC_WINDOWS` (`config_win.cc`, `filter.cc`, Windows checks). Test files are listed in `agent/test/CMakeLists.txt`. No glob: a stray file never enters a build silently.
- Template bases (`native_check_base.cc`, `native_check_cpu_base.cc`, `filter_rules.cc`) are not listed: they are `#include`d by the `.cc` that instantiates them. `grep '#include ".*\.cc"'` lists them.
- Generated protobuf and gRPC files are produced into `src/` by the build.
- Shared code used: `common/grpc`, `common/process`, `common/crypto`, `common/inc` (perfdata, threshold, rapidjson_helper, check_timeperiod, file_watcher), `common/engine_conf/timeperiod.proto`.

## Components and runtime flow

| Component | Files | Role |
|---|---|---|
| Entry points | `src/main.cc`, `src/main_win.cc` | Start-up, signals, Windows service, plain-text expiry |
| Local configuration | `src/config.cc`, `src/config_win.cc`, `inc/.../config.hh` | Singleton read once; custom checks hot reload |
| CA bootstrap | `src/grpc_ca_bootstrap.cc` | Fetch and pin the CA by fingerprint |
| Transport | `bireactor`, `streaming_client`, `streaming_server` | One bidirectional stream, reconnection, token check |
| Scheduler | `src/scheduler.cc` | Rebuild on configuration, spreading, slots, concurrency, timeperiods, OTLP batching |
| Check builder | `scheduler::default_check_builder` | Decrypt, JSON → native check, else `check_exec`, errors → `check_dummy` |
| Check base | `check.hh`, `src/check.cc` | Lifecycle, timeout, running index, HARD/SOFT |
| Plugin execution | `src/check_exec.cc` | Process spawn, output and perfdata parsing, credentials kept encrypted |
| Native checks | `native_linux/`, `native_windows/`, `src/native_check_*`, `src/drive_size.cc`, `src/check_health.cc` | OS measures, thresholds, perfdata |
| Filters | `src/filter.cc`, `src/filter_rules.cc` | NSClient++-like expressions (boost::parser) |

```
main.cc / main_win.cc
   └─ config (JSON | registry) ─ grpc_ca_bootstrap
   └─ streaming_client (AgentService.Export)        ┐ one of the two
   └─ streaming_server (ReversedAgentService.Import)┘
         └─ bireactor<grpc reactor>   (gRPC threads → asio::post)
         └─ scheduler                 (io_context thread only)
               └─ default_check_builder
                     ├─ check_exec / check_custom  → common/process
                     ├─ native checks (native_linux | native_windows)
                     └─ check_dummy (configuration error reporting)
```

`main` loads the local configuration, bootstraps the CA if needed, opens the stream and sends `AgentInfo`. Engine sends `AgentConfiguration`; the gRPC thread posts it to the scheduler, which rebuilds and spreads every check. Each check starts in its slot and completes asynchronously; the scheduler computes HARD/SOFT, stores the OTLP data and re-queues the check at its next absolute time. Every `export_period`, or earlier when needed, the request is written to the stream.

## Threading model

One `asio::io_context` (`g_io_context`) runs on the main thread: scheduler, checks, timers, signal handlers. The program is asynchronous and needs no mutex outside its thread boundaries:

| Other thread | Boundary |
|---|---|
| gRPC callbacks (`bireactor::OnReadDone`, `OnWriteDone`, `OnDone`) | Post incoming requests and the reactor release to the `io_context` |
| `drive_size_thread` (`src/drive_size.cc`), `check_files_thread` (Windows) | Run blocking file-system calls, post results back |
| Windows Event Log subscription callback | Fills `event_container` under `absl::Mutex` |
| Child processes | Complete through asio (`common/process`) |

A thread pool or a thread per check was rejected: the load is bounded by `max_concurrent_checks`, not by CPU, and every check and timer would need locking. Before `run()` starts, start-up code may block with a bounded timeout (the CA bootstrap does: `WaitForConnected` 5 s, unary `GetCACertificate`). A blocking call on the io thread after that delays every check and the export.

Unit tests run the same `io_context` on a dedicated thread (`agent/test/test_main.cc`).

## Transport

One long-lived bidirectional gRPC stream per Engine, with the callback API. `reversed_grpc_streaming` picks who connects:

- direct (default): the agent is the client (`streaming_client`) and calls `AgentService.Export`;
- reversed: the agent is the server (`streaming_server`) and Engine calls `ReversedAgentService.Import`, when the connection must be opened from the poller.

Both modes are functionally identical (same first message, same scheduler, same builder); they differ only by who connects. Both reactors derive from `bireactor<grpc reactor>`, instantiated twice in `bireactor.cc`: one write in flight, one pending read, and a static registry `_instances` that keeps a reactor alive until gRPC calls `OnDone`. `OnDone` releases the reactor through `asio::post`, because releasing it inside the gRPC callback can join the current thread and crash gRPC. `_instances` is never freed, on purpose; leak checkers report it.

The client keeps one scheduler across reconnections and recreates a reactor 10 s after an error; the channel backoff is bounded by `second_max_reconnect_backoff`. The server checks the token in `Import`, answers a refusal with an `ImmediateFinishReactor`, accepts one Engine stream at a time and closes it when the JWT expires. Channels, servers, TLS and keepalive come from `common::grpc::grpc_client_base` / `grpc_server_base`; the only channel built outside them is the CA bootstrap probe.

## Protocol contract (`agent.proto`)

Package `com.centreon.agent`, proto3, compiled by the agent and by Engine. Agents and pollers are upgraded independently and may run against an older or newer peer for months.

- The agent sends `MessageFromAgent` (`oneof`: `init` = `AgentInfo`, always first; `otel_request` = OTLP export). Engine sends `MessageToAgent` (`oneof`: `config` = `AgentConfiguration`, `otel_response`, `force_check`). `CACertificateService.GetCACertificate` is a unary call used by the CA bootstrap.
- `AgentConfiguration` is a full snapshot (services, timeperiods, limits, key and salt), never a delta. The agent ignores content it does not understand.
- A zero value means the previous behaviour: `check_interval` 0 → 300 s, `retry_interval` 0 → 60 s, `max_attempts` 0 → 3, empty `check_period_name` → always in period. `utc_offset` and `dst_active` are `optional` because absent and zero differ.
- Capabilities are announced field by field in `AgentInfo` rather than by a version number: Engine sends `key` and `salt` only if `AgentInfo.encryption_ready` is set and only on a TLS stream.
- `timeperiod.proto` belongs to `common/engine_conf`; the agent only reads it.

The contract evolves by addition; the rules are in [`../conventions/cma.md`](../conventions/cma.md#protocol-and-otlp).

## OTLP export contract

Engine rebuilds a Nagios-like result from the OTLP metrics (`agent_check_result_builder.cc`) and parses every element by name, with `use_exemplar = true`. The encoding (`scheduler::_store_result_in_metrics_and_exemplars`):

- one `ResourceMetrics` per service and per request, attributes `host.name` and `service.name` (empty for the host check);
- gauge `status`, `as_int` 0–3, `description` = first output line up to `|`, exemplars `status_confirmed` and `current_attempt`;
- one gauge per perfdata, named after its label, with its unit and one attribute among `counter`, `derive`, `absolute`, `auto`, `gauge`; exemplars `crit_gt`/`crit_ge`, `crit_lt`/`crit_le`, `warn_gt`/`warn_ge`, `warn_lt`/`warn_le`, `min`, `max` (`_ge`/`_le` for inclusive `@` ranges);
- `start_time_unix_nano` = check start, `time_unix_nano` = check end.

Results accumulate in one request, sent every `export_period` on absolute time points when not empty, and earlier when the same service already has a result in it or when the estimated size passes 2 MiB (below the 4 MiB minimum `max_message_length`). OTLP gives Engine one ingestion path shared with other OpenTelemetry sources; exemplars carry per-point Centreon semantics without multiplying series.

## Scheduler

The agent schedules its services and reports state, attempt and HARD/SOFT itself. Algorithm with examples: `agent/doc/agent-doc.md` § Scheduler.

- On each `AgentConfiguration`, `scheduler::update()` applies the zero-value defaults, clears the queue and rebuilds every check. Running checks finish, but their result is dropped (`check->get_conf() != _conf`).
- Checks are grouped by `min(check_interval, retry_interval)` and interleaved; first starts are spread by `min_interval / nb_checks`, from 1 s after reception.
- Time is cut into slots (`time_step`); the queue is an `absl::btree_map<slot, check>`: one check per slot, a busy slot pushes the check forward. A timer starts due checks while `_active_check < max_concurrent_checks`.
- The next start is computed from absolute time points (`increment_to_after_min`), never "end + interval", so intervals do not drift with check duration. Switching between SOFT and HARD jumps to the other cadence.
- A check outside its `check_period_name` is re-queued at the next valid time (`common::next_valid_time_in_period_by_name`), or one year later if none exists within 366 days.
- `ForceCheck` starts a waiting check at once; a running check is not forced.
- `check::calcul_status_confirmation` computes HARD/SOFT and the attempt (Nagios semantics, `max_attempts` floored at 1). Engine trusts `status_confirmed` and `current_attempt` as sent.
- The host check is the service with an empty `service_description`.

## Checks

Every check derives from `com::centreon::agent::check` (`enable_shared_from_this`), which owns the schedules, the timeout timer, a running index, statistics, the HARD/SOFT state, its `Service` and a `shared_ptr` to the configuration that holds it.

`scheduler::default_check_builder` turns `Service.command_line` into a check:

1. `encrypt::` prefix with a key present: decrypted; failure → `check_dummy`.
2. Valid JSON: native check chosen by `"check"`: `cpu_percentage`, `health`, `custom`; on Windows also `uptime`, `storage`, `memory`, `service`, `counter`, `tasksched`, `files`, `eventlog_nscp`, `process_nscp`. `args.timeout` sets the timeout. Unknown type or constructor exception → `check_dummy`.
3. Anything else: `check_exec`, a plugin process run by `common::process`, with a process timeout of check timeout + 100 ms so that the check's timer fires first.

`check_dummy` reports a configuration error as a CRITICAL result, so that a broken service shows in Centreon instead of only in agent logs. `check_custom` resolves `{"name": ..., "ARGn": ...}` against the local custom checks file. The running index protects against late results: a job captures it at start, and `on_completion` drops a result whose index is no longer current (after a timeout, for instance).

Native checks replace external plugins for common OS indicators. The Monitoring Connectors (`OS-Linux-Centreon-Monitoring-Agent`, `OS-Windows-Centreon-Monitoring-Agent`) build JSON command lines `{"check": "<type>", "args": {...}}`, and UI graphs and thresholds rely on the perfdata names. Several Windows checks reproduce NSClient++ options so that NSClient++ users can migrate. Building blocks: `native_check_base<N>` (one snapshot of N counters), `native_check_cpu<N>` (two snapshots over time), `common::threshold` (Nagios ranges), `common::rapidjson_helper` (typed arguments), `filter` / `filter_rules.cc` (boost::parser).

## Local configuration

The remote configuration (what to check, how often) comes from Engine and is never stored. The local configuration (how to connect, log, identify) is read once at start-up by the `config` singleton (`config::load`, `config::instance()`), from four places that must agree:

| Place | Platform |
|---|---|
| `src/config.cc`: `/etc/centreon-monitoring-agent/centagent.json`, validated by `config::config_schema` (template `conf/centagent.json.in`) | Linux |
| `src/config_win.cc`: `HKLM\SOFTWARE\Centreon\<service name>` (default `CentreonMonitoringAgent`, `--service-name` for other instances) | Windows |
| `installer/scripts_linux/install_cma.sh` | Linux |
| `installer/installer.iss.in`, `installer/install_cma.ps1`, `conf/centagent.reg` | Windows |

Keys: `host`, `host_template`, `endpoint` (only required key), `encryption`, `public_cert`, `private_key`, `ca_certificate` (JSON alias `ca`), `ca_name` (JSON alias `ca_common_name`), `fingerprint`, `reversed_grpc_streaming`, `log_level`, `log_type`, `log_file`, `log_max_file_size`, `log_max_files`, `second_max_reconnect_backoff`, `max_message_length`, `token`, `custom_check_file`.

Windows uses the registry because its installer, silent installs and multi-instance services are built on it. Only custom checks reload at run time: `custom_check_file` points to an INI-like file (`conf/custom_check.ini`), reloaded by a `common::file_watcher`; a malformed file keeps the previous set, and new commands apply to checks built from the next Engine configuration.

## Logging

One spdlog logger, `g_logger` (`log.hh`, named `centreon-monitoring-agent`), created in `main` from the local configuration: stdout, file or rotating file. gRPC logs go to the same logger. Logs flush immediately from WARN, otherwise on an asio timer (`init_log_flush_timer`). On Linux `SIGUSR1` / `SIGUSR2` lower and raise the verbosity at run time. Objects receive the logger in their constructor and keep it as `_logger`. One logger is enough because one host runs one agent and support reads one file.

## Security model

The agent runs as a service on customer hosts (user `centreon-monitoring-agent` on Linux, a Windows service), runs commands sent by a remote poller and receives credentials inside command lines.

- **Transport.** `encryption` is `full` (TLS, server verified), `insecure` (TLS, no verification) or `no` (plain text). In plain text the agent logs a warning and stops after one hour (`main.cc`, `main_win.cc`); plain text is for demos.
- **Peer authentication.** Direct mode: the agent sends `token`. Reversed mode: `token` is the trusted set checked in `streaming_server::Import`.
- **CA distribution.** With `fingerprint` and no CA, `ca_bootstrap_workflow` (direct mode) probes full TLS; on failure it fetches the CA over `TLS_SKIP_VERIFY_CA`, keeps it only if its SHA-256 fingerprint matches, persists it (Linux: file owned by the agent account plus JSON; Windows: Local Machine store, or file plus registry) and logs the change at CRITICAL with `[SECURITY]`.
- **Credentials.** With `key` and `salt`, command lines start with `encrypt::` (AES-256, `common/crypto/aes256`). The agent decrypts at build time, re-encrypts the arguments in `process_args`, decrypts them just before `start_process` and clears the plain text right after.
- **Local commands.** Custom checks come from a file owned by the host administrator; Engine only names them. Command lines are split by `common::process::parse_cmd_line` and run without a shell.

## Changes that need the owner's agreement first

Ask before writing code for:

- a change to `agent.proto`, the OTLP encoding, the scheduler algorithm or `calcul_status_confirmation`;
- a new thread, a new `vcpkg.json` dependency, a new link library or dynamic runtime dependency on Linux;
- anything in the security model above (review by `@centreon/owners-security`);
- a change under `common/` (shared with Engine and Broker).

## Known limits of the current design

- Engine sends full snapshots: a poller reload restarts every schedule, spread again from scratch.
- With one write in flight, a large export delays the next one.
- Windows native checks are only compiled and tested on the Windows runner.
- The plain-text expiry timer is duplicated in `main.cc` and `main_win.cc`.
- The early flush only measures size once more than 10 `ResourceMetrics` are present.
