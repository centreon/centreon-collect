#!/bin/sh
# Entrypoint of the rust-poller bench container: registers the poller in
# centreon-web (register.pl, as the engine containers do), creates its
# objects through CLAPI, starts gorgone-agent with its built-in scheduler,
# then asks web, once, to generate, push and restart the configuration.
set -eu

GORGONE_LIB=/usr/local/lib/gorgone
GORGONE_STATE=/var/lib/centreon-gorgone/agent
export POLLER_NAME="${POLLER_NAME:-rust-poller}"

if [ -z "${GORGONE_AGENT__NODE_ID:-}" ]; then
  if ! GORGONE_AGENT__NODE_ID=$(perl "${GORGONE_LIB}/register.pl" register); then
    echo "rust-poller: poller registration failed" >&2
    exit 1
  fi
  export GORGONE_AGENT__NODE_ID
fi

if [ ! -f "${GORGONE_STATE}/objects" ]; then
  CLAPI_OBJECTS="${GORGONE_LIB}/objects.clapi" perl "${GORGONE_LIB}/register.pl" objects
  touch "${GORGONE_STATE}/objects"
fi

echo "Starting gorgone-agent with its scheduler for node ${GORGONE_AGENT__NODE_ID}..."
/usr/bin/gorgone-agent --config /etc/centreon-gorgone/agent.toml &
agent=$!
trap 'kill -TERM "${agent}" 2>/dev/null' TERM INT

# The container filesystem survives a restart: deploy only on the first start.
if [ "${GORGONE_DEPLOY:-false}" = "true" ] && [ ! -f "${GORGONE_STATE}/deployed" ]; then
  DEPLOY_MARKER="${GORGONE_STATE}/deployed" perl "${GORGONE_LIB}/register.pl" deploy
fi

# Twice: a signal interrupts the first wait.
wait "${agent}" || wait "${agent}"
