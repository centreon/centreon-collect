# centreon-collect development conventions

Conventions shared by every component. A component file (linked from [`../architecture/global.md`](../architecture/global.md#components)) adds to these rules and wins where it is more specific. Where neither file says anything, the code around the change is the reference.

Tests: [`../ci/testing.md`](../ci/testing.md).

## Language version and compilers

The standard is chosen by the build host in the root `CMakeLists.txt`, with `CMAKE_CXX_STANDARD_REQUIRED ON` and `CMAKE_CXX_EXTENSIONS OFF`.

| Build host | Standard | Compiler | Built there |
|---|---|---|---|
| Linux | **C++17** | GCC of the distribution, in the packaging images `.github/docker/Dockerfile.packaging-centreon-collect-<distrib>`: AlmaLinux 8, 9, 10, Debian bookworm, trixie, Ubuntu jammy, noble (amd64; arm64 for Debian and Ubuntu) | Every C++ component |
| Windows | **C++20** | MSVC, `windows-latest` runner | CMA and the shared code it uses only |

Consequences:

- Code compiled on Linux, which is all code outside `agent/native_windows/`, is C++17. No `std::format`, `<ranges>`, concepts, `std::span`, designated initialisers or `<=>` there.
- The oldest compiler in the matrix is GCC 8 (AlmaLinux 8, CMA build). Avoid C++17 library features GCC 8 lacks or ships incomplete: floating-point `std::from_chars`/`std::to_chars` (GCC 11), parallel algorithms. `std::filesystem` needs the `stdc++fs` link, already set where it is used.
- Code shared between Linux and Windows (`common/`, `agent/src/`) compiles with both GCC and MSVC: no GCC extension, no POSIX-only header outside an `#ifdef _WIN32` branch or a platform file.
- Use `fmt` (through spdlog) for formatting, abseil and boost containers already in use; add a library only through `vcpkg.json` (see the architecture).

## Formatting

- C++: the root `.clang-format` (`BasedOnStyle: Chromium`). Format the lines you change. Never reformat untouched code in a functional change; a reformatting is a change of its own. No CI job checks formatting.
- Python (`tests/resources/`): autopep8.
- Robot (`tests/**/*.robot`): `robottidy --overwrite tests`.
- CMake: follow the indentation of the file.

## Naming

| Element | Rule | Example |
|---|---|---|
| Types, functions, variables, files | `snake_case` | `class stream`, `void read_config()`, `scheduler.cc` |
| Private and protected members | leading `_` | `_logger`, `_conf` |
| Private methods | leading `_` | `void _start_check()` |
| Constants and enumerators | `snake_case`; existing upper-case enumerators stay as they are | |
| Namespaces | `com::centreon::<component>[::<sub>]` | `com::centreon::broker::io`, `com::centreon::common` |
| Headers | `.hh`; sources `.cc` | |
| Public headers | `<component>/inc/com/centreon/<component>/...` | `common/inc/com/centreon/common/perfdata.hh` |

Types imposed by a third-party API (gRPC reactors, protobuf messages) keep the API's style.

## Files and headers

- Start a new file with the Apache 2.0 header and `Copyright <year> Centreon`, the creation year; copy it from a recent neighbouring file. Engine files derived from Nagios keep their GPL v2 header; never copy it into a new file.
- Header guard with `#ifndef`, never `#pragma once`. Prefix by component, as the neighbouring headers do: `CCE_` (Engine), `CCB_` (Broker), `CENTREON_COMMON_` (common), `CENTREON_AGENT_` (CMA), followed by the path and name, `_HH`.
- Include order: the matching header, then the component headers, then `com/centreon/common/...`, then third-party. Precompiled headers (`precomp.hh`) are included by CMake, not by the sources.
- `using namespace` only in `.cc` files, never in a header.
- Generated files (`*.pb.cc`, `*.pb.h`, `version.hh`, configuration files produced by CMake) are never committed; `.gitignore` lists them.

## Errors and logging

- Throw `com::centreon::exceptions::msg_fmt` (`clib/inc/com/centreon/exceptions/msg_fmt.hh`) with a message that names the object concerned (service, command, peer, file). Catch `const std::exception&`.
- An exception never escapes an asynchronous handler or a thread: it becomes a result, a status or a log line.
- Log with spdlog through the logger the component provides (`SPDLOG_LOGGER_<LEVEL>(logger, ...)`), with `fmt` placeholders. Messages emitted per check, per event or per message are DEBUG or TRACE. Every message names its subject so that support can grep it.
- Never log a secret: passwords, tokens, keys, decrypted command lines.

## Comments

- Public methods are documented with a Doxygen block (`/** @brief ... */`).
- A comment explains why. No ticket number, no commented-out code, no author name.

## Changes, commits and pull requests

- One change per pull request, opened as a draft, targeting `develop`. A refactoring, a rename or a typo fix found on the way is a separate pull request.
- Commit and pull request titles follow Conventional Commits with the component as scope: `fix(engine): ...`, `feat(cma): ...`, `chore(ci): ...`.
- A behaviour change ships with its tests ([`../ci/testing.md`](../ci/testing.md)).
- A change to a contract between components (see the architecture) changes both sides in the same pull request.
- A construct the author cannot explain in review is not merged.
