# CMA-0009 — Security: transport, credentials, CA bootstrap, tokens

- Status: Proposed
- Owner: @centreon/owners-cpp, review by @centreon/owners-security for any change
- Scope: `agent/src/config*.cc`, `agent/src/main*.cc`, `agent/{inc,src}/grpc_ca_bootstrap.*`, `agent/{inc,src}/check_exec.*`, `agent/src/scheduler.cc` (decrypt), `agent/src/streaming_server.cc` (tokens)

## Context

The agent runs as a service on customer hosts (`centreon-monitoring-agent` user on Linux, a Windows service), executes commands sent by a remote poller, and receives credentials embedded in command lines. Current mechanisms:

| Concern | Mechanism |
| --- | --- |
| Transport | `encryption`: `full` (TLS, server verified), `insecure` (TLS, no verification), `no` (plain text). Plain text logs a warning and the agent stops after 1 hour (`main.cc`, `main_win.cc`). |
| Peer authentication | direct mode: agent sends `token`; reversed mode: `token` becomes the trusted token set checked in `streaming_server::Import` (`is_token_valid`), JWT expiry closes the stream. |
| CA distribution | `fingerprint` without CA: `ca_bootstrap_workflow` probes full TLS, else fetches the CA with `CACertificateService` over TLS without verification, accepts it only if its SHA-256 fingerprint matches, persists it (Linux: file owned by the agent account + JSON config; Windows: Local Machine certificate store first, file + registry only if the store fails). Direct mode only. |
| Credentials | Engine sends `key`/`salt` only on TLS streams and only if `AgentInfo.encryption_ready`; command lines then start with `encrypt::` (AES-256, `common/crypto/aes256`). The agent decrypts at build time, re-encrypts arguments in `process_args`, decrypts them just before `start_process`, clears plain text right after. |
| Local commands | custom checks come from a local file owned by the host admin (CMA-0010); Engine can only reference them by name. |

## Decision

1. `encryption: no` MUST keep the warning and the 1-hour stop. No option, environment variable or build flag may remove them.
2. TLS settings (cipher, verification, keepalive) MUST be set through `common::grpc::grpc_config`, never on a raw channel. `insecure` mode MUST stay explicit in configuration; it is never a fallback after a failed `full` handshake. The only TLS-without-verification connection allowed in code is the CA fetch of `ca_bootstrap_workflow`, and its result MUST be discarded unless the fingerprint matches.
3. Plain-text credentials MUST NOT be stored in a long-lived member, logged, put in an output, or sent back to Engine. The `check_exec` sequence decrypt → start → `clear_unencrypted_args` MUST be preserved on every path, including exceptions.
4. New log lines print a command line only at `TRACE` level (pattern `if (_logger->level() == spdlog::level::trace)` in the scheduler). New logs at `DEBUG` or above log the service name and command name, not the command line. `get_command_line()` returns the stored text, still `encrypt::`-prefixed ciphertext when encrypted, plain text otherwise.
5. Key material received from Engine MUST be held only in `common::crypto::aes256` objects; an invalid key resets the decrypter and logs an error without the key.
6. Token values MUST NOT be logged. A failed token check returns the gRPC status (`ImmediateFinishReactor`) before any `server_reactor` or scheduler is created.
7. File writes performed by the agent (CA bootstrap only, today) MUST target the configured or platform default path, set the owner to the agent account on Linux (Windows relies on the certificate store or the installer ACLs), and MUST log at `CRITICAL` with the `[SECURITY]` tag when trust material changes.
8. The agent MUST NOT execute a command that comes from anywhere else than: the Engine configuration (`check_exec`) or the local custom checks file (`check_custom`). No shell is spawned by the agent itself: command lines are split by `common::process::parse_cmd_line` and executed directly.
9. Any change in this scope requires a reviewer from `@centreon/owners-security` and a robot test for the failure path (wrong token, wrong fingerprint, plain-text expiry).

## Consequences

- Secrets are in clear only for the duration of a `start_process` call.
- Plain-text mode is usable for a demo, not for production: the stop is deliberate.

## Guardrails

- A log line at `INFO`/`DEBUG`/`WARN`/`ERROR` that prints `command_line`, `get_command_line()` after decrypt, `token`, `key`, `salt`, private key content or PEM.
- Removal or bypass of the 1-hour plain-text stop.
- `grpc_config::TLS_SKIP_VERIFY_CA` (or any verification-disabling option) used outside `grpc_ca_bootstrap.cc`.
- `system()`, `popen()`, `cmd.exe /c`, `/bin/sh -c` added in agent code.
- A new path that calls `decrypt_args` without a matching `clear_unencrypted_args`.

## Known deviations

- Timeout and execution-failure outputs embed `get_command_line()` (`check.cc`, `check_exec.cc`, `check_dummy`). For a non-encrypted command line this sends plugin arguments, possibly passwords, back to Engine and to the UI. To fix: output the command name only.
- `scheduler::_check_handler` logs `get_command_line()` at `DEBUG` and `check_exec::start_check` at `ERROR`: plain text for non-encrypted commands. Do not copy; fix with the output fix above.
- The plain-text expiry timer is duplicated in `main.cc` and `main_win.cc`; keep both in sync until factored.

## References

- `agent/src/main.cc`, `agent/src/main_win.cc`
- `agent/src/grpc_ca_bootstrap.cc`, `agent/inc/com/centreon/agent/grpc_ca_bootstrap.hh`
- `agent/src/check_exec.cc`, `common/process/` (`process_args`), `common/crypto/`
- `agent/src/streaming_server.cc`, `common/grpc/`
- `engine/modules/opentelemetry/src/centreon_agent/agent_impl.cc`
