# Centreon Poller — Docker Compose

Ready-to-use Compose stack to run a Centreon poller as a container connected to an existing central server.

The image definition lives in `.github/docker/centreon-gorgone/`. This directory only holds the deployment recipe.

---

## Prerequisites

- Docker Engine 24+ with Compose v2 (`docker compose`)
- A poller already declared in the Centreon Web UI (you need its UUID, name, token, app secret and salt)
- Outbound network access from the poller host to the central server (`pullwss` initiates an outbound connection — no inbound port required)

---

## Quick start (from repo)

```bash
cd docker/poller

# 1. Copy the environment template
cp .env.example .env

# 2. Fill in your values (UUID, NAME, POLLER_TOKEN, CENTRAL_URL, APP_SECRET, SALT)
vi .env

# 3. Start
docker compose up -d

# 4. Follow logs
docker compose logs -f
```

---

## One-liner (no repo required)

The Centreon GUI generates this command pre-filled with your poller's values:

```bash
curl -fsSL https://cloud.centreon.com/poller/install.sh | bash -s -- \
  --poller_token <token> \
  --uuid <uuid> \
  --name <name> \
  --type docker \
  --central_url <host_or_ip[:port]> \
  --appsecret <secret> \
  --salt <salt>
```

The installer (`gorgone/install/cloud-installer/install.sh`) writes the same `.env` + `docker-compose.yml` to `/etc/centreon-poller-installer/` and runs `docker compose up -d` for you.

---

## Configuration (.env)

| Variable | Required | Default | Description |
|----------|----------|---------|-------------|
| `TAG` | no | `latest` | Image tag to pull |
| `TZ` | no | `UTC` | Timezone applied to the container |
| `DEBUG` | no | `false` | `true` / `1` enables verbose entrypoint logging |
| `UUID` | **yes** | — | Unique poller identifier (matches Centreon Web) |
| `NAME` | **yes** | — | Human-readable poller name |
| `POLLER_TOKEN` | **yes** | — | Authentication token for pullwss |
| `CENTRAL_URL` | **yes** | — | Central server URL — hostname or IP, optional `:port` and scheme |
| `APP_SECRET` | **yes** | — | Engine app secret (written to `engine-context.json`) |
| `SALT` | **yes** | — | Engine salt (written to `engine-context.json`) |

> `.env` contains secrets (`POLLER_TOKEN`, `APP_SECRET`, `SALT`) — never commit it. It is gitignored at the repo root.

---

## Volumes

All data is persisted in named Docker volumes, created automatically on first `docker compose up`.

| Volume | Mount point | Content |
|--------|-------------|---------|
| `poller-etc` | `/etc/centreon` | Shared Centreon config |
| `poller-engine` | `/etc/centreon-engine` | Engine config + `engine-context.json` |
| `poller-broker` | `/etc/centreon-broker` | Broker config |
| `poller-centcmd` | `/var/lib/centreon-engine/rw` | Engine command pipe |
| `poller-centcache` | `/var/cache/centreon` | Centreon cache |
| `poller-gorgone-data` | `/var/lib/centreon-gorgone` | Gorgone state + RSA keys |

---

## Useful commands

```bash
# View live logs
docker compose logs -f

# Restart gorgone
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

**Gorgone fails to start** — missing required env var:
- The container exits immediately with `ERROR: <VAR> env var must be set for poller mode`. Check that all of `UUID`, `NAME`, `POLLER_TOKEN`, `CENTRAL_URL`, `APP_SECRET`, `SALT` are set in `.env`.

**Gorgone cannot reach central**:
- Verify `CENTRAL_URL` (try with and without scheme/port).
- Check outbound connectivity: `docker exec centpoller-gorgone curl -kv https://$CENTRAL_HOST`.
- Verify `POLLER_TOKEN` matches the value generated for this poller in Centreon Web.

**`engine-context.json` not written**:
- Only generated in `TYPE=poller` mode (default in this compose).
- Path inside the container: `/etc/centreon-engine/engine-context.json` (volume `poller-engine`).
- Inspect: `docker exec centpoller-gorgone cat /etc/centreon-engine/engine-context.json`.
