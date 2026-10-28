# CMA-0002 — Single-threaded ASIO execution model

- Status: Proposed
- Owner: @centreon/owners-cpp
- Scope: `agent/**`

## Context

`main()` creates one `asio::io_context` (`g_io_context`) and calls `g_io_context->run()` on the main thread. The scheduler, every check, every timer and the signal handlers run there. `agent/doc/agent-doc.md` states the intent: "entirely asynchronous, with the exception of the gRPC layers; single-threaded and therefore needs no mutexes, except in the gRPC part".

Other threads exist and are bounded:

| Thread | Owner | How it reaches the main thread |
| --- | --- | --- |
| gRPC callback threads | `bireactor::OnReadDone/OnWriteDone/OnDone` | `streaming_client::on_incomming_request` and `server_reactor::on_incomming_request` `asio::post` to the scheduler; `OnDone` posts the reactor release |
| `drive_size_thread` (storage, blocking FS calls) | `src/drive_size.cc` | `asio::post` of the result |
| `check_files_thread` (Windows files check) | `native_windows/src/check_files.cc` | `asio::post` of the result |
| Windows Event Log subscription callback | `event_log::event_container` | guarded by `absl::Mutex _events_m`, read by the check on the main thread |
| Process children | `common/process` | completion through asio |

## Decision

1. Scheduler, checks and timers MUST only be touched from the `io_context` thread. Any callback that runs on another thread MUST hand over with `asio::post(*_io_context, ...)` before touching agent state.
2. Code on the `io_context` thread MUST NOT block: no `sleep`, no synchronous network, no blocking file-system call that can hang (network drives, NFS), no `future::get()`, no waiting on a mutex held by a slow thread. A blocking OS API MUST go to a dedicated worker thread following the `drive_size_thread` pattern: one static worker, a request queue under `absl::Mutex`, results posted back, a `thread_kill()` called after `io_context::run()` returns.
3. Mutexes are allowed only at a thread boundary: gRPC reactors (`std::mutex _protect`), worker queues, OS callbacks. A mutex in the scheduler or a check that never crosses a thread is rejected.
4. `check::start_check` MUST return quickly; its result MUST be delivered asynchronously, never by calling `on_completion` synchronously from `start_check` (see CMA-0007).
5. Every lambda posted or passed to an async operation MUST capture a `shared_from_this()` (or a `weak_ptr` locked at call time) so that the object outlives the operation. Raw `this` capture is allowed only in addition to a `shared_ptr` capture (`[me = shared_from_this(), this]`).
6. Every timer or async handler MUST return immediately on `err` (cancellation) and SHOULD check the owner's `_alive` flag when the owner has one (`scheduler::_send_timer_handler`).
7. `spdlog::flush_every` MUST NOT be used: its thread holds a spdlog mutex that would hang a forked child. Log flushing uses the asio timer `init_log_flush_timer` (CMA-0011).
8. Exception: start-up code that runs before `io_context::run()` may block with a bounded timeout (CA bootstrap probe `WaitForConnected` 5 s, unary `GetCACertificate`). Nothing blocking is allowed once `run()` has started.
9. A second thread running `io_context::run()` MUST NOT be added. A need for parallelism is met by more concurrent checks (`max_concurrent_checks`), not by more threads.

## Consequences

- No data race in the scheduler and checks by construction; review only has to watch the boundaries listed above.
- A blocking call anywhere on the main thread delays every check and the metric export.
- Unit tests run the same `io_context` on a dedicated thread (`agent/test/test_main.cc`); a test that touches a check directly from the gtest thread while the io thread runs is racy.

## Guardrails

- `std::thread`, `std::async`, `CreateThread` added outside a worker-thread class with queue and kill function.
- `std::mutex` / `absl::Mutex` in `scheduler`, `check` or a native check that has no OS callback or worker thread.
- `std::this_thread::sleep_for`, `Sleep()`, synchronous `grpc` stub call, or `boost::process::wait` on the io thread.
- A lambda given to `async_wait` / `asio::post` capturing only `this`.
- A handler missing `if (err) return;`.

## References

- `agent/src/main.cc`, `agent/src/main_win.cc`, `agent/doc/agent-doc.md` (Introduction)
- `agent/src/bireactor.cc`, `agent/src/streaming_client.cc`, `agent/src/streaming_server.cc`
- `agent/src/drive_size.cc`, `agent/native_windows/src/check_files.cc`, `agent/native_windows/src/event_log/container.cc`
- `agent/test/test_main.cc`
