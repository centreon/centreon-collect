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

    POLLER_ID="${POLLER_ID:-2}"
    CENTRAL_ADDRESS="${CENTRAL_ADDRESS:?ERROR: CENTRAL_ADDRESS env var must be set for poller mode}"
    CENTRAL_PORT="${CENTRAL_PORT:-8086}"
    GORGONE_TOKEN="${GORGONE_TOKEN:-}"

    echo "Poller ID      : $POLLER_ID"
    echo "Central address: $CENTRAL_ADDRESS:$CENTRAL_PORT"

    cat <<EOF > /etc/centreon-gorgone/config.d/40-gorgoned.yaml
name: poller-${POLLER_ID}
description: Poller configuration
gorgone:
  gorgonecore:
    id: ${POLLER_ID}
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
      token: ${GORGONE_TOKEN}
      address: ${CENTRAL_ADDRESS}

EOF
    chmod 775 /etc/centreon-gorgone/config.d/40-gorgoned.yaml
    rm -f /etc/centreon-gorgone/config.d/31-centreon-api.yaml

    echo "✓ Successfully created /etc/centreon-gorgone/config.d/40-gorgoned.yaml"
    echo ""
fi

echo "✓ Gorgone configuration generation complete"
echo ""
