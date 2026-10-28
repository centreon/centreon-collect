# Centreon Monitoring Agent — architecture map

Entry point to the CMA ADRs. Each block of the architecture names the ADR that governs it. Rules live in the ADRs; this page only places them.

## 1. Context: where the agent sits

```mermaid
flowchart LR
  subgraph Host["Monitored host (Linux / Windows)"]
    CMA["centagent<br/>(agent/)"]
    OS[("OS APIs<br/>/proc, PDH, Evt*, SCM")]
    PLG["Plugins<br/>(centreon-plugins)"]
    LCONF[["Local config<br/>JSON / registry<br/>custom_check.ini"]]
  end
  subgraph Poller["Centreon poller"]
    ENG["centengine<br/>module opentelemetry<br/>centreon_agent/"]
    BRK["centreon-broker"]
  end
  LCONF -->|read at start| CMA
  CMA -->|native checks| OS
  CMA -->|check_exec| PLG
  CMA <-->|"gRPC bidi stream<br/>agent.proto + OTLP"| ENG
  ENG --> BRK
```

| Boundary | Contract | ADR |
| --- | --- | --- |
| Agent ↔ Engine, messages | `agent/proto/agent.proto`: `AgentInfo`, `AgentConfiguration`, `ForceCheck` | [CMA-0004](0004-engine-agent-protocol-contract.md) |
| Agent → Engine, check results | OTLP metrics: `status`, `host.name` / `service.name`, exemplars | [CMA-0005](0005-otlp-metrics-export-contract.md) |
| Agent ↔ Engine, transport | direct (`Export`) or reversed (`Import`), TLS, tokens | [CMA-0003](0003-grpc-streaming-and-connection-modes.md), [CMA-0009](0009-security.md) |
| Host admin → agent | JSON file (Linux), registry (Windows), installers, custom checks | [CMA-0010](0010-local-configuration.md) |
| Agent → packs (CTOR) | native check arguments and perfdata names | [CMA-0008](0008-native-check-conventions.md) |

## 2. Components

```mermaid
flowchart TB
  MAIN["main.cc / main_win.cc<br/>signals, Windows service, logger"]
  CONF["config<br/>config.cc | config_win.cc"]
  BOOT["grpc_ca_bootstrap"]
  CLI["streaming_client<br/>AgentService.Export"]
  SRV["streaming_server<br/>ReversedAgentService.Import"]
  BIR["bireactor&lt;T&gt;<br/>write queue, OnDone registry"]
  SCH["scheduler<br/>queue, slots, export batching"]
  BLD["default_check_builder"]
  CHK["check (base)<br/>timeout, running index, soft/hard"]
  EXE["check_exec / check_custom"]
  NAT["native checks<br/>native_linux | native_windows"]
  DUM["check_dummy"]
  COM[["common/: grpc, process,<br/>crypto, perfdata, threshold,<br/>timeperiod, file_watcher"]]

  MAIN --> CONF --> BOOT
  MAIN --> CLI
  MAIN --> SRV
  CLI --> BIR
  SRV --> BIR
  CLI --> SCH
  SRV --> SCH
  SCH --> BLD
  BLD --> EXE
  BLD --> NAT
  BLD --> DUM
  EXE --> CHK
  NAT --> CHK
  DUM --> CHK
  BIR -.-> COM
  EXE -.-> COM
  SCH -.-> COM
```

| Component | Files | Role | ADR |
| --- | --- | --- | --- |
| Entry points | `src/main.cc`, `src/main_win.cc` | start-up, signals, Windows service, plain-text expiry | [0002](0002-single-threaded-asio-model.md), [0009](0009-security.md), [0011](0011-logging.md) |
| Local configuration | `src/config.cc`, `src/config_win.cc`, `inc/.../config.hh` | singleton read once; custom checks hot reload | [0010](0010-local-configuration.md) |
| CA bootstrap | `src/grpc_ca_bootstrap.cc` | fetch and pin the CA by fingerprint | [0009](0009-security.md) |
| Transport | `bireactor`, `streaming_client`, `streaming_server` | one bidi stream, reconnect, token check | [0003](0003-grpc-streaming-and-connection-modes.md) |
| Scheduler | `src/scheduler.cc` | rebuild on config, spread, slots, concurrency, timeperiods, OTLP batching | [0006](0006-scheduler.md), [0005](0005-otlp-metrics-export-contract.md) |
| Check builder | `scheduler::default_check_builder` | decrypt, JSON → native, else exec, errors → dummy | [0007](0007-check-abstraction-and-lifecycle.md) |
| Check base | `check.hh`, `src/check.cc` | lifecycle, timeout, running index, HARD/SOFT | [0007](0007-check-abstraction-and-lifecycle.md), [0006](0006-scheduler.md) |
| Plugin execution | `src/check_exec.cc` | process spawn, output/perfdata parsing, in-memory credential encryption | [0007](0007-check-abstraction-and-lifecycle.md), [0009](0009-security.md) |
| Native checks | `native_linux/`, `native_windows/`, `src/native_check_*`, `src/drive_size.cc`, `src/check_health.cc` | OS measures, thresholds, perfdata | [0008](0008-native-check-conventions.md) |
| Filters | `src/filter.cc`, `src/filter_rules.cc` | NSClient++-like expressions (boost::parser) | [0008](0008-native-check-conventions.md) |
| Shared libraries | `common/**` | also used by Engine and Broker | [0001](0001-module-layout-and-build.md) |

