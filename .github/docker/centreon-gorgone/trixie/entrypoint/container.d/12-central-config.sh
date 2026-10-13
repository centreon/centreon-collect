#!/bin/sh
# TYPE=central: writes config.d/{30-centreon,31-centreon-api,40-gorgoned,
# 41-autodiscovery}.yaml, loaded via the standard
# config.yaml -> !include config.d/*.yaml path (same as TYPE=poller) — no
# compose `command:` override needed, and 39-action.yaml/whitelist.conf.d
# (baked unconditionally earlier in the Dockerfile) apply here for free.
#
# Heredocs are unquoted (<<EOF) so env vars expand into the YAML directly.
# tpapi (31-centreon-api.yaml) must stay a heredoc rather than a GORGONE__
# env override: gorgone::class::script::load_env_config crashes ("Not a HASH
# reference") on GORGONE__GORGONE__TPAPI__* — tpapi is a named list like
# `modules`, but only `modules` gets the array-search-by-name handling.

set -e

if [ "$TYPE" != "central" ]; then
    echo "=== Central config: not central mode, skipping ==="
    return 0
fi

echo "=== Writing central config.d ==="

CONFIGD="/etc/centreon-gorgone/config.d"
mkdir -p "$CONFIGD/cron.d"

# LAUNCHSERVICEDISCOVERY needs the `autodiscovery` module (enabled below) —
# harmless to schedule either way. Helios keeps its `.disabled` suffix, which
# is what excludes it from the `cron.d/*.yaml` glob, matching its default-off
# state in production.
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

# Ships with centreon-auto-discovery-server (web-side package, not installed
# here) - literal content, written directly like cron.d above.
cat > "$CONFIGD/41-autodiscovery.yaml" <<'EOF'
gorgone:
  modules:
    - name: autodiscovery
      package: "gorgone::modules::centreon::autodiscovery::hooks"
      enable: true
      no_shell_interpretation: true
EOF

# DEBUG=true sets -x on the whole entrypoint; without this guard the plain
# assignment below traces the password itself straight into `docker logs`
# (the heredoc further down that also embeds it is not affected - set -x
# traces the `cat` command line, never a heredoc's body).
central_config_debug_was_on=0
if [ "${DEBUG}" = "true" ] || [ "${DEBUG}" = "1" ]; then
    central_config_debug_was_on=1
fi
set +x
CENTREON_API_BASE_URL="${CENTREON_API_BASE_URL:?ERROR: CENTREON_API_BASE_URL must be set for central mode}"
CENTREON_API_USERNAME="${CENTREON_API_USERNAME:?ERROR: CENTREON_API_USERNAME must be set for central mode}"
CENTREON_API_PASSWORD="${CENTREON_API_PASSWORD:?ERROR: CENTREON_API_PASSWORD must be set for central mode}"
central_config_db_creds_overridden=1
if [ -z "${GORGONE__CENTREON__DATABASE__DB_CONFIGURATION__PASSWORD}${GORGONE__CENTREON__DATABASE__DB_REALTIME__PASSWORD}" ]; then
    central_config_db_creds_overridden=0
fi
if [ "$central_config_debug_was_on" = "1" ]; then
    set -x
fi

# Real production reads DB config from /etc/centreon/config.d/*.yaml,
# written by the PHP installer at install time — out of scope here.
# DSN/credentials are injected via GORGONE__CENTREON__DATABASE__* env vars
# instead; the placeholders below are only ever seen if those aren't set.
#
# KNOWN GAP, not yet made fail-fast: these placeholders are a real fallback
# DB password. A consumer that only overrides the DSN (not USERNAME/
# PASSWORD) silently keeps using "centreon"/"centreon" — works only if that
# happens to be the real DB password, fails with a DB auth error otherwise.
# Warning below is the non-breaking interim step; removing the fallback
# requires updating every consumer first.
if [ "$central_config_db_creds_overridden" = "0" ]; then
    echo "WARNING: TYPE=central is using the default 'centreon'/'centreon' DB credentials — set GORGONE__CENTREON__DATABASE__DB_CONFIGURATION__{USERNAME,PASSWORD} and DB_REALTIME__{USERNAME,PASSWORD} to override" >&2
fi
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

# gorgone's own login to the Centreon API (a real web user, "centreon-
# gorgone" in production), used to validate OTHER callers' tokens
# (gorgone/modules/core/proxy/httpserver.pm:502-524) — not the poller's own
# token. Real production splits this into an external and a local tpapi
# entry; not replicated here — nothing is 127.0.0.1-reachable between
# gorgone-central and httpd in a split-container topology, and only the
# last-loaded config.d file's tpapi array survives Hash::Merge's
# RIGHT_PRECEDENT anyway. One real, working base_url instead.
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

# Module set for a production central, with three deliberate deviations:
#   - gorgonecore.id 1000, not 1 (Central's real nagios_server id, matching
#     the poller-container's own GORGONE_UID): matching that id would hit
#     core.pm:649's self-target-nulling shortcut and null REMOTECOPY/COMMAND
#     dispatch to poller id 1.
#   - proxy.httpserver binds 0.0.0.0, not localhost: httpd is a separate
#     container reaching this one by service name over the network
#     (GORGONE_WSS_UPSTREAM=gorgone-central:8087 on the httpd side).
#   - no `register` module: `nodes`' own DB sync registers id 1 like any
#     other poller once nagios_server.localhost is 0 for Central's row.
# Not yet included: `engine` (gorgone-central-poller shares
# /var/lib/centreon-engine/rw with centengine instead, so RELOAD/RESTART and
# ENGINECOMMAND go through that process today), `statistics`, `audit`.
#
# allowed_hosts.subnets below defaults to the Docker Compose bridge range
# used by this repo's own e2e fixtures — not a sane default everywhere.
# gorgone's own GORGONE__ env override can't reach this field (nested array
# inside a named module, unhandled by gorgone::class::script::load_env_config),
# so it's made configurable here instead via GORGONE_HTTPSERVER_ALLOWED_SUBNETS
# (space-separated CIDRs). Where a fixed CIDR doesn't make sense for the
# deployment, disable the allowlist entirely instead
# (GORGONE__GORGONE__MODULES__HTTPSERVER__ALLOWED_HOSTS__ENABLED=false, a
# plain scalar unlike subnets) and restrict access at the network layer.
GORGONE_HTTPSERVER_ALLOWED_SUBNETS="${GORGONE_HTTPSERVER_ALLOWED_SUBNETS:-127.0.0.1/32 172.16.0.0/12}"
central_config_allowed_subnets_yaml=""
for central_config_subnet in $GORGONE_HTTPSERVER_ALLOWED_SUBNETS; do
    central_config_allowed_subnets_yaml="${central_config_allowed_subnets_yaml}
          - ${central_config_subnet}"
done
cat > "$CONFIGD/40-gorgoned.yaml" <<EOF
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
        subnets:${central_config_allowed_subnets_yaml}

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

    # Registers the /core/action/command route on this process's own
    # httpserver — required for RELOAD/RESTART (restartPollers.php) dispatch
    # to work at all: without it, POST /api/nodes/<id>/core/action/command
    # returns {"error":"method_unknown"} regardless of what the target node
    # supports. 39-action.yaml/whitelist.conf.d are baked unconditionally
    # earlier in the Dockerfile.
    - name: action
      package: "gorgone::modules::core::action::hooks"
      enable: true
      command_timeout: 30
      whitelist_cmds: true
      allowed_cmds: !include /etc/centreon-gorgone/config.d/whitelist.conf.d/*.yaml
EOF

echo "✓ Central config.d written (30/31/40/41 + cron.d)"
echo ""
