#!/bin/sh
# The central is also a poller (node 1): its engine is driven by a
# gorgone-agent connected to the gorgone-server container.

. /usr/share/centreon/gorgone-rs-lib.sh

write_agent_config 1 "http://${GORGONE_API_ADDRESS}:${GORGONE_AGENT_PORT:-5556}" \
  "sudo systemctl reload cbd"
echo "Starting gorgone-agent for the central poller..."
start_as_gorgone /usr/bin/gorgone-agent "$GORGONE_RS_ETC/agent.toml"
