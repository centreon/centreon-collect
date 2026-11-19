#!/bin/sh
# Entrypoint of the centreon-engine bench container: registers the poller in
# centreon-web (register.pl, as the engine containers do), creates its
# objects through CLAPI and marks the poller as run by centreon-engine,
# starts centreon-engine, then asks web, once, to generate, push and
# restart the configuration.
set -eu

GORGONE_LIB=/usr/local/lib/gorgone
ENGINE_STATE=/var/lib/centreon-engine
export POLLER_NAME="${POLLER_NAME:-centreon-engine}"

if [ -z "${CENTREON_ENGINE__NODE_ID:-}" ]; then
  if ! CENTREON_ENGINE__NODE_ID=$(perl "${GORGONE_LIB}/register.pl" register); then
    echo "centreon-engine: poller registration failed" >&2
    exit 1
  fi
  export CENTREON_ENGINE__NODE_ID
fi

if [ ! -f "${ENGINE_STATE}/objects" ]; then
  CLAPI_OBJECTS="${GORGONE_LIB}/objects.clapi" perl "${GORGONE_LIB}/register.pl" objects
  touch "${ENGINE_STATE}/objects"
fi

echo "Starting centreon-engine for node ${CENTREON_ENGINE__NODE_ID}..."
/usr/bin/centreon-engine &
engine=$!
trap 'kill -TERM "${engine}" 2>/dev/null' TERM INT

# The container filesystem survives a restart: deploy only on the first start.
if [ "${GORGONE_DEPLOY:-false}" = "true" ] && [ ! -f "${ENGINE_STATE}/deployed" ]; then
  DEPLOY_MARKER="${ENGINE_STATE}/deployed" perl "${GORGONE_LIB}/register.pl" deploy
fi

# Twice: a signal interrupts the first wait.
wait "${engine}" || wait "${engine}"
