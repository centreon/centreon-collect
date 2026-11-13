#!/bin/sh
# Replaces the script starting the Perl gorgoned: starts gorgone-server, and a
# gorgone-agent for the central poller itself (node 1).

. /usr/share/centreon/gorgone-rs-lib.sh

install -d -o centreon-gorgone -g centreon-gorgone "$GORGONE_RS_ETC" /var/lib/centreon-gorgone
cat > "$GORGONE_RS_ETC/server.toml" <<EOF
# Test bench settings: plaintext, no credentials, automatic enrolment.
# The REST API is reachable from the container networks only, like the Perl
# daemon of the stock image.
rest_listen = "0.0.0.0:8085"
agent_listen = "0.0.0.0:5556"
peer_listen = "127.0.0.1:5557"
allow_plaintext = true
insecure_no_auth = true
auto_enroll = true
database_url = "sqlite:///var/lib/centreon-gorgone/gorgone-rs.db?mode=rwc"

[rest]
allowed_cidrs = ["127.0.0.0/8", "10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16"]

[centcore]
enabled = true
poll_interval_ms = 1000
# Writers of the command files: php-fpm, CLAPI and cron jobs.
allowed_uids = [0, $(id -u apache), $(id -u centreon)]
EOF
chown centreon-gorgone: "$GORGONE_RS_ETC/server.toml"

echo "Starting gorgone-server..."
start_as_gorgone /usr/bin/gorgone-server "$GORGONE_RS_ETC/server.toml"

write_agent_config 1 "http://127.0.0.1:5556"
echo "Starting gorgone-agent for the central poller..."
start_as_gorgone /usr/bin/gorgone-agent "$GORGONE_RS_ETC/agent.toml"
echo "Gorgone (Rust) started."
