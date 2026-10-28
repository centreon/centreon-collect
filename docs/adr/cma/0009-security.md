# CMA-0009: Keep secrets encrypted until process start, and never trust an unverified peer

- **Status:** Proposed
- **Date:** 2026-10-28

## Context

The agent runs as a service on customer hosts (user `centreon-monitoring-agent` on Linux, a Windows service), runs commands sent by a remote poller, and receives credentials inside command lines. The mechanisms in place:

- **Transport.** `encryption` is `full` (TLS, server verified), `insecure` (TLS, no verification) or `no` (plain text). In plain text the agent logs a warning and stops after one hour (`main.cc`, `main_win.cc`).
- **Peer authentication.** In direct mode the agent sends `token`. In reversed mode `token` becomes the trusted set checked in `streaming_server::Import` (`is_token_valid`); a refusal is answered with an `ImmediateFinishReactor`, and a JWT expiry closes the stream.
- **CA distribution.** With `fingerprint` and no CA, `ca_bootstrap_workflow` (direct mode only) probes full TLS; on failure it fetches the CA with `CACertificateService` over `TLS_SKIP_VERIFY_CA`, accepts it only if its SHA-256 fingerprint matches, and persists it: on Linux a file owned by the agent account plus the JSON config, on Windows the Local Machine certificate store, or a file plus the registry if the store fails. It logs the change at CRITICAL with `[SECURITY]`.
- **Credentials.** Engine sends `key` and `salt` only on TLS streams and only if `AgentInfo.encryption_ready`; command lines then start with `encrypt::` (AES-256, `common/crypto/aes256`). The agent decrypts at build time, re-encrypts the arguments in `process_args`, decrypts them just before `start_process` and clears the plain text right after.
- **Local commands.** Custom checks come from a file owned by the host administrator; Engine only names them. Command lines are split by `common::process::parse_cmd_line` and run without a shell.

Alternatives weighed:

- Keep decrypted command lines in memory for the life of the check. It lost: a memory dump or a log would expose every credential.
- Fall back to `insecure` when a `full` handshake fails. It lost: a man-in-the-middle could force the downgrade.
- Distribute the CA by hand only. It lost: installers on many hosts need a pinned, verifiable bootstrap; the fingerprint gives the same trust as a copied file.

## Decision

Keep the plain-text warning and the one-hour stop, with no way to disable them. Set TLS through `common::grpc::grpc_config` only, keep `insecure` an explicit configuration choice, and use `TLS_SKIP_VERIFY_CA` only in `grpc_ca_bootstrap.cc`, discarding the fetched CA unless its fingerprint matches.

Hold plain-text credentials only for the duration of `start_process`: follow every `decrypt_args` with `clear_unencrypted_args`, on every path. Hold key material only in `common::crypto::aes256` objects. Never log a decrypted command line, a token, a key, a salt or a private key, and never put them in a check output. Print a command line in a new log line only at TRACE. Check tokens before creating any `server_reactor` or scheduler.

Run commands only from the Engine configuration or the local custom checks file, without a shell: no `system()`, `popen()`, `cmd.exe /c` or `/bin/sh -c` in agent code. Log trust changes at CRITICAL with `[SECURITY]`. Get a review from `@centreon/owners-security`, with a robot case for the failure path (wrong token, wrong fingerprint, plain-text expiry), for any change in this scope.

## Consequences

- Secrets are in clear only while a process starts.
- Plain-text mode serves demos, not production; the stop is deliberate.
- `get_command_line()` returns the stored text: still `encrypt::` ciphertext when encrypted, plain text otherwise.
- On Windows the CA file owner is not changed; protection relies on the certificate store or the installer ACLs.
- Known defect: timeout and execution-failure outputs embed `get_command_line()` (`check.cc`, `check_exec.cc`, `check_dummy`), and `scheduler::_check_handler` logs it at DEBUG, `check_exec::start_check` at ERROR. For a non-encrypted command, plugin arguments, possibly passwords, reach Engine, the UI and the logs. The fix outputs the command name only.
- The plain-text expiry timer is duplicated in `main.cc` and `main_win.cc` and is kept in sync until factored.

## When to Amend

- Engine stops sending credentials in command lines (vault references resolved by the agent, for instance).
- Mutual TLS becomes mandatory, making tokens redundant, or plain-text transport is removed altogether.
