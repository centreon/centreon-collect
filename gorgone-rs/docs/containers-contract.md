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
- An engine container has no external command file: no `externalcmd.so`, and
  the Engine configuration of its poller in centreon-web has neither that
  module nor `command_file` (the bench removes both when it registers or
  deploys the poller). External commands go through the engine gRPC API
  (`ProcessExternalCommand`) only. Anything that writes to `centengine.cmd`
  on such a poller (`centreontrapd`, event handler or custom scripts) must
  call that API instead.
- A broker container runs `cbd` and its own `gorgone-agent`, the `broker`
  component of its poller (see "Components of a node" in `DESIGN.md`),
  connected to gorgone-server like any agent: no port is open for control.
  The agent receives the broker configuration and the encryption key, and
  reloads or stops `cbd` with a signal.

- A centreon-engine container runs the `centreon-engine` program alone
  (`docs/scheduler-design.md`): no `gorgone-agent`, no `centengine`, no
  `cbd`, no broker module, no `.cfg` file. Configuration
  `/etc/centreon-engine/centreon-engine.toml` (format of the agent; the
  `GORGONE_AGENT__` variables and, over them, `CENTREON_ENGINE__` ones),
  state in `/var/lib/centreon-engine` (job journal, and the configuration in
  force under `engine-configuration/`, loaded again at start: the container
  filesystem, or a volume, must survive a restart for the poller to resume
  without a new push). centreon-web marks its poller with
  `nagios_server.engine_name = centreon-engine` and, with the feature flag
  `engine_structured_configuration`, sends it the structured configuration
  (`ApplyEngineConfiguration`) instead of the `engine`, `broker` and
  `vmware` pushes, after the `engine_context` push of the key that decrypts
  its `encrypt::` values (`/etc/centreon-engine/engine-context.json`, a
  directory the engine user can write). States, downtimes, comments and
  acknowledgements survive a restart in `/var/lib/centreon-engine/retention.json`.
  The engine checks and applies it in process
  and sends the results to the central broker input (TCP 5669) the
  document names. Its plugins are in the image (Debian
  `monitoring-plugins-basic`, reachable as `/usr/lib64/nagios/plugins`, the
  `$USER1$` centreon-web gives new pollers).

## Broker container image

- `cbd` is the main process; the entrypoint writes its pid to
  `/var/lib/centreon-broker/cbd.pid` just before starting it. SIGHUP reloads
  the configuration and `/etc/centreon-engine/engine-context.json`; SIGTERM
  stops `cbd`, and the container with it, which its restart policy starts
  again. The image has no Python, no RRD output (graphs come from
  VictoriaMetrics) and no legacy `sql`/`storage` output.
- The entrypoint starts `/usr/bin/gorgone-agent` with
  `/usr/local/lib/centreon-broker/gorgone-agent.toml` (or
  `GORGONE_AGENT_CONFIG`) when both exist, and starts it again if it ends.
  The bench adds them (`deploy/centreon/broker/agent.toml`):
  - targets `broker` (`/etc/centreon-broker`) and `engine_context`
    (`/etc/centreon-engine`, files written with mode 0640);
  - service `broker`: `reload = "signal:HUP:/var/lib/centreon-broker/cbd.pid"`,
    `restart = "signal:TERM:…"`;
  - `state_dir` on the container's own filesystem, so that the result of a
    restart is sent once the container is back.
- When `BROKER_WAIT_FOR_CONFIG=true`, the entrypoint does not start `cbd`
  while the configuration file is still the packaged default, and starts it
  as soon as another one is there (checked every second). Without it, it
  refuses the packaged default.

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
- `component` (default `engine`) names the part of the node the agent drives;
  the agent declares it with the names of its targets.
- A service operation `signal:<HUP|INT|TERM|USR1|USR2>:/absolute/pid/file`
  sends that signal to the process whose pid the file holds.

## Encryption key distribution

The key centreon-web encrypts credentials with (`engine-context.json`) is a
configuration target like the others, `engine_context`: with the
`gorgone_api_commands` flag on, every configuration push of a poller sends it
first. Each engine agent maps that target to its engine directory, and the
broker agent of the central poller to the key file of its container:
gorgone-server sends the push to both components of that poller.

centreon-web does not create the key when it runs as several replicas: each
replica would encrypt with its own. The key comes from outside, identical in
every web container: `CENTREON_ENGINE_CONTEXT` (the file content, from a
secret) or `ENGINE_CONTEXT_PATH` (a mounted secret file). The replicas also
share their PHP sessions through a Redis-protocol store
(`SESSION_REDIS_URL`), never through a shared directory.
