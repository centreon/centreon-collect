# CMA-0010 — Local configuration: JSON, registry, installers, custom checks

- Status: Proposed
- Owner: @centreon/owners-cpp
- Scope: `agent/{inc,src}/config*`, `agent/conf/**`, `agent/installer/**`, `packaging/centreon-monitoring-agent/**`

## Context

Two kinds of configuration:

- Remote: what to check and how often, sent by Engine (CMA-0004). Never stored locally.
- Local: how to connect, log and identify, read once at start-up by `config`, a process-wide singleton (`config::load`, `config::instance()`).

Local configuration has one schema and four writers that must agree:

| Writer / reader | Platform | Format |
| --- | --- | --- |
| `src/config.cc` (reader, JSON schema `config::config_schema`, validated by `common::json_validator`) | Linux | `/etc/centreon-monitoring-agent/centagent.json` (template `conf/centagent.json.in`) |
| `src/config_win.cc` (reader) | Windows | registry `HKLM\SOFTWARE\Centreon\<service name>` (default `CentreonMonitoringAgent`) |
| `installer/scripts_linux/install_cma.sh` (writer) | Linux | JSON |
| `installer/installer.iss.in`, `installer/install_cma.ps1`, `conf/centagent.reg` (writers) | Windows | registry |

Keys (same names in JSON and registry): `host`, `host_template`, `endpoint`, `encryption`, `public_cert`, `private_key`, `ca_certificate` (JSON alias `ca`), `ca_name` (JSON alias `ca_common_name`), `fingerprint`, `reversed_grpc_streaming`, `log_level`, `log_type`, `log_file`, `log_max_file_size`, `log_max_files`, `second_max_reconnect_backoff`, `max_message_length`, `token`, `custom_check_file`.

Custom checks: `custom_check_file` points to an INI-like file (`conf/custom_check.ini`): `[section]` and `;comment` lines skipped, `name=command` lines with `$ARGn$` placeholders. A `common::file_watcher` reloads it on change; a malformed file keeps the previous set. New commands apply to checks built at the next configuration from Engine.

## Decision

1. A new local option MUST be added in the same PR to: the JSON schema and parser (`config.cc`), the registry reader (`config_win.cc`), the getter in `config.hh`, the Linux installer, the Windows installer and silent-install script, the default template if it has a non-default value, and `agent/doc` or the installer README. Same key name everywhere, snake_case.
2. Every option MUST have a default that keeps the previous behaviour; `endpoint` stays the only required key.
3. JSON aliases are permanent: `ca`/`ca_certificate` and `ca_common_name`/`ca_name` MUST both stay accepted, mutually exclusive by schema (`allOf/not/required`). The registry uses the canonical names only. A renamed key keeps the old one as alias.
4. Validation errors MUST stop start-up with a message naming the key (`main` returns non-zero). Values the schema cannot express (endpoint regex) are checked in code after validation, as done for `endpoint`.
5. Units are explicit in the key or its description: `max_message_length` in MiB (minimum 4), `log_max_file_size` in MiB, `second_max_reconnect_backoff` in seconds.
6. `config::instance()` is read-only after start-up. The only reload allowed is `config::reload_custom_checks()`, called from the scheduler on the `io_context` thread. A new hot-reloaded setting needs an ADR (thread model and partial-failure semantics).
7. Custom check reload MUST be all-or-nothing: parse into a temporary map, replace on success only, log each loaded entry at `INFO`.
8. The UT-only constructor `config(bool reverse_connection)` MUST NOT be used outside tests.

## Consequences

- An option that exists only on one platform is a bug for the other platform's users.
- The singleton keeps call sites simple but makes `config` a hidden dependency of `check_custom` and the scheduler; tests use `config::load(bool)` or `instance_ptr()` checks.

## Guardrails

- A key read in `config.cc` but absent from `config_win.cc` or an installer (or the reverse).
- A key renamed without alias, or a default changed.
- A mutable static or a setter added to `config`.
- File reload logic that mutates `_custom_checks` before the file is fully parsed.

## Known deviations

- Windows `log_type` defaults to `event-log`, a value absent from the JSON schema (Linux has no event log). Platform-specific value, documented here on purpose.
- `conf/centagent.json.in` sets `"encryption": false` (boolean) while the schema declares the enum `full|insecure|no`. To verify that the packaged file passes validation; align the template on `"no"`.
- The schema `endpoint` pattern (`[\w\.:]+:\w+`) differs from the code regex (`[\w\.\-:]+:\w+`, hyphen allowed); the comment says the schema pattern does not work. Keep the code check as reference.
- `log_max_file_size` / `log_max_files` / `second_max_reconnect_backoff` use `"min"`, which is not a JSON-schema keyword (`minimum`); the bound is not enforced.

## References

- `agent/inc/com/centreon/agent/config.hh`, `agent/src/config.cc`, `agent/src/config_win.cc`
- `agent/conf/centagent.json.in`, `agent/conf/centagent.reg`, `agent/conf/custom_check.ini`
- `agent/installer/scripts_linux/install_cma.sh`, `agent/installer/installer.iss.in`, `agent/installer/install_cma.ps1`
- `agent/test/config_test.cc`
- `packaging/centreon-monitoring-agent/centreon-monitoring-agent.yaml`
