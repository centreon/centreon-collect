# CMA-0001 — Module layout, build and platform split

- Status: Proposed
- Owner: @centreon/owners-cpp
- Scope: `agent/**`, `agent/CMakeLists.txt`, `agent/test/CMakeLists.txt`

## Context

The agent ships on Linux (RPM/DEB, `packaging/centreon-monitoring-agent*.yaml`) and Windows (Inno Setup installer, `agent/installer/`). One code base, two native check sets, two entry points. The root `CMakeLists.txt` sets C++17 on Linux hosts and C++20 on Windows hosts. The Linux binary links `libstdc++` and `libgcc` statically so that one package runs on every supported distribution.

Layout:

| Path | Content | Compiled on |
| --- | --- | --- |
| `inc/com/centreon/agent/`, `src/` | core: config, scheduler, checks base, gRPC, logs; `config.cc` Linux only, `config_win.cc`, `filter*.cc`, `main_win.cc` Windows only | both (per CMake list) |
| `native_linux/{inc,src}` | Linux native checks (`check_cpu` on `/proc/stat`), `agent_info` | Linux |
| `native_windows/{inc,src}` | Windows native checks (cpu, memory, uptime, storage, service, counter, tasksched, files, eventlog, process), `ntdll`, `windows_util` | Windows |
| `proto/agent.proto` | Engine ↔ agent contract (CMA-0004) | both |
| `precomp_inc/precomp.hh` | precompiled header: STL, fmt/spdlog, abseil, re2, boost asio/multi_index, rapidjson | both |
| `src/main.cc` / `src/main_win.cc` | Linux entry point / Windows entry point and Windows service | one each |
| `src/config.cc` / `src/config_win.cc` | JSON file / registry configuration (CMA-0010) | one each |
| `test/` | `ut_agent` gtest binary (CMA-0013) | both |
| `installer/`, `conf/`, `scripts/` | installers, default config, systemd unit | per platform |
| `doc/agent-doc.md` | developer documentation (scheduler, native checks, filters) | — |

## Decision

1. Platform-independent code MUST live in `src/` + `inc/com/centreon/agent/` and MUST compile in C++17 with GCC on Linux. C++20 features (`std::format`, concepts, ranges) are allowed only in `native_windows/`.
2. Platform-specific code MUST live in `native_linux/` or `native_windows/`. A `#ifdef _WIN32` in `src/` is allowed only to register or include platform checks (`scheduler.cc`) or to read an OS value with a few lines (`scheduler::check_host_timezone`). A larger divergence becomes a file per platform with the same header name (`check_cpu.hh` exists in both trees).
3. Every new source file MUST be added explicitly to `SRC_COMMON` (both platforms; it also receives `${NATIVE_SRC}/agent_info.cc` and `check_cpu.cc` of the current platform), `SRC_LINUX` or `SRC_WINDOWS` in `agent/CMakeLists.txt` (no glob), and every new test file to `SRC_COMMON` or the platform list in `agent/test/CMakeLists.txt`.
4. Template implementations shared between checks (`native_check_base.cc`, `native_check_cpu_base.cc`, `filter_rules.cc`) are NOT listed in CMake; they are `#include`d by the `.cc` that explicitly instantiates them (`template class native_check_base<...>;`). A new template base MUST follow the same pattern or be header-only.
5. Generated protobuf/gRPC files are produced into `src/` at build time by the custom commands of `agent/CMakeLists.txt`. They MUST NOT be committed nor edited.
6. Third-party headers used by the core SHOULD be added to `precomp_inc/precomp.hh` rather than included per file, and new third-party dependencies MUST come from `vcpkg.json` at the repository root.
7. The Linux link MUST keep `-static-libgcc -static-libstdc++` and the `libstdc++.a` presence check. A new dynamic runtime dependency on Linux requires an ADR.
8. A change in `common/**` used by the agent (`common/grpc`, `common/process`, `common/crypto`, `common/inc`, `common/engine_conf/timeperiod.proto`) MUST be built and tested for Engine and Broker too: these libraries are shared.

## Consequences

- One reviewer can tell from the path whether a change can break the other platform.
- Linux CI does not compile `native_windows/`. A change in `src/` that compiles on Linux may still break Windows: the Windows CI job (`.github/workflows/monitoring-agent.yml`) is the only check.
- The `.cc`-include pattern hides some translation units from CMake; a grep for `#include ".*\.cc"` lists them.

## Guardrails

- A `.cc` file added without its CMake line.
- `std::format`, `<format>`, `<ranges>`, `<concepts>`, designated initializers or `consteval` in `src/`, `inc/` or `native_linux/`.
- A Windows API (`windows.h`, `Pdh*`, `Evt*`, `GetTickCount64`) or a Linux-only API (`/proc`, `localtime_r`, `getpid`) outside a platform tree or an `#ifdef _WIN32` block.
- A generated `*.pb.cc`, `*.pb.h`, `*.grpc.pb.*` file in the diff.
- A new library in `target_link_libraries` without a matching `vcpkg.json` entry.

## Known deviations

- `src/grpc_ca_bootstrap.cc` holds large `#ifdef _WIN32` blocks (registry, certificate store, module path). Do not extend; a new platform branch there moves to a per-platform file.
- `src/drive_size.cc` (storage check) is platform-independent in `src/` but registered only under `_WIN32` in `scheduler::default_check_builder`; it is listed in `SRC_COMMON`. Keep it in `src/`.

## References

- `agent/CMakeLists.txt`, `agent/test/CMakeLists.txt`, root `CMakeLists.txt`, `vcpkg.json`
- `agent/precomp_inc/precomp.hh`
- `.github/workflows/monitoring-agent.yml`, `.github/workflows/collect.yml`
