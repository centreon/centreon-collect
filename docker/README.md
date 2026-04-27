# Centreon Collect — Docker Images

Docker image documentation and ready-to-use Compose stacks for the three main components of Centreon Collect.
Image definitions live in `.github/docker/`.

| Image | Base | Description |
|-------|------|-------------|
| `centreon-engine` | Debian 12 slim | Monitoring scheduler (centengine) |
| `centreon-gorgone` | Debian 12 slim | Orchestration daemon (gorgoned) |
| `centreon-broker` | Debian 12 slim | Event transmitter (cbd) |

---

## Table of Contents

- [Quick Start (Docker Compose)](#quick-start-docker-compose)
- [Build](#build)
- [Centreon Engine](#centreon-engine)
- [Centreon Gorgone](#centreon-gorgone)
- [Plugin Management](#plugin-management)
- [Log Streaming](#log-streaming)
- [CI/CD](#cicd)

---

## Quick Start (Docker Compose)

Ready-to-use Compose stacks are in `docker/` at the repo root.

### Poller (`docker/poller/`)

Runs a Centreon poller: one `centengine` container and one `centpoller-gorgone` container connected to an existing central server.

#### One-liner (no repo required)

This is the intended deployment method. The Centreon GUI generates this command with the values pre-filled for your poller:

```bash
curl -fsSL https://raw.githubusercontent.com/centreon/centreon-collect/<TAG>/docker/poller/docker-compose.yml \
  | POLLER_ID=<id> CENTRAL_ADDRESS=<central-ip> CENTRAL_PORT=443 GORGONE_TOKEN=<token> TAG=<tag> \
    docker compose -f - up -d
```

Example:

```bash
curl -fsSL https://raw.githubusercontent.com/centreon/centreon-collect/25.10.0/docker/poller/docker-compose.yml \
  | POLLER_ID=3 CENTRAL_ADDRESS=192.168.1.100 CENTRAL_PORT=443 GORGONE_TOKEN=my_secret_token TAG=25.10.0 \
    docker compose -f - up -d
```

To update a running poller (no downtime for unchanged containers):

```bash
# Re-run the same command — Compose recreates only what changed
curl -fsSL https://raw.githubusercontent.com/centreon/centreon-collect/25.10.1/docker/poller/docker-compose.yml \
  | POLLER_ID=3 CENTRAL_ADDRESS=192.168.1.100 CENTRAL_PORT=443 GORGONE_TOKEN=my_secret_token TAG=25.10.1 \
    docker compose -f - up -d
```

#### From repo (developers)

```bash
cd docker/poller

# 1. Create your local .env from the example
cp .env.example .env

# 2. Edit .env — set CENTRAL_ADDRESS, GORGONE_TOKEN, POLLER_ID, TAG
vi .env

# 3. Start
docker compose up -d

# 4. Follow logs
docker compose logs -f
```

#### Required environment variables

| Variable | Description |
|----------|-------------|
| `POLLER_ID` | Poller ID as defined in the Centreon central server |
| `CENTRAL_ADDRESS` | IP or hostname of the central server |
| `CENTRAL_PORT` | Gorgone port on the central server (default: `443`) |
| `GORGONE_TOKEN` | Authentication token shared with the central server |
| `TAG` | Image tag to pull (e.g. `25.10.0`, `latest`) |


---

## Build

All images use **multi-stage `Dockerfile.hybrid`** builds that extract binaries from `.deb` packages rather than installing them, reducing image size by 20–53%.

### Prerequisites

Debian `.deb` packages must be built first and placed in a `packages-centreon/` directory alongside the Dockerfile. The CI pipeline does this automatically; for local builds:

```bash
# Build packages first (see root CMakeLists.txt)
# Then place resulting .deb files in:
.github/docker/centreon-engine/bookworm/packages-centreon/
.github/docker/centreon-gorgone/bookworm/packages-centreon/
```

### Building locally

```bash
# Engine
docker build \
  --build-arg VERSION=25.10.0 \
  --build-arg STABILITY=stable \
  -f .github/docker/centreon-engine/bookworm/Dockerfile.hybrid \
  -t centreon-engine:local \
  .

# Gorgone (production — uses .deb packages)
docker build \
  --build-arg VERSION=25.10.0 \
  --build-arg STABILITY=stable \
  -f .github/docker/centreon-gorgone/bookworm/Dockerfile.hybrid \
  -t centreon-gorgone:local \
  .

# Gorgone (development — uses local source repo instead of .deb)
docker build \
  --build-arg USE_SOURCE_GORGONE=true \
  -f .github/docker/centreon-gorgone/bookworm/Dockerfile.hybrid \
  -t centreon-gorgone:dev \
  .
```

### Multi-platform builds

Both images support `linux/amd64` and `linux/arm64`:

```bash
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  -f .github/docker/centreon-engine/bookworm/Dockerfile.hybrid \
  -t centreon-engine:local \
  .
```

> **Note (arm64 / gorgone):** Some Perl modules are not published in the Centreon apt repository for arm64. The Dockerfile detects the architecture and compiles them from source via `cpanminus` automatically.

---

## Centreon Engine

### What's inside

| Path in image | Content |
|---------------|---------|
| `/usr/sbin/centengine` | Engine binary |
| `/usr/sbin/centenginestats` | Engine statistics binary |
| `/usr/lib/libcentreon_clib.so` | Core C library |
| `/usr/lib/cbmod.so` | Broker module loaded by engine |
| `/usr/lib64/centreon-engine/*.so` | Engine modules |
| `/usr/share/centreon/lib/centreon-broker/*.so` | Broker output modules |
| `/opt/centreon/venv/` | Python venv with FastAPI + Uvicorn (pre-installed) |
| `/var/lib/centreon-engine/` | Entrypoint scripts |
| `/etc/sudoers.d/centreon` | Sudoers rules for plugin installation |

### Exposed ports

| Port | Protocol | Usage |
|------|----------|-------|
| `8000` | HTTP | REST API (restart/reload centengine) |

### Environment variables

| Variable | Default | Description |
|----------|---------|-------------|
| `DEBUG` | `false` | Set to `true` or `1` to enable `set -x` in entrypoint |

### Running

```bash
docker run -d \
  --name centengine \
  -p 8000:8000 \
  -p 51001:51001 \
  -v /path/to/engine/config:/etc/centreon-engine \
  centreon-engine:local
```

### Configuration volume

Mount your engine configuration at `/etc/centreon-engine/`. The entrypoint expects at minimum:

```
/etc/centreon-engine/
├── centengine.cfg        # Main engine configuration
└── plugins.json          # Plugin list (optional, see Plugin Management)
```

### REST API

The engine exposes a lightweight FastAPI on port `8000`:

```bash
# Restart centengine
curl -X POST http://localhost:8000/restart

# Reload configuration (SIGHUP)
curl -X POST http://localhost:8000/reload
```

### Startup sequence

Scripts in `/var/lib/centreon-engine/container.d/` run in alphabetical order:

| Script | Type | Action |
|--------|------|--------|
| `00-init.sh` | sync | Refreshes apt package lists |
| `40-engine.sh` | sync | Installs plugins from `plugins.json` (background) |
| `45-plugin-watcher_background.sh` | background | Watches `plugins.json` for changes |
| `99-logs.sh` | sync | Writes `/tmp/docker.ready`, starts `api_control.py` |

---

## Centreon Gorgone

### What's inside

| Path in image | Content |
|---------------|---------|
| `/usr/bin/gorgoned` | Gorgone binary |
| `/usr/share/perl5/gorgone/` | Gorgone Perl modules |
| `/usr/share/perl5/centreon/` | Centreon shared Perl libraries |
| `/etc/centreon-gorgone/` | Default configuration files |
| `/var/lib/centreon-gorgone/` | Entrypoint scripts |
| `/usr/local/bin/gorgone_install_plugins.pl` | Plugin install helper |
| `/usr/sbin/centenginestats` | Stub returning fake stats (container compatibility) |
| `/usr/bin/systemctl` | Stub forwarding restart/reload to engine/broker HTTP APIs |

### Environment variables

| Variable | Default | Description |
|----------|---------|-------------|
| `TYPE` | _(empty)_ | `central` or `poller` — controls config generation |
| `USE_ETCD` | `false` | `true` to fetch config from etcd instead of shared volume |
| `ETCD_HOST` | `etcd` | etcd hostname |
| `ETCD_PORT` | `2379` | etcd port |
| `CONFIG_DIR` | `/etc/centreon/config.d` | Directory watched for config files |
| `MAX_RETRIES` | `10` | Retry attempts when waiting for config |
| `RETRY_DELAY` | `2` | Seconds between retries |
| `CONFIG_WAIT_TIMEOUT` | `300` | Total seconds to wait for config availability |
| `POLLER_ID` | _(required in poller mode)_ | Poller ID |
| `CENTRAL_ADDRESS` | _(required in poller mode)_ | Central server address |
| `CENTRAL_PORT` | _(required in poller mode)_ | Central gorgone port |
| `GORGONE_TOKEN` | _(required in poller mode)_ | Authentication token |
| `DEBUG` | `false` | Set to `true` or `1` to enable `set -x` in entrypoint |

### Modes

#### Central mode

```bash
docker run -d \
  --name centpoller-gorgone \
  -e TYPE=central \
  -v /path/to/centreon/config:/etc/centreon/config.d \
  centreon-gorgone:local \
  /usr/bin/perl /usr/bin/gorgoned \
    --config=/etc/centreon-gorgone/config.yaml \
    --severity=info
```

The config directory must contain `10-database.yaml` with MariaDB connection details:

```yaml
name: db
package: "gorgone::class::db"
db: centreon
host: mariadb
port: 3306
user: centreon
password: centreon
```

#### Poller mode

```bash
docker run -d \
  --name centpoller-gorgone \
  -e TYPE=poller \
  -e POLLER_ID=2 \
  -e CENTRAL_ADDRESS=central.example.com \
  -e CENTRAL_PORT=443 \
  -e GORGONE_TOKEN=my_secret_token \
  centreon-gorgone:local \
  /usr/bin/perl /usr/bin/gorgoned \
    --config=/etc/centreon-gorgone/config.yaml \
    --severity=info
```

Poller mode generates a minimal configuration with `action`, `engine`, and `pullwss` modules. No database config is required.

#### etcd-based configuration

Set `USE_ETCD=true` to fetch configuration from an etcd cluster instead of a shared volume. Gorgone watches for changes and automatically reloads on update:

```bash
docker run -d \
  --name centpoller-gorgone \
  -e TYPE=central \
  -e USE_ETCD=true \
  -e ETCD_HOST=etcd.example.com \
  -e ETCD_PORT=2379 \
  centreon-gorgone:local \
  /usr/bin/perl /usr/bin/gorgoned \
    --config=/etc/centreon-gorgone/config.yaml \
    --severity=info
```

### Startup sequence

Scripts in `/var/lib/centreon-gorgone/container.d/` run in alphabetical order:

| Script | Type | Action |
|--------|------|--------|
| `00-init.sh` | sync | Refreshes apt package lists |
| `05-fetch-etcd-config.sh` | sync | Waits for / fetches configuration (etcd or volume) |
| `06-watch-etcd-config_background.sh` | background | Watches etcd for config changes (only if `USE_ETCD=true`) |
| `10-create-config.sh` | sync | Generates gorgone config based on `TYPE` |
| `99-logs.sh` | sync | Writes `/tmp/docker.ready`, prints ready message |

After all scripts complete, `container.sh` hands off to `gosu centreon-gorgone` to run `gorgoned` as the correct user.

---

## Plugin Management

Both the engine and gorgone containers support runtime plugin installation via a `plugins.json` file.

### Format

```json
{
  "centreon-plugin-Applications-Protocol-Snmp": 20250300,
  "centreon-plugin-Hardware-Devices-Cisco-Asa-Snmp": 20250400
}
```

- **Key**: exact Debian package name
- **Value**: upstream version number (integer); the installed Debian version will be `<value>-1+deb12u1`, so a version is considered up-to-date if the installed version string _starts with_ the JSON value

### Engine — automatic installation and live watching

Place `plugins.json` at `/etc/centreon-engine/plugins.json`. The engine entrypoint:

1. **At startup** (`40-engine.sh`): installs any missing or outdated plugins
2. **At runtime** (`45-plugin-watcher_background.sh` + `check_plugins.py`): polls every 30 seconds; if the file changes, re-evaluates and installs only what is needed — already up-to-date plugins are skipped

### Gorgone — on-demand installation

Gorgone installs plugins when ordered to by the central server via the action module. The `gorgone_install_plugins.pl` script handles this using `apt-get`.

### Required apt repository

The Centreon plugins apt repository is pre-configured in the image:

```
deb https://packages.centreon.com/apt-plugins-stable/ bookworm main
```

> **arm64:** The plugins repository only publishes `amd64` packages. On arm64 hosts, plugin installation via apt will not work.

---

## Log Streaming

Both containers stream their service log files to `docker logs`.

### Engine

```
docker logs centengine
```

Log lines from `/var/log/centreon-engine/centengine.log` are prefixed with `[ENGINE-LOG]`:

```
[ENGINE-LOG] [2026-03-25 10:00:01] [info] Starting centengine...
```

### Gorgone

Gorgone logs to stdout directly — no prefix needed.

```
docker logs centpoller-gorgone
```

---

## CI/CD

### Workflows

| Workflow | Trigger | Builds |
|----------|---------|--------|
| `.github/workflows/docker-collect.yml` | PR / manual | `centreon-engine`, `centreon-broker` |
| `.github/workflows/docker-gorgone.yml` | PR / manual | `centreon-gorgone` |

Both workflows:
- Build `.deb` packages first
- Then build Docker images using `Dockerfile.hybrid`
- Target `linux/amd64` and `linux/arm64` via BuildKit multi-platform builds
- Push to the internal Docker registry

### Build args

| Arg | Description |
|-----|-------------|
| `VERSION` | Package version (parsed from `CMakeLists.txt` or `.version.*`) |
| `IS_CLOUD` | Cloud environment flag |
| `STABILITY` | `stable`, `testing`, etc. |
| `USE_SOURCE_GORGONE` | `true` to use local gorgone source instead of .deb (gorgone only) |