## 3. Threads

```mermaid
flowchart LR
  subgraph IO["io_context thread (main)"]
    S["scheduler"] --- C["checks"] --- T["timers / signals"]
  end
  G["gRPC threads<br/>bireactor callbacks"] -->|asio::post| IO
  W1["drive_size_thread"] -->|asio::post| IO
  W2["check_files_thread (Win)"] -->|asio::post| IO
  E["Event Log callback (Win)"] -->|absl::Mutex| C
  P["child processes"] -->|asio completion| IO
```

Rule set: [CMA-0002](0002-single-threaded-asio-model.md). Mutexes only on the arrows above.

## 4. Runtime flows

### 4.1 Start-up and connection

```mermaid
sequenceDiagram
  participant M as main
  participant CF as config
  participant B as ca_bootstrap
  participant ST as streaming_client/server
  participant E as Engine
  M->>CF: load (JSON | registry)
  M->>B: bootstrap if fingerprint and no CA
  M->>ST: load(grpc_config)
  ST->>E: open stream (Export) / accept (Import, token check)
  ST->>E: MessageFromAgent.init (AgentInfo)
  Note over ST: scheduler created with default_config (no check)
```

ADRs: [0010](0010-local-configuration.md), [0009](0009-security.md), [0003](0003-grpc-streaming-and-connection-modes.md), [0004](0004-engine-agent-protocol-contract.md).

### 4.2 Configuration and check execution

```mermaid
sequenceDiagram
  participant E as Engine
  participant R as bireactor (gRPC thread)
  participant S as scheduler (io thread)
  participant K as check
  E->>R: MessageToAgent.config
  R->>S: asio::post on_engine_request
  S->>S: update(): defaults, decrypt key, build all checks, spread
  loop each slot (≤ max_concurrent_checks)
    S->>K: start_check(timeout)
    K-->>S: on_completion(index, status, perfdata, outputs) [async]
    S->>S: soft/hard, store OTLP, re-queue at next absolute time
  end
  E->>R: MessageToAgent.force_check
  R->>S: asio::post → start now if waiting
```

ADRs: [0006](0006-scheduler.md), [0007](0007-check-abstraction-and-lifecycle.md), [0002](0002-single-threaded-asio-model.md).

### 4.3 Result export

```mermaid
sequenceDiagram
  participant S as scheduler
  participant R as bireactor
  participant E as Engine
  S->>S: add ResourceMetrics (host.name, service.name, status + exemplars, perfdata)
  alt same service already in request, or > 2 MiB
    S->>R: write(request) now
  end
  S->>R: every export_period: write(request) if not empty
  R->>E: StartWrite (one in flight, queue)
  E->>E: agent_check_result_builder → check result
```

ADRs: [0005](0005-otlp-metrics-export-contract.md), [0003](0003-grpc-streaming-and-connection-modes.md).

## 5. Cross-cutting rules

| Topic | Applies to | ADR |
| --- | --- | --- |
| Layout, CMake, C++17 vs C++20, `common/` impact | every file | [CMA-0001](0001-module-layout-and-build.md) |
| Security (TLS, secrets, tokens, commands) | config, transport, exec, logs | [CMA-0009](0009-security.md) |
| Logging levels and macros | every log line | [CMA-0011](0011-logging.md) |
| C++ style | every `.cc` / `.hh` | [CMA-0012](0012-code-style.md) |
| Tests and definition of done | every PR | [CMA-0013](0013-testing-and-definition-of-done.md) |

## 6. Which ADR for which change

| Change | Read |
| --- | --- |
| New native check | 0008 → 0007 → 0002 → 0013 |
| New field sent by Engine | 0004 → 0006 → Engine side |
| New data in check results | 0005 → 0004 |
| New local option | 0010 → 0009 if security-related |
| Connection / reconnection behaviour | 0003 → 0002 → 0009 |
| Scheduling, retries, timeperiods | 0006 → 0013 |
| Build, new file, new dependency | 0001 |

Area guidance for Claude sessions: [`agent/CLAUDE.md`](../../../agent/CLAUDE.md). Design notes (scheduler worked examples, filter grammar tips): [`agent/doc/agent-doc.md`](../../../agent/doc/agent-doc.md).
