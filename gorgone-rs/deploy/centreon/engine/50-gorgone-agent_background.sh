#!/bin/sh
# Runs gorgone-agent next to centengine in the same container. Without
# GORGONE_AGENT__NODE_ID the poller registers itself in centreon-web first and
# takes the id web gave it. With GORGONE_DEPLOY=true it then asks web, once,
# to generate, push and restart its configuration. Both steps remove the
# external command file from the Engine configuration of the poller in web
# (register.pl): this engine is driven through its gRPC API only.

GORGONE_LIB=/usr/local/lib/gorgone
GORGONE_STATE=/var/lib/centreon-gorgone/agent

if [ -z "${GORGONE_AGENT__NODE_ID:-}" ]; then
  if ! GORGONE_AGENT__NODE_ID=$(perl "${GORGONE_LIB}/register.pl" register); then
    echo "gorgone-agent: poller registration failed"
    exit 1
  fi
  export GORGONE_AGENT__NODE_ID
fi

echo "Starting gorgone-agent for node ${GORGONE_AGENT__NODE_ID}..."
/usr/bin/gorgone-agent --config "${GORGONE_LIB}/agent.toml" &

# The container filesystem survives a restart: deploy only on the first start.
if [ "${GORGONE_DEPLOY:-false}" = "true" ] && [ ! -f "${GORGONE_STATE}/deployed" ]; then
  DEPLOY_MARKER="${GORGONE_STATE}/deployed" perl "${GORGONE_LIB}/register.pl" deploy
fi

wait
