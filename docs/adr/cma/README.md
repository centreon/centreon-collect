# CMA — Centreon Monitoring Agent ADRs

Scope: `agent/` (binary `centagent`, library `centagent_lib`, tests `ut_agent`), and the agent side of `agent/proto/agent.proto`.
Start with [ARCHITECTURE.md](ARCHITECTURE.md): components, threads, flows, and the ADR governing each.
Retro-documented from the code at commit `a78cca3`. Status `Proposed` until the owner validates each file.

| ID | Title | Read before changing |
| --- | --- | --- |
| [CMA-0001](0001-module-layout-and-build.md) | Split the agent into a portable core and one native tree per platform | `CMakeLists.txt`, any new file, `native_*` |
| [CMA-0002](0002-single-threaded-asio-model.md) | Run all agent logic on a single `io_context` thread | anything async, any thread, any callback |
| [CMA-0003](0003-grpc-streaming-and-connection-modes.md) | Talk to Engine over one bidirectional gRPC stream, in direct or reversed mode | `bireactor`, `streaming_client`, `streaming_server` |
| [CMA-0004](0004-engine-agent-protocol-contract.md) | Evolve `agent.proto` by addition only | `proto/agent.proto`, `agent_info` |
| [CMA-0005](0005-otlp-metrics-export-contract.md) | Encode check results as OTLP gauges carrying Centreon semantics in exemplars | `scheduler` result storage, metric names, exemplars |
| [CMA-0006](0006-scheduler.md) | Schedule checks on absolute time slots, rebuilt from each configuration | `scheduler`, `check::calcul_status_confirmation`, `time_step` |
| [CMA-0007](0007-check-abstraction-and-lifecycle.md) | Derive every check from `check` and complete it asynchronously | `check`, `check_exec`, `default_check_builder` |
| [CMA-0008](0008-native-check-conventions.md) | Write native checks with shared argument, threshold and perfdata conventions | any `check_*` native class, `filter` |
| [CMA-0009](0009-security.md) | Keep secrets encrypted until process start, and never trust an unverified peer | `config`, `grpc_ca_bootstrap`, `check_exec`, logs |
| [CMA-0010](0010-local-configuration.md) | Keep local configuration keys identical across the JSON file, the registry and the installers | `config.cc`, `config_win.cc`, `installer/`, `conf/` |
| [CMA-0011](0011-logging.md) | Log through the injected spdlog logger, with a level chosen by frequency | any log line |
| [CMA-0012](0012-code-style.md) | Follow the collect C++ conventions for names, headers and errors | every `.cc` / `.hh` |
| [CMA-0013](0013-testing-and-definition-of-done.md) | Ship every behaviour change with a test, and contract changes with a robot case | every PR |

## Dependency map

```
main.cc / main_win.cc
   └─ config (JSON | registry) ─ grpc_ca_bootstrap
   └─ streaming_client (AgentService.Export)      ┐ one of the two
   └─ streaming_server (ReversedAgentService.Import)┘
         └─ bireactor<grpc reactor>   (gRPC threads → asio::post)
         └─ scheduler                 (io_context thread only)
               └─ default_check_builder
                     ├─ check_exec / check_custom  → common/process
                     ├─ native checks (native_linux | native_windows)
                     └─ check_dummy (configuration error reporting)
```

Shared code outside `agent/` used by the agent: `common/grpc`, `common/process`, `common/crypto`, `common/inc` (perfdata, threshold, rapidjson_helper, check_timeperiod, file_watcher), `common/engine_conf/timeperiod.proto`. A change there also impacts Engine and Broker (see CMA-0001).
