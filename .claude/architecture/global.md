# centreon-collect architecture

Entry point for anyone, human or Claude, who changes code in this repository. Read this file first, then the architecture file of the component you touch, then the conventions it links.

| Topic | File |
|---|---|
| Architecture of a component | [Components](#components) below |
| Development conventions, all components | [`../conventions/global.md`](../conventions/global.md) |
| Development conventions of a component | [Components](#components) below |
| Automated tests and CI | [`../ci/testing.md`](../ci/testing.md) |

Decisions taken from now on are recorded as Architecture Decision Records and linked from the section they change. The existing design is described here and in the component files, not in ADRs.

## Using these files from a skill, an agent or a script

These files are the single source of the repository's rules. Anything that drives Claude on this repository (skill, custom subagent, prompt, CI job) follows this contract:

- **Reference, never copy.** A skill or agent links these files by path and never restates their rules: a copy drifts the day the file changes. Content specific to the skill (its steps, its output format) stays in the skill.
- **Resolve the component first.** From the files the task touches, find the components in [Components](#components), then read: this file, [`../conventions/global.md`](../conventions/global.md), the component's architecture and conventions files when they exist, and [`../ci/testing.md`](../ci/testing.md) when tests or CI are involved.
- **Cite what was read.** The plan names the files read and the sections that govern the change.
- **Stop on a departure.** A change that has to depart from these files is raised with the user before code is written; the skill never works around a rule.
- **Subagents get the paths.** A subagent does not see the conversation that started it. Custom subagents load `CLAUDE.md` and the path-scoped rules, but the built-in Explore and Plan agents do not: their prompt names the files to read.
- **Feed corrections back.** A convention stated in a review or a correction that no file covers is proposed as a sentence for the matching file, in its own pull request.

What loads automatically: `CLAUDE.md` imports this file and the global conventions at session start. `agent/CLAUDE.md` imports the CMA files when a file under `agent/` is read. `.claude/rules/cma.md` and `.claude/rules/testing.md` point to the CMA and testing files when a matching file is read or edited. The `PreToolUse` hook `.claude/hooks/component-context.sh` names the governing files on the first edit of each component in a session. When a component gets its own files, add it to the [Components](#components) table, to the hook's mapping, and, if its paths are outside its folder, to a rule in `.claude/rules/`.

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
| Centreon Monitoring Agent (CMA) | `agent/` | C++ | Runs checks on Linux and Windows hosts, receives its configuration from Engine, sends results as OTLP | [`cma.md`](cma.md) | [`../conventions/cma.md`](../conventions/cma.md) |
| Engine | `engine/` | C++ | Scheduler, Nagios fork: runs checks, computes states, notifications. Modules in `engine/modules/` (`opentelemetry`, `external_commands`) | to write | to write |
| Broker | `broker/` | C++ | Event transmitter (`cbd`) and Engine module (`cbmod`, `broker/neb`): SQL, RRD, BAM, Lua, TSDB and OTLP outputs | to write | to write |
| Shared libraries | `common/`, `clib/` | C++ | gRPC, process, crypto, logging, perfdata, thresholds, timeperiods, configuration protobufs; used by several binaries | to write | to write |
| BBDO protocol | `bbdo/` | protobuf | Messages between Engine (`cbmod`) and Broker | to write | to write |
| Connectors | `connectors/` | C++ | Long-running Perl and SSH check executors driven by Engine | to write | to write |
| Gorgone | `gorgone/` | Perl | Configuration deployment and remote actions between central and pollers | to write | to write |
| CCC | `ccc/` | C++ | gRPC command-line client for Engine and Broker | to write | to write |
| Packaging | `packaging/`, `selinux/` | nfpm YAML, SELinux | RPM and DEB packages per component | — | — |
| Robot tests | `tests/` | Robot Framework, Python | End-to-end tests of Engine, Broker, connectors and CMA | — | [`../ci/testing.md`](../ci/testing.md) |

A component marked "to write" follows the global conventions and the code around the change. Rules written for one component never apply to another.

## Cross-component rules

- **Contracts between components.** `bbdo/*.proto` (Engine → Broker), `agent/proto/agent.proto` and the OTLP encoding (CMA ↔ Engine), `engine/enginerpc` and `broker/core/src/broker.proto` (gRPC APIs used by `ccc` and by the tests). Each side is released independently, so a contract evolves by addition and both sides change in the same pull request.
- **Shared code.** A change in `common/` or `clib/` is a change for every binary that links it. Build and run the unit tests of all of them (`ut_common`, `ut_engine`, `ut_broker`, `ut_agent`, `ut_clib`, `ut_connector`), and say in the pull request which ones were run.
- **Dependency direction.** A component depends on `common/`, `clib/`, `bbdo/` and third-party libraries. It never includes the sources of a sibling component (`agent/` never includes `engine/` or `broker/`; Engine reaches the agent through `agent.proto` only).
- **Third-party libraries** come from the root `vcpkg.json`, with ports and triplets in `overlays/` and `custom-triplets/`. Adding one is a decision to discuss before writing code.

## Build

One CMake project at the root (`CMakeLists.txt`, then `CMakeListsLinux.txt` or `CMakeListsWindows.txt`), dependencies through vcpkg. The C++ standard depends on the build host: see [`../conventions/global.md`](../conventions/global.md#language-version-and-compilers). Only the CMA and the shared code it uses build on Windows.

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

Build matrices (distributions, architectures, databases) are computed in `.github/workflows/get-environment.yml`. How to write the tests these workflows run: [`../ci/testing.md`](../ci/testing.md).
