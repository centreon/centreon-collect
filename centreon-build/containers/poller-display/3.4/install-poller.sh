#!/bin/sh

set -e
set -x

# Start services.
service mysql start
httpd -k start

# Install Centreon Web module.
/tmp/install-centreon-module.php -c /etc/centreon/centreon.conf.php -m centreon-poller-display

# Stop services.
httpd -k stop
service mysql stop
