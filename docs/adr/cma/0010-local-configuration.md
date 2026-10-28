# CMA-0010: Keep local configuration keys identical across the JSON file, the registry and the installers

- **Status:** Proposed
- **Date:** 2026-10-28

## Context

The agent has two kinds of configuration. The remote one (what to check, how often) comes from Engine (CMA-0004) and is never stored locally. The local one (how to connect, log and identify) is read once at start-up by `config`, a process-wide singleton (`config::load`, `config::instance()`).

The local configuration has one set of keys and four places that must agree:

- `src/config.cc` reads `/etc/centreon-monitoring-agent/centagent.json` on Linux, validated against `config::config_schema` by `common::json_validator` (template `conf/centagent.json.in`);
- `src/config_win.cc` reads `HKLM\SOFTWARE\Centreon\<service name>` on Windows (default `CentreonMonitoringAgent`, `--service-name` for other instances);
- `installer/scripts_linux/install_cma.sh` writes the JSON file;
- `installer/installer.iss.in`, `installer/install_cma.ps1` and `conf/centagent.reg` write the registry.

Keys: `host`, `host_template`, `endpoint`, `encryption`, `public_cert`, `private_key`, `ca_certificate` (JSON alias `ca`), `ca_name` (JSON alias `ca_common_name`), `fingerprint`, `reversed_grpc_streaming`, `log_level`, `log_type`, `log_file`, `log_max_file_size`, `log_max_files`, `second_max_reconnect_backoff`, `max_message_length`, `token`, `custom_check_file`. Only `endpoint` is required.

Custom checks: `custom_check_file` points to an INI-like file (`conf/custom_check.ini`): `[section]` and `;comment` lines are skipped, `name=command` lines carry `$ARGn$` placeholders. A `common::file_watcher` reloads it on change through `config::reload_custom_checks()`; a malformed file keeps the previous set. New commands apply to checks built from the next Engine configuration.

Alternatives weighed:

- One configuration file on both platforms. It lost: the Windows installer, silent installs and multi-instance services are built on the registry, and Windows administrators manage it with their usual tools.
- Hot reload of every option. It lost: connection and log settings would need a reconnect path and partial-failure rules; only custom checks change often enough to justify it.

## Decision

Add a new local option, in the same change, to the JSON schema and parser, the registry reader, the getter in `config.hh`, the Linux installer, the Windows installer and silent-install script, the default template when its value is not the default, and the documentation. Use the same snake_case key everywhere. Give it a default that keeps the previous behaviour, and state its unit in its name or description.

Keep every JSON alias accepted forever, mutually exclusive by schema, and keep the old name as an alias when renaming a key. Stop start-up with a message naming the key when validation fails, and check in code what the schema cannot express, as is done for `endpoint`.

Treat `config::instance()` as read-only after start-up. Reload custom checks all or nothing: parse into a temporary map, replace on success only, log each loaded entry at INFO. Use the test-only constructor `config(bool)` in tests only.

## Consequences

- An option that exists on one platform only is a bug for the users of the other platform.
- The singleton keeps call sites simple but is a hidden dependency of `check_custom` and the scheduler; tests use `config::load(bool)` or check `instance_ptr()`.
- The registry uses the canonical key names only; aliases exist in JSON.
- Platform-specific value: Windows `log_type` defaults to `event-log`, which the JSON schema does not have.
- To verify: `conf/centagent.json.in` sets `"encryption": false` while the schema expects `full`, `insecure` or `no`. The template is aligned on `"no"` once checked.
- The schema `endpoint` pattern differs from the code regex (which allows `-`); the code check is the reference.
- `log_max_file_size`, `log_max_files` and `second_max_reconnect_backoff` use `"min"`, which is not a JSON-schema keyword, so their bound is not enforced.

## When to Amend

- Another option needs to change without restart.
- The Windows agent moves to a configuration file, or the Linux installer moves to another format.
