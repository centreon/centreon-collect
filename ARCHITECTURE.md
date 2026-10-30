# centreon-collect architecture

Entry point for anyone, human or agent, who changes code in this repository. Read this file first, then the architecture file of the component you touch, then the conventions it links. Rules for coding agents: [`AGENTS.md`](AGENTS.md).

| Topic | File |
|---|---|
| Architecture of a component | [Components](#components) below |
| Development conventions, all components | [`docs/conventions/global.md`](docs/conventions/global.md) |
| Development conventions of a component | [Components](#components) below |
| Automated tests and CI | [`docs/testing.md`](docs/testing.md) |
| What blocks a merge | [Review criteria](#review-criteria) below |
| Architecture decisions | [`docs/adr/`](docs/adr/README.md) |

Decisions taken from now on are recorded as Architecture Decision Records in [`docs/adr/`](docs/adr/README.md) and linked from the section they change. The existing design is described here and in the component files, not in ADRs.

When a component gets its own files, add it to the [Components](#components) table, to the mapping of `.claude/hooks/component-context.sh`, a `CLAUDE.md` in its folder that imports them, and, for its paths outside that folder, a folder `CLAUDE.md` or a rule in `.claude/rules/`.

## What the repository ships

centreon-collect is the collection side of Centreon: everything that runs on a poller or on a monitored host, plus the Broker that feeds the central database.

```
 monitored hosts                 poller                                    central
┌──────────────────┐  gRPC   ┌────────────────────────────┐  BBDO   ┌──────────────────────┐
│ centagent (CMA)  │◄───────►│ centengine                 │────────►│ cbd (Broker)         │
└──────────────────┘  OTLP   │  ├─ opentelemetry module   │ TCP/gRPC│  ├─ unified_sql ─► DB │
                             │  ├─ cbmod (Broker NEB mod.)│         │  ├─ rrd ─► RRD files  │
 plugins, connectors ◄───────│  └─ checks: process,       │         │  ├─ bam, lua, tsdb…  │
 (perl, ssh)                 │     connectors             │         │  └─ otlp             │
                             └────────────────────────────┘         └──────────────────────┘
                gorgone (Perl): configuration deployment and actions between central and pollers
```

## Components

| Component | Path | Language | Role | Architecture | Conventions |
|---|---|---|---|---|---|
| Centreon Monitoring Agent (CMA) | `agent/` | C++ | Runs checks on Linux and Windows hosts, receives its configuration from Engine, sends results as OTLP | [`docs/architecture/cma.md`](docs/architecture/cma.md) | [`docs/conventions/cma.md`](docs/conventions/cma.md) |
| Engine | `engine/`, `common/engine_conf/` | C++ | Scheduler, Nagios fork: runs checks, computes states, notifications. Modules in `engine/modules/` (`opentelemetry`, `external_commands`) | [`docs/architecture/engine.md`](docs/architecture/engine.md) | [`docs/conventions/engine.md`](docs/conventions/engine.md) |
| Broker | `broker/` | C++ | Event transmitter (`cbd`) and Engine module (`cbmod`, `broker/neb`): SQL, RRD, BAM, Lua, TSDB and OTLP outputs | [`docs/architecture/broker.md`](docs/architecture/broker.md) | [`docs/conventions/broker.md`](docs/conventions/broker.md) |
| Shared libraries | `common/`, `clib/` | C++ | gRPC, process, crypto, logging, perfdata, thresholds, timeperiods, configuration protobufs; used by several binaries | to write | to write |
| BBDO protocol | `bbdo/` | protobuf | Messages between Engine (`cbmod`) and Broker | [Broker § Contracts](docs/architecture/broker.md#contracts) | [Broker § BBDO events](docs/conventions/broker.md#bbdo-events) |
| Connectors | `connectors/` | C++ | Long-running Perl and SSH check executors driven by Engine | to write | to write |
| Gorgone | `gorgone/` | Perl | Configuration deployment and remote actions between central and pollers | to write | to write |
| CCC | `ccc/` | C++ | gRPC command-line client for Engine and Broker | to write | to write |
| Packaging | `packaging/`, `selinux/` | nfpm YAML, SELinux | RPM and DEB packages per component | — | — |
| Robot tests | `tests/` | Robot Framework, Python | End-to-end tests of Engine, Broker, connectors and CMA | — | [`docs/testing.md`](docs/testing.md) |

A component marked "to write" follows the global conventions and the code around the change. Rules written for one component never apply to another.

## Cross-component rules

- **Contracts between components.** `bbdo/*.proto` (Engine → Broker), `agent/proto/agent.proto` and the OTLP encoding (CMA ↔ Engine), `engine/enginerpc` and `broker/core/src/broker.proto` (gRPC APIs used by `ccc` and by the tests). Each side is released independently, so a contract evolves by addition and both sides change in the same pull request.
- **Shared code.** A change in `common/` or `clib/` is a change for every binary that links it. Build and run the unit tests of all of them (`ut_common`, `ut_engine`, `ut_broker`, `ut_agent`, `ut_clib`, `ut_connector`), and say in the pull request which ones were run.
- **Dependency direction.** A component depends on `common/`, `clib/`, `bbdo/` and third-party libraries. It never includes the sources of a sibling component (`agent/` never includes `engine/` or `broker/`; Engine reaches the agent through `agent.proto` only). Existing exception: Engine and Broker include each other's headers and Engine links `cbmod` (see the Known limits of both files); do not add a new cross-include.
- **Third-party libraries** come from the root `vcpkg.json`, with ports and triplets in `overlays/` and `custom-triplets/`. Adding one is a decision to discuss before writing code.

## Review criteria

### Zones

| Zone | Paths | Rules that apply |
|---|---|---|
| Documented component | CMA, Engine, Broker: the zones of the Scope table of their architecture file, outside shared code | Global and component files, in full |
| Component without files | `connectors/`, `gorgone/`, `ccc/` | Global files; the code around the change is the reference |
| Shared code | `common/` (except `common/engine_conf/`, Engine), `clib/` | Global files only, never the rules of one component; unit tests of every consumer |
| Known deviations | Tables "Known deviations" of the conventions files | Never a model; fixed only in a dedicated pull request |

Where no file answers, the code around the change is the reference. In a documented component, its own code wins over an older sibling (the CMA file names which siblings not to copy).

### Blocking criteria

| Finding | Documented component | Component without files | Shared code |
|---|---|---|---|
| Bug or regression | Blocks | Blocks | Blocks |
| Security: secret in a log or an output, TLS or token weakened, command run through a shell | Blocks | Blocks | Blocks |
| CI red: build, unit tests, Robot tests, secret scan | Blocks | Blocks | Blocks |
| Contract changed other than by addition, or one side only | Blocks | Blocks | Blocks |
| Test required by [`docs/testing.md`](docs/testing.md) missing | Blocks | Blocks | Blocks |
| Global convention not followed in new code | Blocks | Blocks | Blocks |
| Component convention not followed, or a known deviation copied | Blocks | — | — |
| Change listed under "needs the owner's agreement" made without it | Blocks | — | Blocks (`common/` change seen from the CMA) |
| Global convention not followed in existing code the change touches | To challenge | To challenge | To challenge |
| Formatting of the changed lines | To challenge | To challenge | To challenge |

"Blocks": the pull request is not merged until it is fixed. "To challenge": the reviewer raises it; the author fixes it or explains why not.

### Checked by tools

| Check | Where | Notes |
|---|---|---|
| Build with the standard of each host, warnings `-Wall -Wextra` | `collect.yml`, `monitoring-agent.yml` | Windows code (`agent/native_windows/`) builds only on the Windows runner |
| Unit and Robot tests | Same workflows, `robot-test.yml`, `windows-agent-robot-test.yml` | |
| Secrets and dependencies | `secu-secret-scan.yml`, `secu-dependency-scan.yml`, `.githooks/` (gitleaks) | |
| Formatting | Local only: `clang-format`, `robottidy`, autopep8 | No CI job: check it before pushing |

A rule a tool checks is not repeated in a prompt or a skill.

## Build

One CMake project at the root (`CMakeLists.txt`, then `CMakeListsLinux.txt` or `CMakeListsWindows.txt`), dependencies through vcpkg. The C++ standard depends on the build host: see [`docs/conventions/global.md`](docs/conventions/global.md#language-version-and-compilers). Only the CMA and the shared code it uses build on Windows.

```bash
# Linux, from the repository root (vcpkg setup: README.md § Compilation)
cmake -B build -DVCPKG_OVERLAY_TRIPLETS=custom-triplets -DVCPKG_TARGET_TRIPLET=x64-linux \
      -DVCPKG_OVERLAY_PORTS=overlays -DWITH_TESTING=On -S .
cmake --build build
```

## CI at a glance

| Workflow | Covers |
|---|---|
| `.github/workflows/collect.yml` | Engine, Broker, common, clib, connectors: Linux build, unit tests, packages, robot tests |
| `.github/workflows/monitoring-agent.yml` | CMA: Linux and Windows builds, `ut_agent`, installers, `cma*.robot` |
| `.github/workflows/windows-agent-robot-test.yml` | Windows agent against a Linux poller |
| `.github/workflows/gorgone.yml`, `robot-test-gorgone.yml` | Gorgone |
| `.github/workflows/secu-secret-scan.yml`, `secu-dependency-scan.yml` | Secrets and dependency scans |

Build matrices (distributions, architectures, databases) are computed in `.github/workflows/get-environment.yml`. How to write the tests these workflows run: [`docs/testing.md`](docs/testing.md).
