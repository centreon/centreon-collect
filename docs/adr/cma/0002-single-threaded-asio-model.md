# CMA-0002: Run all agent logic on a single `io_context` thread

- **Status:** Proposed
- **Date:** 2026-10-28

## Context

`main()` creates one `asio::io_context` (`g_io_context`) and calls `run()` on the main thread. The scheduler, the checks, the timers and the signal handlers all run there. `agent/doc/agent-doc.md` states the intent: the program is asynchronous except for the gRPC layers, single-threaded, and needs no mutex outside gRPC.

Other threads exist, each at a known boundary:

- gRPC callback threads (`bireactor::OnReadDone`, `OnWriteDone`, `OnDone`) post incoming requests and the reactor release to the `io_context`;
- `drive_size_thread` (`src/drive_size.cc`) and, on Windows, `check_files_thread` (`native_windows/src/check_files.cc`) run blocking file-system calls and post their results;
- the Windows Event Log subscription callback fills `event_container` under `absl::Mutex _events_m`;
- child processes complete through asio (`common/process`).

Before `run()` starts, the CA bootstrap blocks with bounded timeouts (`WaitForConnected`, 5 s, and the unary `GetCACertificate`).

Alternatives weighed:

- A thread pool running `io_context::run()`. It lost: every check, timer and the scheduler queue would need locking, for a workload bounded by `max_concurrent_checks`, not by CPU.
- One thread per check. It lost: hundreds of checks per host would mean hundreds of threads, and the scheduler would need to synchronise with all of them.

## Decision

Touch the scheduler, the checks and their timers from the `io_context` thread only. Hand work over from any other thread with `asio::post(*_io_context, ...)` before touching agent state.

Never block the `io_context` thread once `run()` has started: no `std::this_thread::sleep_for` or `Sleep()`, no synchronous gRPC stub call, no `boost::process::wait`, no file-system call that can hang, no `future::get()`, no wait on a mutex held by a slow thread. Create a thread (`std::thread`, `std::async`, `CreateThread`) only as such a worker. Run a blocking OS call in a dedicated worker built like `drive_size_thread`: one static worker, a request queue under `absl::Mutex`, results posted back, a `thread_kill()` called after `run()` returns.

Use a mutex only where a thread boundary is crossed: gRPC reactors, worker queues, OS callbacks. Capture `shared_from_this()`, or a `weak_ptr` locked at call time, in every lambda handed to an asynchronous operation; capture `this` only next to it. Return at once from a handler that receives an error code, and check the owner's `_alive` flag when it has one.

Flush logs with the asio timer of `init_log_flush_timer`, not with `spdlog::flush_every`. Get more parallelism from `max_concurrent_checks`, never from a second thread running `run()`.

## Consequences

- The scheduler and the checks have no data race by construction. Review only watches the boundaries listed above.
- A blocking call on the main thread delays every check and the metric export.
- `spdlog::flush_every` is banned because its thread holds a spdlog mutex that a forked child inherits and can hang on.
- Exception: start-up code that runs before `run()` may block with a bounded timeout, as the CA bootstrap does.
- Unit tests run the same `io_context` on a dedicated thread (`agent/test/test_main.cc`). A test that touches a check from the gtest thread while that thread runs is racy.

## When to Amend

- A measure needs CPU-bound work that a single thread cannot absorb within the check interval.
- A gRPC or OS API forces callbacks that cannot be posted, so that state must be shared across threads.
