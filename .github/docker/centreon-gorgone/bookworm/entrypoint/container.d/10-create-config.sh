#!/bin/sh
set -e
TYPE="${TYPE:-central}"

if [ "$TYPE" = "central" ]; then
    echo "=== Generating Gorgone Configuration for Central (placeholder) ==="
    echo ""

    # TODO(MON-196928): Central Gorgone modules to be configured in a follow-up ticket.
    # For now, only a minimal placeholder is written so TYPE=central dispatch remains functional.
    cat <<EOF > /etc/centreon-gorgone/config.d/40-gorgoned.yaml
name: gorgoned-central
description: Configuration for centreon-gorgone central (placeholder)
gorgone:
  gorgonecore:
    id: 1
  modules: []
# TODO(MON-196928): Central Gorgone modules to be configured in a follow-up ticket
EOF

    chmod 775 /etc/centreon-gorgone/config.d/40-gorgoned.yaml
    echo "✓ Successfully created /etc/centreon-gorgone/config.d/40-gorgoned.yaml (placeholder)"
    echo ""

elif [ "$TYPE" = "poller" ]; then
    echo "=== Generating Gorgone Configuration for Poller ==="
    echo ""

    UUID="${UUID:?ERROR: UUID env var must be set for poller mode}"
    NAME="${NAME:?ERROR: NAME env var must be set for poller mode}"
    POLLER_TOKEN="${POLLER_TOKEN:?ERROR: POLLER_TOKEN env var must be set for poller mode}"
    CENTRAL_URL="${CENTRAL_URL:?ERROR: CENTRAL_URL env var must be set for poller mode}"

    # Strip optional scheme (http://, https://) and trailing path, then split host/port.
    CENTRAL_HOSTPORT=$(echo "$CENTRAL_URL" | sed -e 's#^[a-zA-Z]\+://##' -e 's#/.*$##')
    CENTRAL_HOST=$(echo "$CENTRAL_HOSTPORT" | cut -d: -f1)
    CENTRAL_PORT=$(echo "$CENTRAL_HOSTPORT" | cut -s -d: -f2)
    CENTRAL_PORT="${CENTRAL_PORT:-8086}"

    echo "Poller UUID    : $UUID"
    echo "Poller name    : $NAME"
    echo "Central address: $CENTRAL_HOST:$CENTRAL_PORT"

    cat <<EOF > /etc/centreon-gorgone/config.d/40-gorgoned.yaml
name: ${NAME}
description: Poller configuration
gorgone:
  gorgonecore:
    id: ${UUID}
    privkey: /var/lib/centreon-gorgone/.keys/rsakey.priv.pem
    pubkey: /var/lib/centreon-gorgone/.keys/rsakey.pub.pem

  modules:
    - name: engine
      package: gorgone::modules::centreon::engine::hooks
      enable: true
      command_file: "/var/lib/centreon-engine/rw/centengine.cmd"

    - name: pullwss
      package: "gorgone::modules::core::pullwss::hooks"
      enable: true
      ssl: false
      port: ${CENTRAL_PORT}
      token: ${POLLER_TOKEN}
      address: ${CENTRAL_HOST}

EOF
    chmod 775 /etc/centreon-gorgone/config.d/40-gorgoned.yaml
    rm -f /etc/centreon-gorgone/config.d/31-centreon-api.yaml

    echo "✓ Successfully created /etc/centreon-gorgone/config.d/40-gorgoned.yaml"
    echo ""
fi

echo "✓ Gorgone configuration generation complete"
echo ""
