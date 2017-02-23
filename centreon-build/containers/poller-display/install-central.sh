#!/bin/sh

set -e
set -x

# Start services.
service mysql start
httpd -k start

# Install Centreon Web module.
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-poller-display-central

# Create and populate poller (that will have Poller Display).
centreon -u admin -p centreon -o INSTANCE -a ADD -v "Poller;poller;22;"
centreon -u admin -p centreon -o HOST -a ADD -v "Poller;Poller;poller;generic-host;Poller;"
centreon -u admin -p centreon -o SERVICE -a ADD -v "Poller;Ping;Ping-LAN"

# Stop services.
httpd -k stop
service mysql stop
