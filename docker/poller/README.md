# Centreon Poller — Docker Compose

This stack runs a Centreon poller using two containers:

| Container | Image | Role |
|-----------|-------|------|
| `centpoller` | `centreon-engine-bookworm` | Executes monitoring checks (centengine) |
| `centpoller-gorgone` | `centreon-gorgone-bookworm` | Communicates with the central server (gorgoned) |

Gorgone connects back to the central server over a persistent WebSocket (`pullwss`), receives engine configuration, and forwards check results. No inbound port needs to be open on the poller host.

---

## Prerequisites

- Docker Engine 24+ with Compose v2 (`docker compose`)
- Network access from the poller host to the central server on `CENTRAL_PORT` (default 443)
- A poller already declared in the Centreon web UI, with its ID and gorgone token available

---

## Quick start

```bash
# 1. Copy the environment template
cp .env.example .env

# 2. Fill in your values
vi .env

# 3. Start the stack
docker compose up -d

# 4. Follow logs
docker compose logs -f
```

---

## Configuration (.env)

| Variable | Required | Default | Description |
|----------|----------|---------|-------------|
| `TAG` | no | `latest` | Image tag to pull (e.g. `25.10.0`, `fth-docker-collect`) |
| `TZ` | no | `America/Toronto` | Timezone applied to both containers |
| `DEBUG` | no | `false` | Set to `true` to enable verbose entrypoint logging |
| `POLLER_ID` | **yes** | — | Poller ID as declared in Centreon web UI |
| `CENTRAL_ADDRESS` | **yes** | — | IP or hostname of the central Centreon server |
| `CENTRAL_PORT` | no | `443` | Port gorgone uses to reach the central server |
| `GORGONE_TOKEN` | **yes** | — | Shared secret between poller and central gorgone |

Example `.env`:

```dotenv
TAG=25.10.0
TZ=Europe/Paris
DEBUG=false
POLLER_ID=3
CENTRAL_ADDRESS=central.example.com
CENTRAL_PORT=443
GORGONE_TOKEN=my_very_secret_token
```

> `.env` is gitignored — never commit it.

---

## Volumes

All data is persisted in named Docker volumes. They are created automatically on first `docker compose up`.

| Volume | Mounted in | Content |
|--------|-----------|---------|
| `poller-engine` | `/etc/centreon-engine` | Engine configuration pushed by central |
| `poller-broker` | `/etc/centreon-broker` | Broker configuration |
| `poller-gorgone` | `/etc/centreon-gorgone` | Gorgone configuration |
| `poller-etc` | `/etc/centreon` | Shared Centreon config (credentials, etc.) |
| `poller-centcore` | `/var/lib/centreon/centcore` | Centcore command pipe |
| `poller-centcmd` | `/var/lib/centreon-engine/rw` | Engine command pipe (shared between both containers) |
| `poller-centlog` | `/var/log/centreon-engine` | Engine log files |
| `poller-centcache` | `/var/cache/centreon` | Centreon cache |
| `poller-gorgone-keys` | `/var/lib/centreon-gorgone/.keys` | Gorgone RSA keys (generated on first start) |

---

## Ports

No ports need to be exposed. Gorgone initiates an outbound WebSocket connection (`pullwss`) to the central server — no inbound rules required on the poller host.

---

## Plugin management

Plugins are installed automatically by the engine container at startup and kept up-to-date at runtime.

Push a `plugins.json` file into the `poller-engine` volume (at `/etc/centreon-engine/plugins.json`) via the central server, or copy it manually:

```json
{
  "centreon-plugin-Applications-Protocol-Snmp": 20250300,
  "centreon-plugin-Operatingsystems-Linux-Snmp": 20250300
}
```

The engine container watches this file every 30 seconds. When it changes, only missing or outdated packages are installed — no container restart needed.

> Plugin packages are fetched from `https://packages.centreon.com/apt-plugins-stable/`.
> The poller host needs outbound internet access to that URL, or a local mirror must be configured.

---

## Useful commands

```bash
# View live logs for both containers
docker compose logs -f

# View logs for a single container
docker compose logs -f gorgone
docker compose logs -f centengine

# Restart a single service
docker compose restart gorgone

# Stop the stack (volumes are preserved)
docker compose down

# Stop and delete all volumes (full reset)
docker compose down -v

# Check container status
docker compose ps
```

---

## Troubleshooting

**Gorgone cannot connect to central**
- Verify `CENTRAL_ADDRESS` and `CENTRAL_PORT` in `.env`
- Verify `GORGONE_TOKEN` matches the one configured on the central server
- Check outbound connectivity: `docker exec centpoller-gorgone curl -k https://$CENTRAL_ADDRESS:$CENTRAL_PORT`

**Engine has no configuration**
- Configuration is pushed by gorgone after it registers with central
- Wait a few seconds after the stack starts, then check: `docker exec centpoller ls /etc/centreon-engine/`
- Check gorgone logs for registration errors: `docker compose logs gorgone`

**Plugin install fails**
- Check that the poller host has outbound access to `packages.centreon.com`
- Check engine logs: `docker compose logs centengine | grep -i plugin`
