# Containers without shared volumes: contract

Status: proof of concept. Each container owns its files; configuration and
control go over the network. A container may keep its own data volume (the
database, VictoriaMetrics), never one shared with another container.

## Who runs where

- `web`: centreon-web only. No RRD: graphs come from VictoriaMetrics, so the
  image carries no `rrdtool`.
- An engine container runs `centengine` and its `gorgone-agent` in the same
  container (no sidecar): the configuration directories and the engine API
  socket are local to it. Restart = the agent asks the engine to exit and the
  container restart policy starts both again; the agent's durable journal
  (`state_dir`, on the container's own filesystem) resends the result of that
  job after the restart.
- A broker container runs `cbd` behind its control API. It receives its
  configuration and the encryption key through that API, pushed by the agent
  of its poller.

## Broker control API (broker container image)

- `PUT /files/{target}` writes the files of a plain tar body
  (`Content-Type: application/x-tar`) into a destination directory: regular
  files only (no link, no absolute path, no `..`), 64 MiB at most, each file
  written atomically. Targets:
  - `broker`: `/etc/centreon-broker`;
  - `engine_context`: `/etc/centreon-engine`, for the key file
    `engine-context.json`, written with mode 0640.
- It answers `204` on success, `400` on a bad archive or target, `401` without
  the token and `413` above the size limit.
- `POST /reload`, `POST /restart`, `GET /health` as today.
- When `BROKER_CONTROL_TOKEN` is set, every endpoint except `GET /health`
  requires `Authorization: Bearer <token>` (constant-time comparison).
- When `BROKER_WAIT_FOR_CONFIG=true`, the container starts the control API
  without `cbd` while the configuration file is still the packaged default,
  and starts `cbd` as soon as a configuration has been pushed. `GET /health`
  answers `503 {"state": "waiting_for_config"}` meanwhile.

## gorgone-agent

- A target writes into a local `path`, sends to remote `urls`
  (`PUT`, plain tar), or both; `ApplyConfig` succeeds only if every
  destination accepted it. Engine validation (`engine_main_file`) needs a
  local `path`.
- `file_mode` (octal, e.g. `"0640"`) forces the mode of the files written
  locally for a target.
- `http_bearer_token` (top-level, secret) is sent as `Authorization: Bearer`
  on every `http-post:` service action and remote target call.
- The applied digest of a remote destination is the last one it accepted,
  kept by the agent (durable with `state_dir`), and reported in heartbeats
  like a local one.

## Encryption key distribution

The key centreon-web encrypts credentials with (`engine-context.json`) is a
configuration target like the others, `engine_context`: with the
`gorgone_api_commands` flag on, every configuration push of a poller sends it
first. Each agent maps that target to its engine directory, and the central
poller's agent also to its broker container.
