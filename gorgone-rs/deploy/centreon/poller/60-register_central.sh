#!/bin/sh
# Same scenario as the stock poller image (register the poller, wait for the
# central to see it, then generate, push and restart), with gorgone-agent in
# place of the Perl gorgoned: no thumbprint exchange, the agent dials in.

. /usr/share/centreon/gorgone-rs-lib.sh

WEB_HOST=${WEB_HOST:-web}
GORGONE_HOST=${GORGONE_HOST:-gorgone-server}
GORGONE_API_PORT=${GORGONE_API_PORT:-8085}
GORGONE_AGENT_PORT=${GORGONE_AGENT_PORT:-5556}
WEB_API_USERNAME=${WEB_API_USERNAME:-admin}
WEB_API_PASSWORD=${WEB_API_PASSWORD:-Centreon\!2021}

json() { python3 -c "import json,sys; d=json.load(sys.stdin); print($1)" 2>/dev/null; }

if getent hosts "${WEB_HOST}"; then
  : "${HOSTNAME:=$(hostname)}"

  API_TOKEN=$(curl -s -X POST -H "Content-Type: application/json" \
      -d "{\"security\":{\"credentials\":{\"login\":\"${WEB_API_USERNAME}\", \"password\":\"${WEB_API_PASSWORD}\"}}}" \
      "http://${WEB_HOST}/centreon/api/latest/login" | json 'd["security"]["token"]')

  curl -s -X POST -H "Content-Type: application/json" -H "X-AUTH-TOKEN: ${API_TOKEN}" \
      -d '{"name":"'"$HOSTNAME"'","hostname":"'"$HOSTNAME"'","type":"poller","address":"'"$HOSTNAME"'","parent_address":"127.0.0.1"}' \
      "http://${WEB_HOST}/centreon/api/latest/platform/topology"

  # Where the poller sends its events: the central broker.
  CENTRAL_IP_ADDRESS=${BROKER_HOST:-${WEB_HOST}}
  curl -s -X POST -H "Content-Type: application/json" -H "centreon-auth-token: ${API_TOKEN}" \
      -d "{\"linked_remote_master\":\"\",\"linked_remote_slaves\":[],\"open_broker_flow\":false,\"centreon_central_ip\":\"${CENTRAL_IP_ADDRESS}\",\"server_ip\":\"$HOSTNAME\",\"server_name\":\"$HOSTNAME\",\"server_type\":\"poller\"}" \
      "http://${WEB_HOST}/centreon/api/index.php?object=centreon_configuration_remote&action=linkCentreonRemoteServer"

  # The node id is the id centreon-web gave to the poller.
  POLLER_ID=$(curl -s -G -H "X-AUTH-TOKEN: ${API_TOKEN}" \
      --data-urlencode "search={\"name\":\"$HOSTNAME\"}" \
      "http://${WEB_HOST}/centreon/api/latest/configuration/monitoring-servers" | json 'd["result"][0]["id"]')
  if [ -z "$POLLER_ID" ]; then
    echo "Cannot find the id of poller $HOSTNAME"
    exit 1
  fi
  echo "Poller $HOSTNAME has id $POLLER_ID"

  # A poller has no cbd: its broker module is reloaded with the engine.
  write_agent_config "$POLLER_ID" "http://${GORGONE_HOST}:${GORGONE_AGENT_PORT}" \
    "sudo systemctl reload centengine"
  start_as_gorgone /usr/bin/gorgone-agent "$GORGONE_RS_ETC/agent.toml"

  retry_count=0
  until [ "$CONNECTED" = "True" ] || [ $retry_count -ge 120 ]; do
    sleep 1
    CONNECTED=$(curl -s -u "${GORGONE_API_USERNAME}:${GORGONE_API_PASSWORD}" \
        "http://${GORGONE_HOST}:${GORGONE_API_PORT}/v1/nodes" \
        | json "[n['connected'] for n in d['nodes'] if n['id'] == '$POLLER_ID'][0]")
    retry_count=$((retry_count + 1))
  done
  if [ "$CONNECTED" != "True" ]; then
    echo "gorgone-server never saw the agent of poller $POLLER_ID"
    exit 1
  fi

  for action in POLLERGENERATE CFGMOVE POLLERRESTART; do
    curl -s -X POST -H "Content-Type: application/json" -H "centreon-auth-token: ${API_TOKEN}" \
        -d "{\"action\":\"$action\",\"values\":\"$HOSTNAME\"}" \
        "http://${WEB_HOST}/centreon/api/index.php?action=action&object=centreon_clapi"
    echo
  done
fi
