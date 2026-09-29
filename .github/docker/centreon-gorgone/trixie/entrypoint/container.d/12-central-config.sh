#!/bin/sh
# TYPE=central: write config.d/{30-centreon,31-centreon-api,40-gorgoned}.yaml
# directly, the same standard config.yaml -> !include config.d/*.yaml path
# TYPE=poller already uses (not a shell-generated file there, but the same
# destination) — so no compose `command:` override is needed for central mode
# any more than for poller mode, and 39-action.yaml/whitelist.conf.d (baked
# unconditionally earlier in the Dockerfile) apply here for free.
#
# Heredocs here are intentionally unquoted (<<EOF, not <<'EOF'): env vars
# expand directly into the YAML as it's written, no separate sed/templating
# step. tpapi (31-centreon-api.yaml) is the one block that specifically CANNOT
# go through gorgone's own GORGONE__ env var mechanism instead:
# gorgone::class::script::load_env_config crashes ("Not a HASH reference") on
# any GORGONE__GORGONE__TPAPI__* override — tpapi is a named list exactly like
# `modules` structurally, but only `modules` gets the special
# array-search-by-name handling. Confirmed by direct reproduction, not fixed
# on the gorgone side — written here instead.

set -e

TYPE="${TYPE:-central}"

if [ "$TYPE" != "central" ]; then
    echo "=== Central config: not central mode, skipping ==="
    return 0
fi

echo "=== Writing central config.d ==="

CONFIGD="/etc/centreon-gorgone/config.d"
mkdir -p "$CONFIGD/cron.d"

# Real cron.d content (relayed by the docker-web-split session, `cat`/`tree`
# off a real central). LAUNCHSERVICEDISCOVERY needs the `autodiscovery`
# module, a known gap (see 40-gorgoned.yaml's own comment below) — this job is
# harmless to have scheduled either way, it just won't find a handler if it
# ever fires before that module is added. Helios keeps its real `.disabled`
# suffix, which is the actual mechanism that excludes it from the
# `cron.d/*.yaml` glob — matches its default-off state in production, not
# something to activate to "complete" this list.
cat > "$CONFIGD/cron.d/41-service-discovery.yaml" <<'EOF'
- id: service_discovery
  timespec: "30 22 * * *"
  action: LAUNCHSERVICEDISCOVERY
EOF

cat > "$CONFIGD/cron.d/43-centreon-helios.yaml.disabled" <<'EOF'
- id: centreon_helios
  timespec: "0 0 * * *"
  action: COMMAND
  parameters:
    - command: "/usr/bin/php -q /usr/share/centreon/cron/centreon-helios.php >> /var/log/centreon-gorgone/centreon-helios.log 2>&1"
      timeout: 120
EOF

CENTREON_API_BASE_URL="${CENTREON_API_BASE_URL:?ERROR: CENTREON_API_BASE_URL must be set for central mode}"
CENTREON_API_USERNAME="${CENTREON_API_USERNAME:?ERROR: CENTREON_API_USERNAME must be set for central mode}"
CENTREON_API_PASSWORD="${CENTREON_API_PASSWORD:?ERROR: CENTREON_API_PASSWORD must be set for central mode}"

# Real production splits DB connectivity into 30-centreon.yaml, which does
# `centreon: !include /etc/centreon/config.d/*.yaml` — a directory centreon-
# web's own PHP installer writes at install time, out of scope for this image
# to replicate. DSN/credentials are injected via GORGONE__CENTREON__DATABASE__*
# env vars instead (confirmed safe, works even with no dsn: line present at
# all) — the placeholders below are only ever seen if those aren't set.
cat > "$CONFIGD/30-centreon.yaml" <<EOF
name: 30-centreon.yaml
centreon:
  database:
    db_configuration:
      dsn: "mysql:host=REPLACE_ME;dbname=centreon"
      username: centreon
      password: centreon
    db_realtime:
      dsn: "mysql:host=REPLACE_ME;dbname=centreon_storage"
      username: centreon
      password: centreon
EOF

