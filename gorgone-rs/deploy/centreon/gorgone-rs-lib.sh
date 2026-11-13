#!/bin/sh
# Shared by the web and poller test images: writes the agent configuration
# and starts the Rust binaries under the centreon-gorgone user.

GORGONE_RS_ETC=/etc/centreon-gorgone/rs
GORGONE_RS_LOG=/var/log/centreon-gorgone

# write_agent_config <node id> <server url> <broker reload command>
write_agent_config() {
  install -d -o centreon-gorgone -g centreon-gorgone "$GORGONE_RS_ETC" /var/lib/centreon-gorgone/agent
  cat > "$GORGONE_RS_ETC/agent.toml" <<EOF
node_id = "$1"
server_url = "$2"
# Test value: the server of this test bench enrols nodes automatically.
token = "integration-test"
state_dir = "/var/lib/centreon-gorgone/agent"

[commands]
allowlist = []

[engine]
command_file = "/var/lib/centreon-engine/rw/centengine.cmd"
allowed_commands = [
  'SCHEDULE_(FORCED_)?(HOST|SVC)_CHECK',
  'SCHEDULE_(FORCED_)?HOST_SVC_CHECKS',
  'ACKNOWLEDGE_(HOST|SVC)_PROBLEM',
  'REMOVE_(HOST|SVC)_ACKNOWLEDGEMENT',
  'SCHEDULE_(HOST|SVC)_DOWNTIME',
  'SCHEDULE_HOST_SVC_DOWNTIME',
  'DEL_(HOST|SVC)_DOWNTIME',
  'ADD_(HOST|SVC)_COMMENT',
  'DEL_(HOST|SVC)_COMMENT',
  'PROCESS_(HOST|SERVICE)_CHECK_RESULT',
  '(ENABLE|DISABLE)_(HOST|SVC)_NOTIFICATIONS',
  '(ENABLE|DISABLE)_(HOST|SVC)_CHECK',
]

[targets.engine]
path = "/etc/centreon-engine"
prune = false

[targets.broker]
path = "/etc/centreon-broker"
prune = false

[targets.vmware]
path = "/etc/centreon"
prune = false

[services.engine]
start = "sudo systemctl start centengine"
stop = "sudo systemctl stop centengine"
reload = "sudo systemctl reload centengine"
restart = "sudo systemctl restart centengine"

[services.broker]
reload = "$3"
EOF
  chown centreon-gorgone: "$GORGONE_RS_ETC/agent.toml"
}

# start_as_gorgone <binary> <config file>: runs in the background.
start_as_gorgone() {
  install -d -o centreon-gorgone -g centreon-gorgone "$GORGONE_RS_LOG"
  runuser -u centreon-gorgone -- "$1" --config "$2" \
    >> "$GORGONE_RS_LOG/$(basename "$1").log" 2>&1 &
}
