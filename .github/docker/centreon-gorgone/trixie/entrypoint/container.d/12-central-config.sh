#!/bin/sh
# TYPE=central: write config.d/{30-centreon,31-centreon-api,40-gorgoned,
# 41-autodiscovery}.yaml directly, the same standard
# config.yaml -> !include config.d/*.yaml path
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

# Real production splits DB connectivity into 30-centreon.yaml, which does
# `centreon: !include /etc/centreon/config.d/*.yaml` — a directory centreon-
# web's own PHP installer writes at install time, out of scope for this image
# to replicate. DSN/credentials are injected via GORGONE__CENTREON__DATABASE__*
# env vars instead (confirmed safe, works even with no dsn: line present at
# all) — the placeholders below are only ever seen if those aren't set.
#
# KNOWN GAP, not yet made fail-fast (coordination needed before tightening):
# these placeholders are a real, literal fallback DB password. A deployment
# that only overrides the DSN (not USERNAME/PASSWORD) silently keeps using
# "centreon"/"centreon" — works by coincidence if the real DB password
# happens to match that string, fails with a DB auth error in gorgone's own
# logs otherwise. Confirmed both ways empirically (k8s Helm chart hit this on
# a real non-matching password; centreon-web-split's own compose currently
# only overrides DB_CONFIGURATION's DSN and doesn't override DB_REALTIME at
# all, so it would break if this became a hard requirement today). Warning
# below is the non-breaking interim step; removing the fallback entirely
# requires updating every consumer's compose/chart first.
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
# `audit`. `autodiscovery` IS enabled (see 41-autodiscovery.yaml above),
# giving cron.d/41-service-discovery.yaml's LAUNCHSERVICEDISCOVERY job a
# real handler.
#
# allowed_hosts.subnets below defaults to the Docker Compose bridge range
# used by this repo's own e2e fixtures — it is NOT a sane default for every
# topology. gorgone's own GORGONE__ env var override mechanism can't reach
# this field (nested array inside a named module, confirmed unhandled by
# gorgone::class::script::load_env_config), so it's made configurable here
# instead via GORGONE_HTTPSERVER_ALLOWED_SUBNETS (space-separated CIDRs).
# Kubernetes: pod CIDR varies by CNI/cluster and allowing the whole pod CIDR
# is equivalent to allowing every pod in the cluster, so don't set this var —
# instead disable the allowlist entirely with the existing, already-working
# GORGONE__GORGONE__MODULES__HTTPSERVER__ALLOWED_HOSTS__ENABLED=false (a
# plain scalar, unlike subnets) and restrict access with a NetworkPolicy
# instead (identity-based, confirmed working end-to-end by the k8s Helm
# chart: port 8085 from fpm/config-delivery pods only, 8087/pullwss from
# httpd only).
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

echo "✓ Central config.d written (30/31/40/41 + cron.d)"
echo ""