# gorgone's OWN login to the Centreon Web API (a real web user, "centreon-
# gorgone" in production), used to validate OTHER callers' tokens (pullwss
# poller tokens, see gorgone/modules/core/proxy/httpserver.pm:502-524) — not
# the poller token itself. Real production splits this into TWO files
# (30-centreon.yaml, external https://<host>/... + 31-centreon-api.yaml, local
# http://127.0.0.1/..., both centreonv2+clapi) — deliberately not replicated
# here: in a split-container topology nothing is actually 127.0.0.1-reachable
# between gorgone-central and httpd (separate containers), so the "local"
# variant's premise doesn't hold, and per Hash::Merge's RIGHT_PRECEDENT
# array-replacement behavior only the LAST-loaded config.d file's tpapi array
# survives the merge anyway. One real, working base_url instead.
cat > "$CONFIGD/31-centreon-api.yaml" <<EOF
name: 31-centreon-api.yaml
gorgone:
  tpapi:
    - name: centreonv2
      base_url: "$CENTREON_API_BASE_URL"
      username: "$CENTREON_API_USERNAME"
      password: "$CENTREON_API_PASSWORD"
    - name: clapi
      username: "$CENTREON_API_USERNAME"
      password: "$CENTREON_API_PASSWORD"
EOF

# Module set modeled directly on a real production central's own
# config.d/40-gorgoned.yaml (relayed verbatim by the docker-web-split
# session), with three deliberate, load-bearing deviations for this topology:
#   - gorgonecore.id 1000, not 1 (Central's real nagios_server id, matching
#     the poller-container's own GORGONE_UID). If this process's own id
#     matched the node id it dispatches REMOTECOPY to, core.pm:649's
#     self-target-nulling shortcut would null the dispatch, reproducing
#     ticket 11's "[proxy] need a valid node id" self-collision. Confirmed
#     both ways by direct testing.
#   - proxy.httpserver binds 0.0.0.0, not localhost: httpd is a separate
#     container reaching this one by service name over the network
#     (GORGONE_WSS_UPSTREAM=gorgone-central:8087 on the httpd side), not the
#     same host the real value assumes.
#   - no `register` module: the real central doesn't use one either — `nodes`'
#     own DB sync is sufficient once nagios_server.localhost is 0 for
#     Central's row, which is what actually gets id 1 into register_nodes as
#     a normal push_zmq/pullwss-shaped entry, not localhost==1's
#     special-cased exclusion.
# Known gaps versus the real module list, not yet included: `engine` (this
# process doesn't share /var/lib/centreon-engine/rw with centengine —
# gorgone-central-poller does, for exactly that reason — RELOAD/RESTART and
# ENGINECOMMAND still go through that second process today), `statistics`,
# `audit`, `autodiscovery` (cron.d/41-service-discovery.yaml's
# LAUNCHSERVICEDISCOVERY job is already scheduled, matching production, but
# has no handler until this module is added).
cat > "$CONFIGD/40-gorgoned.yaml" <<'EOF'
name: 40-gorgoned.yaml
gorgone:
  gorgonecore:
    privkey: "/var/lib/centreon-gorgone/.keys/rsakey.priv.pem"
    pubkey: "/var/lib/centreon-gorgone/.keys/rsakey.pub.pem"
    id: 1000

  modules:
    - name: httpserver
      package: "gorgone::modules::core::httpserver::hooks"
      enable: true
      address: "0.0.0.0"
      port: "8085"
      ssl: false
      auth:
        enabled: false
      allowed_hosts:
        enabled: true
        subnets:
          - 127.0.0.1/32
          - 172.16.0.0/12

    - name: cron
      package: "gorgone::modules::core::cron::hooks"
      enable: true
      cron: !include /etc/centreon-gorgone/config.d/cron.d/*.yaml

    - name: nodes
      package: "gorgone::modules::centreon::nodes::hooks"
      enable: true

    - name: proxy
      package: "gorgone::modules::core::proxy::hooks"
      enable: true
      httpserver:
        enable: true
        ssl: false
        address: "0.0.0.0"
        port: 8087

    - name: legacycmd
      package: "gorgone::modules::centreon::legacycmd::hooks"
      enable: true
      cmd_dir: "/var/lib/centreon/centcore/"
      cmd_file: "/var/lib/centreon/centcore.cmd"
      cache_dir: "/var/cache/centreon/"
      cache_dir_trap: "/etc/snmp/centreon_traps"
      remote_dir: "/var/cache/centreon/config/remote-data/"
EOF

echo "✓ Central config.d written (30/31/40 + cron.d)"
echo ""
