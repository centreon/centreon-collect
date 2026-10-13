#!/bin/sh
# TYPE=poller: config is static in the image (config.d/40-gorgoned.yaml).
# Dynamic params are injected at startup via env vars (GORGONE_UID,
# GORGONE_TOKEN, CENTRAL_HOST, CENTRAL_PORT) using gorgone's own
# load_env_config mechanism — nothing to generate here.
#
# TYPE=central: writes config.d/{30-centreon,31-centreon-api,40-gorgoned,
# 41-autodiscovery}.yaml, loaded via the same standard
# config.yaml -> !include config.d/*.yaml path (same as TYPE=poller) — no
# compose `command:` override needed, and 39-action.yaml/whitelist.conf.d
# (baked unconditionally earlier in the Dockerfile) apply here for free.
#
# Heredocs are unquoted (<<EOF) so env vars expand into the YAML directly.
# tpapi (31-centreon-api.yaml) must stay a heredoc rather than a GORGONE__
# env override: gorgone::class::script::load_env_config crashes ("Not a HASH
# reference") on GORGONE__GORGONE__TPAPI__* — tpapi is a named list like
# `modules`, but only `modules` gets the array-search-by-name handling.
# 30-centreon.yaml's DB values are pre-escaped for a single-quoted YAML
# scalar (see central_config_yaml_squote below) rather than interpolated
# raw: a DB password can contain quotes/colons/hashes that would otherwise
# break the YAML or get misparsed.

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
MYSQL_HOST="${MYSQL_HOST:-}"
MYSQL_DB_CONFIGURATION="${MYSQL_DB_CONFIGURATION:-centreon}"
MYSQL_DB_STORAGE="${MYSQL_DB_STORAGE:-centreon_storage}"
MYSQL_USER="${MYSQL_USER:-centreon}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-}"
# Fail fast unless a consumer already supplies the password directly via the
# GORGONE__ override mechanism (so a consumer that only ever sets
# GORGONE__CENTREON__DATABASE__DB_{CONFIGURATION,REALTIME}__PASSWORD keeps
# working unchanged).
if [ -z "${GORGONE__CENTREON__DATABASE__DB_CONFIGURATION__PASSWORD}${GORGONE__CENTREON__DATABASE__DB_REALTIME__PASSWORD}" ] \
   && [ -z "$MYSQL_PASSWORD" ]; then
    echo "ERROR: MYSQL_PASSWORD must be set for central mode (or set GORGONE__CENTREON__DATABASE__DB_CONFIGURATION__PASSWORD / DB_REALTIME__PASSWORD directly)" >&2
    exit 1
fi
# A single-quoted YAML scalar only treats one character specially: a literal
# `'` must be doubled to `''`. Everything else (`:`, `#`, `"`, digits,
# true/null/yes...) is taken as-is and stays a string, no type coercion —
# unlike an unquoted/plain scalar. The one thing this can't represent is a
# literal newline inside the value (YAML folds it); not a realistic concern
# for a DB password.
central_config_yaml_squote() {
    printf '%s' "$1" | sed "s/'/''/g"
}
central_config_mysql_host_yaml=$(central_config_yaml_squote "$MYSQL_HOST")
central_config_mysql_db_configuration_yaml=$(central_config_yaml_squote "$MYSQL_DB_CONFIGURATION")
central_config_mysql_db_storage_yaml=$(central_config_yaml_squote "$MYSQL_DB_STORAGE")
central_config_mysql_user_yaml=$(central_config_yaml_squote "$MYSQL_USER")
central_config_mysql_password_yaml=$(central_config_yaml_squote "$MYSQL_PASSWORD")
if [ "$central_config_debug_was_on" = "1" ]; then
    set -x
fi

# Real production reads DB config from /etc/centreon/config.d/*.yaml,
# written by the PHP installer at install time — out of scope here.
# GORGONE__CENTREON__DATABASE__* env vars still override any field below
# after load (gorgone's own mechanism, applied regardless of how this file
# was generated).
cat > "$CONFIGD/30-centreon.yaml" <<EOF
name: 30-centreon.yaml
centreon:
  database:
    db_configuration:
      dsn: 'mysql:host=${central_config_mysql_host_yaml};dbname=${central_config_mysql_db_configuration_yaml}'
      username: '$central_config_mysql_user_yaml'
      password: '$central_config_mysql_password_yaml'
    db_realtime:
      dsn: 'mysql:host=${central_config_mysql_host_yaml};dbname=${central_config_mysql_db_storage_yaml}'
      username: '$central_config_mysql_user_yaml'
      password: '$central_config_mysql_password_yaml'
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
