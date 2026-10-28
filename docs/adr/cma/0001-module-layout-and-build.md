# CMA-0001: Split the agent into a portable core and one native tree per platform

- **Status:** Proposed
- **Date:** 2026-10-28

## Context

The agent ships on Linux (RPM and DEB, `packaging/centreon-monitoring-agent*.yaml`) and on Windows (Inno Setup installer, `agent/installer/`) from one code base. The root `CMakeLists.txt` compiles in C++17 on a Linux host and in C++20 on a Windows host. The Linux binary links `libstdc++` and `libgcc` statically, so that one package runs on every supported distribution.

Today the code is laid out as follows. `inc/com/centreon/agent/` and `src/` hold the core: configuration, scheduler, check base classes, gRPC, logs. `native_linux/` and `native_windows/` hold the native checks and `agent_info` of each platform. `proto/agent.proto` is the Engine contract, `precomp_inc/precomp.hh` the precompiled header, `test/` the `ut_agent` binary. Each source file is listed by hand in `agent/CMakeLists.txt`: `SRC_COMMON` for both platforms (it also receives `${NATIVE_SRC}/agent_info.cc` and `check_cpu.cc` of the current platform), `SRC_LINUX` (only `config.cc`) and `SRC_WINDOWS` (`config_win.cc`, `filter.cc` and the Windows checks). Shared template bases (`native_check_base.cc`, `native_check_cpu_base.cc`, `filter_rules.cc`) are not listed: they are `#include`d by the `.cc` file that instantiates them explicitly.

The agent depends on `common/grpc`, `common/process`, `common/crypto`, `common/inc` and `common/engine_conf/timeperiod.proto`, which Engine and Broker also use.

Alternatives weighed:

- One tree with `#ifdef _WIN32` everywhere. It lost: a reviewer could no longer tell from the path whether a change can break the other platform, and the Linux CI never compiles the Windows branches.
- A glob in CMake. It lost: a stray file would silently enter one platform's build, and the explicit lists document which file builds where.
- C++20 everywhere. It lost: the Linux build host compiler is pinned to C++17 for the whole repository.

## Decision

Put code that runs on both platforms in `src/` and `inc/com/centreon/agent/`, and write it in C++17. Put platform code in `native_linux/` or `native_windows/`, with the same header name when both platforms implement the same check (`check_cpu.hh`). Keep `#ifdef _WIN32` in the core to registering platform checks or reading an OS value in a few lines.

List every new source file in `SRC_COMMON`, `SRC_LINUX` or `SRC_WINDOWS`, and every new test file in the matching list of `agent/test/CMakeLists.txt`. Implement a new template base either in its header or in a `.cc` included by the file that instantiates it.

Take new third-party libraries from the root `vcpkg.json` and add their headers to `precomp.hh`. Keep the static runtime link on Linux. Build and test Engine and Broker when changing a library of `common/` used by the agent.

## Consequences

- C++20 features (`std::format`, `<ranges>`, concepts) are allowed in `native_windows/` only.
- The Linux CI does not compile `native_windows/`. A change in `src/` that builds on Linux can still break Windows; the Windows job of `.github/workflows/monitoring-agent.yml` is the only check.
- Generated protobuf and gRPC files are produced into `src/` by the build and are never committed.
- Files included as `.cc` are invisible to CMake; `grep '#include ".*\.cc"'` lists them.
- A new dynamic runtime dependency on Linux, or a new link library, is a decision of its own.
- Exception: `src/grpc_ca_bootstrap.cc` holds large `#ifdef _WIN32` blocks (registry, certificate store, module path). It is not extended; a new platform branch there moves to a per-platform file.
- Exception: `src/drive_size.cc` stays in `SRC_COMMON` although the storage check is registered for Windows only.

## When to Amend

- The Linux build moves to C++20, or a third platform (macOS, ARM-only target) is added.
- The static `libstdc++` link becomes impossible on a supported distribution.
