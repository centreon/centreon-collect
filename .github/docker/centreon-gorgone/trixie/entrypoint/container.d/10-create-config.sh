#!/bin/sh
# TYPE=poller: static config.d/40-gorgoned.yaml, params via load_env_config
# env vars (GORGONE_UID, GORGONE_TOKEN, CENTRAL_HOST, CENTRAL_PORT) — nothing
# to do here.
# TYPE=central: writes config.d/{30-centreon,31-centreon-api,40-gorgoned,
# 41-autodiscovery}.yaml at startup, same !include config.d/*.yaml path as
# TYPE=poller. tpapi (31-centreon-api.yaml) can't use a GORGONE__ override:
# load_env_config crashes on GORGONE__GORGONE__TPAPI__* (only `modules` gets
# the array-search-by-name handling tpapi would need).

set -e

if [ "$TYPE" != "central" ]; then
    echo "=== Central config: not central mode, skipping ==="
    return 0
fi

echo "=== Writing central config.d ==="

CONFIGD="/etc/centreon-gorgone/config.d"
mkdir -p "$CONFIGD/cron.d"

# LAUNCHSERVICEDISCOVERY handler: see `autodiscovery` below. Helios stays
# `.disabled` (default-off in production).
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

cat > "$CONFIGD/41-autodiscovery.yaml" <<'EOF'
gorgone:
  modules:
    - name: autodiscovery
      package: "gorgone::modules::centreon::autodiscovery::hooks"
      enable: true
      no_shell_interpretation: true
EOF

# set +x: DEBUG=true must not trace the password assignments below.
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
# Skip the fail-fast if GORGONE__ already overrides the password directly.
if [ -z "${GORGONE__CENTREON__DATABASE__DB_CONFIGURATION__PASSWORD}${GORGONE__CENTREON__DATABASE__DB_REALTIME__PASSWORD}" ] \
   && [ -z "$MYSQL_PASSWORD" ]; then
    echo "ERROR: MYSQL_PASSWORD must be set for central mode (or set GORGONE__CENTREON__DATABASE__DB_CONFIGURATION__PASSWORD / DB_REALTIME__PASSWORD directly)" >&2
    exit 1
fi
# YAML single-quoted scalar: only `'` is special (doubled to `''`); no type
# coercion, so true/null/123 stay strings. Doesn't survive a literal newline.
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

# GORGONE__CENTREON__DATABASE__* still overrides any field below after load.
# Database TLS: DBD::mysql cannot enforce TLS on Debian, override both DSN with the MariaDB driver, e.g.
# GORGONE__CENTREON__DATABASE__DB_CONFIGURATION__DSN='MariaDB:host=db;dbname=centreon;mariadb_ssl=1;mariadb_ssl_ca_file=/etc/ssl/db-ca.pem;mariadb_ssl_verify_server_cert=1'
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

# gorgone's own login to the Centreon API, used to validate OTHER callers'
# tokens (gorgone/modules/core/proxy/httpserver.pm:502-524) — not the
# poller's own token.
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

# Deviations from a normal poller config:
#   - gorgonecore.id 1000, not 1: matching Central's real nagios_server id
#     would hit core.pm:649's self-nulling shortcut on REMOTECOPY/COMMAND.
#   - proxy.httpserver binds 0.0.0.0: httpd reaches this over the network.
#   - no `register` module: `nodes`' DB sync registers id 1 like any poller.
# Not yet included: `engine`, `statistics`, `audit`.
#
# allowed_hosts.subnets defaults to this repo's own e2e bridge range — not
# universal. GORGONE__ can't override a nested array, so it's configurable
# via GORGONE_HTTPSERVER_ALLOWED_SUBNETS instead. Where a fixed CIDR doesn't
# fit, disable the allowlist (GORGONE__GORGONE__MODULES__HTTPSERVER__ALLOWED_HOSTS__ENABLED=false)
# and restrict access at the network layer instead.
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
EOF
# `action` is NOT declared here: 39-action.yaml (baked unconditionally in the
# Dockerfile) already provides it — declaring it again here duplicate-loads
# the module and logs "Package ... already loaded".

echo "✓ Central config.d written (30/31/40/41 + cron.d)"
echo ""
